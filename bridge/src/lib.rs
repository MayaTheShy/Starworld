// Rust C-ABI bridge for StardustXR client integration.

mod model_downloader;

use std::collections::HashMap;
use std::ffi::CStr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex, OnceLock};
use std::thread::JoinHandle;

use glam::Mat4;
use stardust_xr_asteroids as ast; // alias for brevity
use stardust_xr_asteroids::{
    client::ClientState,
    elements::{PlaySpace, Spatial, Model},
    Migrate, Reify, CustomElement, Projector, Context,
};
use stardust_xr_molecules::accent_color::AccentColor;
use stardust_xr_fusion::objects::connect_client as fusion_connect_client;
use stardust_xr_fusion::node::NodeType;
use stardust_xr_fusion::root::RootAspect;
use tokio::runtime::Runtime;
use std::path::PathBuf;
use model_downloader::ModelDownloader;

// Global model downloader instance
static MODEL_DOWNLOADER: OnceLock<ModelDownloader> = OnceLock::new();

#[derive(Clone, serde::Serialize, serde::Deserialize)]
struct BridgeState {
    nodes: HashMap<u64, Node>,
}

impl Default for BridgeState {
    fn default() -> Self {
        Self { nodes: HashMap::new() }
    }
}

enum Command {
    Create { c_id: u64, name: String, transform: Mat4 },
    Update { c_id: u64, transform: Mat4 },
    SetModel { c_id: u64, model_url: String },
    SetTexture { c_id: u64, texture_url: String },
    SetColor { c_id: u64, color: [f32; 4] }, // RGBA
    SetDimensions { c_id: u64, dimensions: [f32; 3] },
    SetEntityType { c_id: u64, entity_type: u8 },
    Remove { c_id: u64 },
    Shutdown,
}

// Connection status for startup
static CONNECTION_SUCCESS: AtomicBool = AtomicBool::new(false);
static CONNECTION_FAILED: AtomicBool = AtomicBool::new(false);

impl Migrate for BridgeState { type Old = Self; }

impl ClientState for BridgeState {
    const APP_ID: &'static str = "org.stardustxr.starworld";
    fn initial_state_update(&mut self) {}
    
    fn on_frame(&mut self, _info: &stardust_xr_fusion::root::FrameInfo) {
        // Sync from the global shared state on each frame
        if let Ok(ctrl) = CTRL.lock() {
            if let Some(shared) = &ctrl.shared_state {
                if let Ok(shared_state) = shared.lock() {
                    eprintln!("[bridge/on_frame] Syncing {} nodes from shared_state", shared_state.nodes.len());
                    self.nodes = shared_state.nodes.clone();
                } else {
                    eprintln!("[bridge/on_frame] Failed to lock shared_state");
                }
            } else {
                eprintln!("[bridge/on_frame] No shared_state in ctrl");
            }
        } else {
            eprintln!("[bridge/on_frame] Failed to lock CTRL");
        }
    }
}

impl Reify for BridgeState {
    fn reify(&self) -> impl ast::Element<Self> {
        eprintln!("[bridge/reify] Reifying {} nodes", self.nodes.len());
        
        // Initialize model downloader if not already done
        let downloader = MODEL_DOWNLOADER.get_or_init(|| {
            let cache_dir = dirs::cache_dir()
                .unwrap_or_else(|| PathBuf::from("/tmp"))
                .join("starworld/models");
            ModelDownloader::new(cache_dir).expect("Failed to create model downloader")
        });
        
        fn get_model_path(entity_type: u8, model_url: &str, downloader: &ModelDownloader) -> Option<PathBuf> {
            // First check if there's a model URL provided
            if !model_url.is_empty() {
                // Handle HTTP/HTTPS URLs
                if model_url.starts_with("http://") || model_url.starts_with("https://") {
                    eprintln!("[bridge/reify] Attempting to download model from URL: {}", model_url);
                    if let Some(path) = downloader.get_model(model_url) {
                        eprintln!("[bridge/reify] Using downloaded model: {}", path.display());
                        return Some(path);
                    } else {
                        eprintln!("[bridge/reify] Model download failed or in progress, falling back to primitive");
                        // Fall through to use primitive based on entity type
                    }
                } else if model_url.starts_with("file://") {
                    // Local file path
                    let path = PathBuf::from(&model_url[7..]);
                    if path.exists() {
                        eprintln!("[bridge/reify] Using local file: {}", path.display());
                        return Some(path);
                    }
                } else if PathBuf::from(model_url).exists() {
                    // Direct path
                    eprintln!("[bridge/reify] Using direct path: {}", model_url);
                    return Some(PathBuf::from(model_url));
                }
            }
            
            // Fall back to primitive models based on entity type
            let cache_dir = dirs::cache_dir()?.join("starworld/primitives");
            let filename = match entity_type {
                1 => "cube.glb",      // Box
                2 => "sphere.glb",    // Sphere  
                3 => "model.glb",     // Model (using Suzanne as placeholder)
                _ => return None,
            };
            let path = cache_dir.join(filename);
            if path.exists() {
                Some(path)
            } else {
                eprintln!("[bridge/reify] Primitive model file not found: {}", path.display());
                None
            }
        }
        
        let children = self.nodes.iter().filter_map(|(id, node)| {
            let dims = glam::Vec3::from(node.dimensions);
            if dims.length() < 0.001 {
                eprintln!("[bridge/reify] Skipping node {} (zero dimensions)", id);
                return None;
            }
            
            let (scale, rot, trans) = node.transform.to_scale_rotation_translation();
            let vis_scale = if dims.length() > 0.001 { dims } else { scale };
            
            let trans_array = [trans.x, trans.y, trans.z];
            let rot_array = [rot.x, rot.y, rot.z, rot.w];
            let scale_array = [vis_scale.x, vis_scale.y, vis_scale.z];
            let transform = stardust_xr_fusion::spatial::Transform::from_translation_rotation_scale(trans_array, rot_array, scale_array);
            
            // Try to load the appropriate model based on entity type and model URL
            let model_child = if let Some(model_path) = get_model_path(node.entity_type, &node.model_url, downloader) {
                let entity_type_name = match node.entity_type {
                    1 => "cube",
                    2 => "sphere",
                    3 => "3D model",
                    _ => "unknown"
                };
                
                let model_source = if !node.model_url.is_empty() {
                    format!("from URL: {}", node.model_url)
                } else {
                    format!("primitive from {}", model_path.display())
                };
                
                eprintln!("[bridge/reify] Loading {} for node {} {}", 
                    entity_type_name, id, model_source);
                
                match Model::direct(&model_path) {
<<<<<<< HEAD
                    Ok(mut model) => {
                        // Asteroids Model now supports material color tinting.
                        if node.color != [1.0, 1.0, 1.0, 1.0] {
                            let color = ast::elements::RgbaLinear::new(
                                node.color[0], node.color[1], node.color[2], node.color[3]
                            );
                            model = model.color_tint(color);
                            eprintln!("[bridge/reify] Node {}: applied color tint RGBA({:.2}, {:.2}, {:.2}, {:.2})",
                                id, node.color[0], node.color[1], node.color[2], node.color[3]);
                        }

                        // TODO: Apply texture from texture_url (pending API)
=======
                    Ok(model) => {
                        // TODO: Color tinting is not currently supported due to missing public API in asteroids.
                        // When Model/MaterialParameter API is available, apply color here.
                        if node.color != [1.0, 1.0, 1.0, 1.0] {
                            eprintln!("[bridge/reify] Node {} requested color tint RGBA({:.2}, {:.2}, {:.2}, {:.2}) -- NOT SUPPORTED YET", 
                                id, node.color[0], node.color[1], node.color[2], node.color[3]);
                        }
                        // TODO: Apply texture from texture_url (future)
>>>>>>> 0a39697599277320e2650a938b695beeb401c931
                        if !node.texture_url.is_empty() {
                            eprintln!("[bridge/reify] Node {} has texture URL: {} - NOT YET APPLIED (API limitation)",
                                id, node.texture_url);
                        }
<<<<<<< HEAD

=======
>>>>>>> 0a39697599277320e2650a938b695beeb401c931
                        Some(model.build())
                    }
                    Err(e) => {
                        eprintln!("[bridge/reify] Failed to load model for node {}: {}", id, e);
                        None
                    }
                }
            } else {
                eprintln!("[bridge/reify] No model available for entity type {} (node {})", node.entity_type, id);
                None
            };
            
            Some((*id, Spatial::default()
                .transform(transform)
                .build()
                .maybe_child(model_child)))
        });

        PlaySpace.build().stable_children(children)
    }
}

static STARTED: AtomicBool = AtomicBool::new(false);
static STOP_REQUESTED: AtomicBool = AtomicBool::new(false);
lazy_static::lazy_static! {
    static ref CTRL: Mutex<Ctrl> = Mutex::new(Ctrl::default());
}

#[derive(Default, Clone, serde::Serialize, serde::Deserialize)]
struct Node {
    id: u64,
    name: String,
                        Ok(mut model) => {
                            // Asteroids Model now supports material color tinting.
                            if node.color != [1.0, 1.0, 1.0, 1.0] {
                                let color = ast::elements::RgbaLinear::new(
                                    node.color[0], node.color[1], node.color[2], node.color[3]
                                );
                                model = model.color_tint(color);
                                eprintln!("[bridge/reify] Node {}: applied color tint RGBA({:.2}, {:.2}, {:.2}, {:.2})",
                                    id, node.color[0], node.color[1], node.color[2], node.color[3]);
                            }

                            // TODO: Apply texture from texture_url (pending API)
                            if !node.texture_url.is_empty() {
                                eprintln!("[bridge/reify] Node {} has texture URL: {} - NOT YET APPLIED (API limitation)",
                                    id, node.texture_url);
                            }
                            Some(model.build())
                        }
                        Err(e) => {
                            eprintln!("[bridge/reify] Failed to load model for node {}: {}", id, e);
                            None
                        }
    ctrl.next_id = 1;
    let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel::<Command>();
    ctrl.tx = Some(tx.clone());

    // Shared state that both the command handler and the client state will access
    let shared_state = Arc::new(Mutex::new(BridgeState::default()));
    let shared_for_commands = Arc::clone(&shared_state);
    let shared_for_event_loop = Arc::clone(&shared_state);

    // Build a multi-threaded Tokio runtime for the client
    let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .expect("tokio runtime");
    let handle = std::thread::spawn(move || {
        let res = rt.block_on(async move {
            // Spawn command processor task that updates shared state
            let cmd_task = tokio::spawn(async move {
                while let Some(cmd) = rx.recv().await {
                    match cmd {
                        Command::Create { c_id, name, transform } => {
                            if let Ok(mut state) = shared_for_commands.lock() {
                                let node = Node {
                                    id: c_id,
                                    name: name.clone(),
                                    transform,
                                    entity_type: 1, // Default to Box
                                    model_url: String::new(),
                                    texture_url: String::new(),
                                    color: [1.0, 1.0, 1.0, 1.0], // White
                                    dimensions: [0.1, 0.1, 0.1], // Default 10cm cube
                                };
                                state.nodes.insert(c_id, node);
                                println!("[bridge] create node id={} name={} (state nodes={})", c_id, name, state.nodes.len());
                            }
                        }
                        Command::Update { c_id, transform } => {
                            if let Ok(mut state) = shared_for_commands.lock() {
                                if let Some(n) = state.nodes.get_mut(&c_id) {
                                    n.transform = transform;
                                    // Suppress verbose per-frame update logs; enable for debugging if needed
                                    // println!("[bridge] update node id={}", c_id);
                                } else {
                                    println!("[bridge] update for unknown node id={}", c_id);
                                }
                            }
                        }
                        Command::SetModel { c_id, model_url } => {
                            if let Ok(mut state) = shared_for_commands.lock() {
                                if let Some(n) = state.nodes.get_mut(&c_id) {
                                    n.model_url = model_url.clone();
                                    println!("[bridge] set model for node id={}: {}", c_id, model_url);
                                }
                            }
                        }
                        Command::SetTexture { c_id, texture_url } => {
                            if let Ok(mut state) = shared_for_commands.lock() {
                                if let Some(n) = state.nodes.get_mut(&c_id) {
                                    n.texture_url = texture_url.clone();
                                    println!("[bridge] set texture for node id={}: {}", c_id, texture_url);
                                }
                            }
                        }
                        Command::SetColor { c_id, color } => {
                            if let Ok(mut state) = shared_for_commands.lock() {
                                if let Some(n) = state.nodes.get_mut(&c_id) {
                                    n.color = color;
                                    println!("[bridge] set color for node id={}: {:?}", c_id, color);
                                }
                            }
                        }
                        Command::SetDimensions { c_id, dimensions } => {
                            if let Ok(mut state) = shared_for_commands.lock() {
                                if let Some(n) = state.nodes.get_mut(&c_id) {
                                    n.dimensions = dimensions;
                                    println!("[bridge] set dimensions for node id={}: {:?}", c_id, dimensions);
                                }
                            }
                        }
                        Command::SetEntityType { c_id, entity_type } => {
                            if let Ok(mut state) = shared_for_commands.lock() {
                                if let Some(n) = state.nodes.get_mut(&c_id) {
                                    n.entity_type = entity_type;
                                    println!("[bridge] set entity type for node id={}: {}", c_id, entity_type);
                                }
                            }
                        }
                        Command::Remove { c_id } => {
                            if let Ok(mut state) = shared_for_commands.lock() {
                                if state.nodes.remove(&c_id).is_some() {
                                    println!("[bridge] remove node id={} (remaining={})", c_id, state.nodes.len());
                                }
                            }
                        }
                        Command::Shutdown => { STOP_REQUESTED.store(true, Ordering::SeqCst); break; }
                    }
                }
            });
            println!("[bridge] Connecting to Stardust server...");
            // Retry fusion connect with a timeout to detect missing compositor
            let max_retries = 10; // 5 seconds total (10 * 500ms)
            let mut retry_count = 0;
            let mut client = loop {
                match stardust_xr_fusion::client::Client::connect().await {
                    Ok(c) => {
                        println!("[bridge] Successfully connected to Stardust compositor");
                        CONNECTION_SUCCESS.store(true, Ordering::SeqCst);
                        break c;
                    }
                    Err(e) => {
                        retry_count += 1;
                        if retry_count >= max_retries {
                            eprintln!("[bridge] ERROR: Could not connect to Stardust compositor after {} attempts", max_retries);
                            eprintln!("[bridge] ERROR: {:?}", e);
                            eprintln!("[bridge] Make sure the Stardust server is running:");
                            eprintln!("[bridge]   systemctl --user start stardust");
                            eprintln!("[bridge]   or: stardust-xr-server");
                            CONNECTION_FAILED.store(true, Ordering::SeqCst);
                            return; // Exit the async block, which will cause sdxr_start to return error
                        }
                        eprintln!("[bridge] Fusion connect failed (attempt {}/{}): {:?}; retrying...", retry_count, max_retries, e);
                        tokio::time::sleep(std::time::Duration::from_millis(500)).await;
                        if STOP_REQUESTED.load(Ordering::SeqCst) { return; }
                    }
                }
            };
            let dbus_connection = match fusion_connect_client().await {
                Ok(c) => c,
                Err(e) => {
                    eprintln!("[bridge] DBus connect failed: {:?}; continuing without context extras", e);
                    // Fallback to a new connection attempt with default
                    match fusion_connect_client().await {
                        Ok(c2) => c2,
                        Err(_) => return,
                    }
                }
            };
            
            let _dbus_connection = match stardust_xr_fusion::objects::connect_client().await {
                Ok(conn) => conn,
                Err(_) => {
                    eprintln!("[bridge] Failed to connect to D-Bus, using fallback");
                    match fusion_connect_client().await {
                        Ok(c2) => c2,
                        Err(_) => return,
                    }
                }
            };
            
            let accent_color = AccentColor::new(dbus_connection.clone());
            let context = Context { dbus_connection, accent_color };
            // Use the shared_state Arc instead of creating a new BridgeState
            let mut projector = {
                let state_guard = shared_for_event_loop.lock().unwrap();
                Projector::create(&*state_guard, &context, client.get_root().clone().as_spatial_ref(), "/".into())
            };
            
            println!("[bridge] Persistent event loop running");
            let event_loop_fut = client.sync_event_loop(|client, flow| {
                use stardust_xr_fusion::root::{RootEvent, ClientState as SaveStatePayload};
                let mut frames = vec![];
                while let Some(re) = client.get_root().recv_root_event() {
                    match re {
                        RootEvent::Ping { response } => {
                            let _ = response.send_ok(());
                        }
                        RootEvent::Frame { info } => frames.push(info),
                        RootEvent::SaveState { response } => {
                            let payload = SaveStatePayload { data: None, root: client.get_root().id(), spatial_anchors: Default::default() };
                            let _ = response.send_ok(payload);
                        }
                    }
                }
                if frames.is_empty() { return; }
                
                // Lock shared_state and work with it
                if let Ok(mut state) = shared_for_event_loop.lock() {
                    eprintln!("[bridge/event_loop] Processing {} frames, state has {} nodes", frames.len(), state.nodes.len());
                    for frame in frames {
                        state.on_frame(&frame);
                        projector.frame(&context, &frame, &mut *state);
                    }
                    projector.update(&context, &mut *state);
                }
                
                if STOP_REQUESTED.load(Ordering::SeqCst) { flow.stop(); }
            });
            if let Err(e) = event_loop_fut.await {
                eprintln!("[bridge] Event loop error: {:?}", e);
            }
            println!("[bridge] Event loop terminated");
            let _ = cmd_task;
        });
        drop(rt);
        let _ = res;
        STARTED.store(false, Ordering::SeqCst);
    });

    ctrl.rt = None; // runtime consumed inside thread
    ctrl.handle = Some(handle);
    // Store the shared state so we can read from it later
    ctrl.shared_state = Some(shared_state);
    
    STOP_REQUESTED.store(false, Ordering::SeqCst);
    
    // Wait for connection to succeed or fail (max 6 seconds)
    let max_wait_iterations = 120; // 120 * 50ms = 6 seconds
    for _ in 0..max_wait_iterations {
        if CONNECTION_SUCCESS.load(Ordering::SeqCst) {
            println!("[bridge] Connection established successfully");
            return 0; // Success
        }
        if CONNECTION_FAILED.load(Ordering::SeqCst) {
            eprintln!("[bridge] Connection failed - exiting");
            STARTED.store(false, Ordering::SeqCst);
            return -1; // Failure
        }
        std::thread::sleep(std::time::Duration::from_millis(50));
    }
    
    eprintln!("[bridge] WARNING: Connection status unknown after timeout");
    0 // Assume success to maintain backwards compatibility if status isn't set
}

#[no_mangle]
pub extern "C" fn sdxr_poll() -> i32 { if !STARTED.load(Ordering::SeqCst) { -1 } else { 0 } }

#[no_mangle]
pub extern "C" fn sdxr_shutdown() {
    let mut ctrl = CTRL.lock().unwrap();
    if let Some(tx) = ctrl.tx.take() {
        let _ = tx.send(Command::Shutdown);
    }
    if let Some(h) = ctrl.handle.take() {
        let _ = h.join();
    }
    STARTED.store(false, Ordering::SeqCst);
}

#[no_mangle]
pub extern "C" fn sdxr_create_node(name: *const std::os::raw::c_char, mat4: *const f32) -> u64 {
    if !STARTED.load(Ordering::SeqCst) { return 0; }
    let name = unsafe { CStr::from_ptr(name) }.to_string_lossy().to_string();
    let m = unsafe { std::slice::from_raw_parts(mat4, 16) };
    let mut arr = [0.0f32; 16];
    arr.copy_from_slice(m);
    let mat = Mat4::from_cols_array(&arr);

    let mut ctrl = CTRL.lock().unwrap();
    let c_id = ctrl.next_id; ctrl.next_id += 1;
    if let Some(tx) = &ctrl.tx { let _ = tx.send(Command::Create { c_id, name, transform: mat }); }
    c_id
}

#[no_mangle]
pub extern "C" fn sdxr_update_node(id: u64, mat4: *const f32) -> i32 {
    if !STARTED.load(Ordering::SeqCst) { return -1; }
    let m = unsafe { std::slice::from_raw_parts(mat4, 16) };
    let mut arr = [0.0f32; 16];
    arr.copy_from_slice(m);
    let mat = Mat4::from_cols_array(&arr);
    let ctrl = CTRL.lock().unwrap();
    if let Some(tx) = &ctrl.tx { let _ = tx.send(Command::Update { c_id: id, transform: mat }); }
    0
}

#[no_mangle]
pub extern "C" fn sdxr_remove_node(id: u64) -> i32 {
    if !STARTED.load(Ordering::SeqCst) { return -1; }
    let ctrl = CTRL.lock().unwrap();
    if let Some(tx) = &ctrl.tx { let _ = tx.send(Command::Remove { c_id: id }); }
    0
}

// Optional: expose number of nodes for diagnostics
#[no_mangle]
pub extern "C" fn sdxr_node_count() -> u64 {
    if !STARTED.load(Ordering::SeqCst) { return 0; }
    let ctrl = CTRL.lock().unwrap();
    ctrl.nodes.len() as u64
}

#[no_mangle]
pub extern "C" fn sdxr_set_node_model(id: u64, model_url: *const std::os::raw::c_char) -> i32 {
    if !STARTED.load(Ordering::SeqCst) { return -1; }
    let url = unsafe { CStr::from_ptr(model_url) }.to_string_lossy().to_string();
    let ctrl = CTRL.lock().unwrap();
    if let Some(tx) = &ctrl.tx {
        let _ = tx.send(Command::SetModel { c_id: id, model_url: url });
    }
    0
}

#[no_mangle]
pub extern "C" fn sdxr_set_node_texture(id: u64, texture_url: *const std::os::raw::c_char) -> i32 {
    if !STARTED.load(Ordering::SeqCst) { return -1; }
    let url = unsafe { CStr::from_ptr(texture_url) }.to_string_lossy().to_string();
    let ctrl = CTRL.lock().unwrap();
    if let Some(tx) = &ctrl.tx {
        let _ = tx.send(Command::SetTexture { c_id: id, texture_url: url });
    }
    0
}

#[no_mangle]
pub extern "C" fn sdxr_set_node_color(id: u64, r: f32, g: f32, b: f32, a: f32) -> i32 {
    if !STARTED.load(Ordering::SeqCst) { return -1; }
    let ctrl = CTRL.lock().unwrap();
    if let Some(tx) = &ctrl.tx {
        let _ = tx.send(Command::SetColor { c_id: id, color: [r, g, b, a] });
    }
    0
}

#[no_mangle]
pub extern "C" fn sdxr_set_node_dimensions(id: u64, x: f32, y: f32, z: f32) -> i32 {
    if !STARTED.load(Ordering::SeqCst) { return -1; }
    let ctrl = CTRL.lock().unwrap();
    if let Some(tx) = &ctrl.tx {
        let _ = tx.send(Command::SetDimensions { c_id: id, dimensions: [x, y, z] });
    }
    0
}

#[no_mangle]
pub extern "C" fn sdxr_set_node_entity_type(id: u64, entity_type: u8) -> i32 {
    if !STARTED.load(Ordering::SeqCst) { return -1; }
    let ctrl = CTRL.lock().unwrap();
    if let Some(tx) = &ctrl.tx {
        let _ = tx.send(Command::SetEntityType { c_id: id, entity_type });
    }
    0
}
