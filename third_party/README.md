# Third-Party Crates

To enable deeper integration and inspection of StardustXR client behavior, place local clones of the following repositories here:

- `asteroids/` (https://github.com/StardustXR/asteroids)
- `core/` (https://github.com/StardustXR/core) — provides fusion client elements

Recommended structure:
```
third_party/
  asteroids/
  core/
```
After cloning, you can update `bridge/Cargo.toml` to use `path` dependencies instead of `git` to ensure reproducible builds and easier iteration:

```
[dependencies.stardust-xr-asteroids]
path = "../third_party/asteroids"

[dependencies.stardust-xr-fusion]
path = "../third_party/core"
```
Then run:

```bash
cargo update
cargo build -p stardust_bridge
```

This lets us:
- Inspect and modify client crate code during debugging
- Pin exact revisions without relying on remote branches
- Potentially implement custom elements or expose more C ABI hooks

If you prefer not to vendor the crates, please confirm the exact commit SHAs you want pinned and we can lock them in `Cargo.toml` instead of the moving `dev` branch.
