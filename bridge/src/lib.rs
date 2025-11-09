// Rust C-ABI bridge for StardustXR client integration.

use std::collections::HashMap;
use std::ffi::CStr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread::JoinHandle;

use glam::Mat4;
use stardust_xr_asteroids as ast; // alias for brevity
use stardust_xr_asteroids::{
    client::ClientState,
    elements::{PlaySpace, Spatial, Lines, Model, ModelPart},
    Migrate, Reify, CustomElement, Projector, Context,
};
use stardust_xr_molecules::accent_color::AccentColor;
use stardust_xr_fusion::objects::connect_client as fusion_connect_client;
use stardust_xr_fusion::node::NodeType;
use stardust_xr_fusion::root::RootAspect;
use stardust_xr_fusion::spatial::Transform;
use stardust_xr_fusion::drawable::MaterialParameter;
use stardust_xr_fusion::values::ResourceID;
use tokio::runtime::Runtime;

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
        use stardust_xr_fusion::values::{color, Vector3};
        use stardust_xr_fusion::drawable::{Line, LinePoint};
        
        eprintln!("[bridge/reify] Reifying {} nodes", self.nodes.len());
        
        fn create_wireframe_cube(color_val: stardust_xr_fusion::values::Color, thickness: f32) -> Vec<Line> {
            let h = 0.5; // half size
            let points = [
                [-h, -h, -h], [h, -h, -h], [h, h, -h], [-h, h, -h], // back face
                [-h, -h, h], [h, -h, h], [h, h, h], [-h, h, h], // front face
            ];
            
            // 12 edges of the cube
            let edges = [
                (0, 1), (1, 2), (2, 3), (3, 0), // back face
                (4, 5), (5, 6), (6, 7), (7, 4), // front face
                (0, 4), (1, 5), (2, 6), (3, 7), // connecting edges
            ];
            
            edges.iter().map(|(a, b)| {
                let pa = points[*a];
                let pb = points[*b];
                Line {
                    points: vec![
                        LinePoint { point: Vector3 { x: pa[0], y: pa[1], z: pa[2] }, thickness, color: color_val },
                        LinePoint { point: Vector3 { x: pb[0], y: pb[1], z: pb[2] }, thickness, color: color_val },
                    ],
                    cyclic: false,
                }
            }).collect()
        }
        
        fn create_wireframe_sphere(color_val: stardust_xr_fusion::values::Color, thickness: f32) -> Vec<Line> {
            let segments = 32;
            let r = 0.5; // radius
            let mut lines = Vec::new();
            
            // Create 3 orthogonal circles (XY, XZ, YZ planes)
            for axis in 0..3 {
                let mut points = Vec::new();
                for i in 0..=segments {
                    let angle = (i as f32 / segments as f32) * std::f32::consts::TAU;
                    let (sin, cos) = angle.sin_cos();
                    let point = match axis {
                        0 => Vector3 { x: cos * r, y: sin * r, z: 0.0 }, // XY plane
                        1 => Vector3 { x: cos * r, y: 0.0, z: sin * r }, // XZ plane
                        _ => Vector3 { x: 0.0, y: cos * r, z: sin * r }, // YZ plane
                    };
                    points.push(LinePoint { point, thickness, color: color_val });
                }
                lines.push(Line { points, cyclic: true });
            }
            lines
        }
        
        fn create_octahedron_wireframe(color_val: stardust_xr_fusion::values::Color, thickness: f32) -> Vec<Line> {
            let r = 0.5;
            // 6 vertices of octahedron
            let verts = [
                Vector3 { x: 0.0, y: r, z: 0.0 },    // top
                Vector3 { x: r, y: 0.0, z: 0.0 },    // +X
                Vector3 { x: 0.0, y: 0.0, z: r },    // +Z
                Vector3 { x: -r, y: 0.0, z: 0.0 },   // -X
                Vector3 { x: 0.0, y: 0.0, z: -r },   // -Z
                Vector3 { x: 0.0, y: -r, z: 0.0 },   // bottom
            ];
            
            // 12 edges
            let edges = [
                (0, 1), (0, 2), (0, 3), (0, 4), // top pyramid
                (5, 1), (5, 2), (5, 3), (5, 4), // bottom pyramid
                (1, 2), (2, 3), (3, 4), (4, 1), // equator
            ];
            
            edges.iter().map(|(a, b)| {
                Line {
                    points: vec![
                        LinePoint { point: verts[*a], thickness, color: color_val },
                        LinePoint { point: verts[*b], thickness, color: color_val },
                    ],
                    cyclic: false,
                }
            }).collect()
        }
        
        let children = self.nodes.iter().filter_map(|(id, node)| {
            let dims = glam::Vec3::from(node.dimensions);
            if dims.length() < 0.001 {
                eprintln!("[bridge/reify] Skipping node {} (zero dimensions)", id);
                return None;
            }
            
            let (scale, rot, trans) = node.transform.to_scale_rotation_translation();
            let vis_scale = if dims.length() > 0.001 { dims } else { scale };
            let node_color = color::rgba_linear!(node.color[0], node.color[1], node.color[2], node.color[3]);
            
            let trans_array = [trans.x, trans.y, trans.z];
            let rot_array = [rot.x, rot.y, rot.z, rot.w];
            let scale_array = [vis_scale.x, vis_scale.y, vis_scale.z];
            let transform = stardust_xr_fusion::spatial::Transform::from_translation_rotation_scale(trans_array, rot_array, scale_array);
            
            // Create appropriate visual based on entity type
            match node.entity_type {
                1 => {
                    // Box - use wireframe cube with color
                    eprintln!("[bridge/reify] Creating box (wireframe) for node {}", id);
                    let cube_lines = create_wireframe_cube(node_color, 0.005);
                    Some((*id, Spatial::default()
                        .transform(transform)
                        .build()
                        .child(Lines::new(cube_lines).build())))
                }
                2 => {
                    // Sphere - use wireframe sphere with color
                    eprintln!("[bridge/reify] Creating sphere (wireframe) for node {}", id);
                    let sphere_lines = create_wireframe_sphere(node_color, 0.005);
                    Some((*id, Spatial::default()
                        .transform(transform)
                        .build()
                        .child(Lines::new(sphere_lines).build())))
                }
                3 => {
                    // Model - attempt to load from URL if provided, fallback to wireframe
                    if !node.model_url.is_empty() {
                        eprintln!("[bridge/reify] Creating model for node {} from URL: {}", id, node.model_url);
                        // For now, we can't easily load arbitrary HTTP URLs in the Overte format
                        // Fall back to wireframe octahedron as a distinct placeholder
                        let oct_lines = create_octahedron_wireframe(node_color, 0.005);
                        Some((*id, Spatial::default()
                            .transform(transform)
                            .build()
                            .child(Lines::new(oct_lines).build())))
                    } else {
                        eprintln!("[bridge/reify] Creating model placeholder for node {} (no URL)", id);
                        let oct_lines = create_octahedron_wireframe(node_color, 0.005);
                        Some((*id, Spatial::default()
                            .transform(transform)
                            .build()
                            .child(Lines::new(oct_lines).build())))
                    }
                }
                _ => {
                    // Unknown or unsupported type - render as wireframe cube
                    eprintln!("[bridge/reify] Creating wireframe for unknown node {} type={}", id, node.entity_type);
                    let cube_lines = create_wireframe_cube(node_color, 0.003);
                    Some((*id, Spatial::default()
                        .transform(transform)
                        .build()
                        .child(Lines::new(cube_lines).build())))
                }
            }
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
    #[serde(skip)]
    transform: Mat4,
    entity_type: u8, // 0=Unknown, 1=Box, 2=Sphere, 3=Model, etc.
    model_url: String,
    texture_url: String,
    #[serde(skip)]
    color: [f32; 4], // RGBA
    #[serde(skip)]
    dimensions: [f32; 3], // xyz dimensions in meters
}

struct Ctrl {
    rt: Option<Runtime>,
    handle: Option<JoinHandle<()>>, // client running thread
    tx: Option<tokio::sync::mpsc::UnboundedSender<Command>>,
    next_id: u64,
    nodes: HashMap<u64, Node>,
    shared_state: Option<Arc<Mutex<BridgeState>>>,
}

impl Default for Ctrl {
    fn default() -> Self {
        Self {
            rt: None,
            handle: None,
            tx: None,
            next_id: 1,
            nodes: HashMap::new(),
            shared_state: None,
        }
    }
}

#[no_mangle]
pub extern "C" fn sdxr_start(app_id: *const std::os::raw::c_char) -> i32 {
    if STARTED.swap(true, Ordering::SeqCst) { return 0; }
    let _name = unsafe { CStr::from_ptr(app_id) }.to_string_lossy().to_string();

    let mut ctrl = CTRL.lock().unwrap();
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
            // Retry fusion connect for a few seconds to handle compositor wake-up races.
            let mut client = loop {
                match stardust_xr_fusion::client::Client::connect().await {
                    Ok(c) => break c,
                    Err(e) => {
                        eprintln!("[bridge] Fusion connect failed: {:?}; retrying...", e);
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
            
            let dbus_connection = match stardust_xr_fusion::objects::connect_client().await {
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
    0
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
