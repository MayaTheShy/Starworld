// Minimal test to verify socket connection
use tokio::net::UnixStream;

#[tokio::main]
async fn main() {
    let socket_path = std::env::var("XDG_RUNTIME_DIR")
        .unwrap_or_else(|_| "/run/user/1000".to_string()) + "/stardust-0";
    
    println!("Attempting to connect to: {}", socket_path);
    
    match UnixStream::connect(&socket_path).await {
        Ok(_stream) => {
            println!("✓ Successfully connected to socket!");
        }
        Err(e) => {
            println!("✗ Failed to connect: {}", e);
        }
    }
}
