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
    elements::{PlaySpace, Axes},
    Migrate, Reify,
};
use stardust_xr_asteroids::{CustomElement, Transformable};
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
                    self.nodes = shared_state.nodes.clone();
                }
            }
        }
    }
}

impl Reify for BridgeState {
    fn reify(&self) -> impl ast::Element<Self> {
        // Root playspace. Attach a visible Axes element per tracked node id.
        let children = self.nodes.iter().map(|(id, node)| {
            // Decompose transform into TRS
            let (scale, rot, trans) = node.transform.to_scale_rotation_translation();
            // Make axes much larger and visible: default is 1cm, scale up to 20cm
            let vis_scale = glam::Vec3::splat(20.0) * scale.x;
            (
                *id,
                Axes::default()
                    .pos([trans.x, trans.y, trans.z])
                    .rot([rot.x, rot.y, rot.z, rot.w])
                    .scl([vis_scale.x, vis_scale.y, vis_scale.z])
                    .build()
            )
        });

        PlaySpace
            .build()
            .stable_children(children)
    }
}

static STARTED: AtomicBool = AtomicBool::new(false);
static CONNECTED: AtomicBool = AtomicBool::new(false);
lazy_static::lazy_static! {
    static ref CTRL: Mutex<Ctrl> = Mutex::new(Ctrl::default());
}

#[derive(Default, Clone, serde::Serialize, serde::Deserialize)]
struct Node {
    id: u64,
    name: String,
    #[serde(skip)]
    transform: Mat4,
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

    // Build a single-threaded Tokio runtime for the client
    let rt = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .expect("tokio runtime");
    let handle = std::thread::spawn(move || {
        // Initialize tracing for debugging
        let _ = tracing_subscriber::fmt()
            .with_env_filter(tracing_subscriber::EnvFilter::from_default_env()
                .add_directive("stardust_xr_fusion=debug".parse().unwrap()))
            .try_init();
            
        let res = rt.block_on(async move {
            // Spawn command processor task that updates shared state
            let cmd_task = tokio::spawn(async move {
                while let Some(cmd) = rx.recv().await {
                    match cmd {
                        Command::Create { c_id, name, transform } => {
                            if let Ok(mut state) = shared_for_commands.lock() {
                                state.nodes.insert(c_id, Node { id: c_id, name: name.clone(), transform });
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
                        Command::Shutdown => break,
                    }
                }
            });

            println!("[bridge] Connecting to Stardust server...");
            
            // Run the client - asteroids will manage the projector and call reify() each frame
            // This blocks until the client disconnects or is shut down
            ast::client::run::<BridgeState>(&[]).await;
            
            println!("[bridge] Client disconnected");
            let _ = cmd_task;
        });
        drop(rt);
        let _ = res;
        STARTED.store(false, Ordering::SeqCst);
        CONNECTED.store(false, Ordering::SeqCst);
    });

    ctrl.rt = None; // runtime consumed inside thread
    ctrl.handle = Some(handle);
    // Store the shared state so we can read from it later
    ctrl.shared_state = Some(shared_state);
    
    // Give the async runtime a moment to attempt connection
    std::thread::sleep(std::time::Duration::from_millis(100));
    
    // If the thread is still alive, assume connection succeeded
    // (if it failed immediately, STARTED would be false)
    if STARTED.load(Ordering::SeqCst) {
        CONNECTED.store(true, Ordering::SeqCst);
    }
    
    0
}

#[no_mangle]
pub extern "C" fn sdxr_poll() -> i32 {
    if !STARTED.load(Ordering::SeqCst) { return -1; }
    // Return 0 if connected and running, -1 if disconnected
    if CONNECTED.load(Ordering::SeqCst) { 0 } else { -1 }
}

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

// Optional: expose number of nodes for diagnostics
#[no_mangle]
pub extern "C" fn sdxr_node_count() -> u64 {
    if !STARTED.load(Ordering::SeqCst) { return 0; }
    let ctrl = CTRL.lock().unwrap();
    ctrl.nodes.len() as u64
}
