# Starworld (StardustXR + Overte client)

## Rust bridge (optional)
This project can load a Rust bridge shared library exposing a C ABI to the StardustXR client. Build it with:

```bash
cd bridge
cargo build
```

This produces `bridge/target/debug/libstardust_bridge.so`. The app will try to load it automatically at startup. You can also set an explicit path:

```bash
export STARWORLD_BRIDGE_PATH=./bridge/target/debug/libstardust_bridge.so
```

If the bridge is not present, the app falls back to a stub and (previously) attempted raw sockets; with the bridge present it will initialize via the official client crates.

## Overte
Overte connectivity is optional; if unreachable, the client runs in offline mode and logs a warning.

## CLI
- `--socket=/path/to.sock` (legacy attempt)
- `--abstract=name` (legacy abstract socket attempt)

Prefer using the Rust bridge.

## Vendoring StardustXR crates (recommended for deep integration)
For fuller control and inspection of the StardustXR client pipeline, clone the `asteroids` and `core` (fusion) repositories into `third_party/` and switch the bridge's `Cargo.toml` to `path` dependencies. See `third_party/README.md` for details.

Benefits:
- Deterministic builds (no moving `dev` branch)
- Ability to patch or instrument crate internals without forking remote
- Easier to expose new C ABI functions (input, health queries, node data extraction)

After vendoring:
```bash
sed -i 's/git = .*/path = "..\/third_party\/asteroids"/' bridge/Cargo.toml
sed -i 's/git = .*/path = "..\/third_party\/core"/' bridge/Cargo.toml
cargo build -p stardust_bridge
```

If you do not vendor, please provide commit SHAs to pin; I can update `Cargo.toml` accordingly.
