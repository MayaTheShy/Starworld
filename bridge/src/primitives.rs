use std::path::PathBuf;
use std::fs;

// Embedded GLTF primitives for basic shapes
pub mod embedded_models {
    use super::*;
    use std::sync::OnceLock;
    
    static CACHE_DIR: OnceLock<PathBuf> = OnceLock::new();
    
    pub fn get_cache_dir() -> &'static PathBuf {
        CACHE_DIR.get_or_init(|| {
            let dir = dirs::cache_dir()
                .unwrap_or_else(|| PathBuf::from("/tmp"))
                .join("starworld/primitives");
            let _ = std::fs::create_dir_all(&dir);
            dir
        })
    }
    
    pub fn get_cube_model() -> PathBuf {
        let path = get_cache_dir().join("cube.glb");
        if !path.exists() {
            std::fs::write(&path, CUBE_GLB).expect("Failed to write cube.glb");
        }
        path
    }
    
    pub fn get_sphere_model() -> PathBuf {
        let path = get_cache_dir().join("sphere.glb");
        if !path.exists() {
            std::fs::write(&path, SPHERE_GLB).expect("Failed to write sphere.glb");
        }
        path
    }
    
    // Minimal cube GLB (binary GLTF) - this is a placeholder
    // TODO: Generate proper GLB data or bundle actual primitive files
    const CUBE_GLB: &[u8] = b""; // Will be filled with actual data
    const SPHERE_GLB: &[u8] = b""; // Will be filled with actual data
}
