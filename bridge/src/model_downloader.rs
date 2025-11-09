// Model downloader for fetching GLTF/GLB models from URLs

use std::fs;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use std::io::Write;

/// Computes a simple hash of a URL for cache filenames
fn url_hash(url: &str) -> String {
    // Simple hash: use last path component + length as identifier
    // In production, use a proper hash like SHA256
    let sanitized = url.replace(['/', ':', '?', '&', '='], "_");
    let len = url.len();
    format!("{:x}_{}", len, sanitized.chars().rev().take(32).collect::<String>())
}

/// Model cache entry
#[derive(Clone)]
struct CacheEntry {
    path: PathBuf,
    downloading: bool,
}

/// Downloads and caches 3D models from HTTP URLs
pub struct ModelDownloader {
    cache_dir: PathBuf,
    cache: Arc<Mutex<HashMap<String, CacheEntry>>>,
    client: reqwest::blocking::Client,
}

impl ModelDownloader {
    /// Create a new model downloader with the given cache directory
    pub fn new(cache_dir: PathBuf) -> Result<Self, std::io::Error> {
        fs::create_dir_all(&cache_dir)?;
        
        let client = reqwest::blocking::Client::builder()
            .timeout(std::time::Duration::from_secs(30))
            .build()
            .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))?;
        
        Ok(Self {
            cache_dir,
            cache: Arc::new(Mutex::new(HashMap::new())),
            client,
        })
    }
    
    /// Get a model from URL, downloading if necessary
    /// Returns PathBuf if available, None if downloading or failed
    pub fn get_model(&self, url: &str) -> Option<PathBuf> {
        // Check cache first
        {
            let cache = self.cache.lock().ok()?;
            if let Some(entry) = cache.get(url) {
                if entry.downloading {
                    eprintln!("[downloader] Model still downloading: {}", url);
                    return None;
                }
                if entry.path.exists() {
                    return Some(entry.path.clone());
                }
            }
        }
        
        // Determine file extension from URL
        let extension = if url.ends_with(".glb") {
            "glb"
        } else if url.ends_with(".gltf") {
            "gltf"
        } else if url.ends_with(".vrm") {
            "vrm"
        } else {
            // Default to GLB
            eprintln!("[downloader] Unknown extension for {}, assuming .glb", url);
            "glb"
        };
        
        let hash = url_hash(url);
        let filename = format!("{}.{}", hash, extension);
        let dest_path = self.cache_dir.join(&filename);
        
        // Check if already on disk
        if dest_path.exists() {
            eprintln!("[downloader] Found cached model: {}", dest_path.display());
            let mut cache = self.cache.lock().ok()?;
            cache.insert(url.to_string(), CacheEntry {
                path: dest_path.clone(),
                downloading: false,
            });
            return Some(dest_path);
        }
        
        // Mark as downloading
        {
            let mut cache = self.cache.lock().ok()?;
            cache.insert(url.to_string(), CacheEntry {
                path: dest_path.clone(),
                downloading: true,
            });
        }
        
        eprintln!("[downloader] Downloading model: {}", url);
        
        // Download in current thread (blocking)
        match self.download_file(url, &dest_path) {
            Ok(_) => {
                eprintln!("[downloader] Downloaded successfully: {}", dest_path.display());
                let mut cache = self.cache.lock().ok()?;
                cache.insert(url.to_string(), CacheEntry {
                    path: dest_path.clone(),
                    downloading: false,
                });
                Some(dest_path)
            }
            Err(e) => {
                eprintln!("[downloader] Failed to download {}: {}", url, e);
                // Remove from cache on failure
                let mut cache = self.cache.lock().ok()?;
                cache.remove(url);
                None
            }
        }
    }
    
    /// Download a file from URL to destination path
    fn download_file(&self, url: &str, dest: &PathBuf) -> Result<(), Box<dyn std::error::Error>> {
        let response = self.client.get(url).send()?;
        
        if !response.status().is_success() {
            return Err(format!("HTTP {}", response.status()).into());
        }
        
        let bytes = response.bytes()?;
        
        // Create temporary file first
        let temp_path = dest.with_extension("tmp");
        let mut file = fs::File::create(&temp_path)?;
        file.write_all(&bytes)?;
        file.sync_all()?;
        drop(file);
        
        // Rename to final destination
        fs::rename(&temp_path, dest)?;
        
        Ok(())
    }
    
    /// Clear the download cache
    #[allow(dead_code)]
    pub fn clear_cache(&self) -> Result<(), std::io::Error> {
        let mut cache = self.cache.lock().unwrap();
        cache.clear();
        
        if self.cache_dir.exists() {
            fs::remove_dir_all(&self.cache_dir)?;
            fs::create_dir_all(&self.cache_dir)?;
        }
        
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_url_hash() {
        let hash1 = url_hash("https://example.com/model.glb");
        let hash2 = url_hash("https://example.com/model.glb");
        let hash3 = url_hash("https://example.com/other.glb");
        
        assert_eq!(hash1, hash2);
        assert_ne!(hash1, hash3);
    }
}
