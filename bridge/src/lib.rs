// Rust C-ABI bridge for StardustXR client integration.

use std::ffi::CStr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;
use std::thread::JoinHandle;

use glam::Mat4;
use stardust_xr_asteroids as ast; // alias for brevity
use stardust_xr_asteroids::{
    client::ClientState,
    elements::PlaySpace,
    Migrate, Reify,
    CustomElement, Element,
};
use tokio::runtime::Runtime;

#[derive(Default, serde::Serialize, serde::Deserialize)]
struct BridgeState {}

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
        // Root playspace. We attach our dynamic nodes under this.
        PlaySpace.build()
    }
}

static STARTED: AtomicBool = AtomicBool::new(false);
lazy_static::lazy_static! {
    static ref CTRL: Mutex<Ctrl> = Mutex::new(Ctrl::default());
}

#[derive(Default)]
struct Ctrl {
    rt: Option<Runtime>,
    handle: Option<JoinHandle<()>>, // client running thread
    tx: Option<tokio::sync::mpsc::UnboundedSender<Command>>,
    next_id: u64,
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
            // Run the client with our BridgeState
            let _state = BridgeState {};

            // Launch a task to apply incoming commands once the client is up
            let cmd_task = tokio::spawn(async move {
                // This is a placeholder; in a full implementation we would
                // hold references to created nodes. For now we simply drain.
                while let Some(cmd) = rx.recv().await {
                    match cmd {
                        Command::Create { .. } => {}
                        Command::Update { .. } => {}
                        Command::Shutdown => break,
                    }
                }
            });

            ast::client::run::<BridgeState>(&[]).await;
            // Do not await cmd_task here; runtime shutdown will cancel it
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
