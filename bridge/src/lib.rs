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
use stardust_xr_asteroids::CustomElement;
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
                    .pos(trans)
                    .rot(rot)
                    .scl(vis_scale)
                    .build()
            )
        });

        PlaySpace
            .build()
            .stable_children(children)
    }
}

static STARTED: AtomicBool = AtomicBool::new(false);
lazy_static::lazy_static! {
    static ref CTRL: Mutex<Ctrl> = Mutex::new(Ctrl::default());
}

#[derive(Default)]
struct Node {
    id: u64,
    name: String,
    transform: Mat4,
}

struct Ctrl {
    rt: Option<Runtime>,
    handle: Option<JoinHandle<()>>, // client running thread
    tx: Option<tokio::sync::mpsc::UnboundedSender<Command>>,
    next_id: u64,
    nodes: HashMap<u64, Node>,
}

impl Default for Ctrl {
    fn default() -> Self {
        Self {
            rt: None,
            handle: None,
            tx: None,
            next_id: 1,
            nodes: HashMap::new(),
        }
    }
}

#[no_mangle]
pub extern "C" fn sdxr_start(app_id: *const std::os::raw::c_char) -> i32 {
    if STARTED.swap(true, Ordering::SeqCst) { return 0; }
    let name = unsafe { CStr::from_ptr(app_id) }.to_string_lossy().to_string();

    let mut ctrl = CTRL.lock().unwrap();
    ctrl.next_id = 1;
    let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel::<Command>();
    ctrl.tx = Some(tx.clone());

    // Build a single-threaded Tokio runtime for the client
    let rt = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .expect("tokio runtime");
    let handle = std::thread::spawn(move || {
        let res = rt.block_on(async move {
            // Run the client with our BridgeState (root PlaySpace)
            let _state = BridgeState {};

            // Spawn command processor task
            let cmd_task = tokio::spawn(async move {
                // We cannot mutate CTRL from inside this async task directly, so we
                // accumulate changes and apply them via a secondary channel or by
                // locking CTRL per message. Simplicity first: lock per command.
                while let Some(cmd) = rx.recv().await {
                    match cmd {
                        Command::Create { c_id, name, transform } => {
                            if let Ok(mut ctrl_locked) = CTRL.lock() {
                                ctrl_locked.nodes.insert(c_id, Node { id: c_id, name: name.clone(), transform });
                            }
                            println!("[bridge] create node id={} name={}", c_id, name);
                        }
                        Command::Update { c_id, transform } => {
                            if let Ok(mut ctrl_locked) = CTRL.lock() {
                                if let Some(n) = ctrl_locked.nodes.get_mut(&c_id) {
                                    n.transform = transform;
                                    println!("[bridge] update node id={}", c_id);
                                } else {
                                    println!("[bridge] update for unknown node id={}", c_id);
                                }
                            }
                        }
                        Command::Shutdown => break,
                    }
                }
            });

            ast::client::run::<BridgeState>(&[]).await;
            // Runtime shutdown will drop task; we ignore join result intentionally.
            let _ = cmd_task;
        });
        drop(rt);
        let _ = res;
        STARTED.store(false, Ordering::SeqCst);
    });

    ctrl.rt = None; // runtime consumed inside thread
    ctrl.handle = Some(handle);
    0
}

#[no_mangle]
pub extern "C" fn sdxr_poll() -> i32 {
    if !STARTED.load(Ordering::SeqCst) { return -1; }
    0
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
