# Continuous Integration Setup

This project uses Gitea Actions for continuous integration and testing.

## Workflows

### Main CI Workflow (`.gitea/workflows/ci.yml`)
Runs on every push to `main` or `dev` branches and on pull requests.

**Steps:**
1. Checkout code with submodules
2. Install system dependencies (CMake, GLM, OpenSSL, zlib, curl)
3. Install Rust nightly toolchain
4. Cache build artifacts
5. Build Rust bridge
6. Configure and build C++ project
7. Run unit tests
8. Check Rust code formatting
9. Run Rust clippy for linting
10. Upload build artifacts

### Rust Quality Checks (`.gitea/workflows/rust-quality.yml`)
Runs on changes to `bridge/**` directory.

**Steps:**
1. Format checking with `cargo fmt`
2. Linting with `cargo clippy`
3. Unused dependency detection with `cargo udeps`
4. Security audit with `cargo audit`
5. Documentation build verification

## Local Testing

You can run the full CI test suite locally:

```bash
./ci-test.sh
```

This script:
- Cleans the build directory
- Builds the Rust bridge
- Builds the C++ project
- Runs all tests
- Performs code quality checks

## Test Requirements

### System Dependencies
- Ubuntu 20.04+ or compatible Linux distribution
- CMake 3.15+
- GCC/Clang with C++20 support
- Rust nightly toolchain

### Runtime Dependencies
- libglm-dev
- libssl-dev
- zlib1g-dev
- libcurl4-openssl-dev

## Current Test Coverage

### C++ Tests (`tests/TestHarness.cpp`)
1. **Protocol Signature Stability**: Verifies the Overte protocol version signature
2. **Discovery JSON Parsing (Vircadia)**: Tests parsing domain list with Vircadia field names
3. **Discovery JSON Parsing (Overte)**: Tests parsing domain list with Overte field names

### Rust Bridge Tests
Currently relies on successful compilation. Future additions:
- Unit tests for C ABI functions
- Integration tests with mock StardustXR server
- Property propagation tests

## CI Status Badge

Add this to your Gitea repository README:

```markdown
[![CI](https://git.spatulaa.com/MayaTheShy/Starworld/actions/workflows/ci.yml/badge.svg)](https://git.spatulaa.com/MayaTheShy/Starworld/actions)
```

## Debugging CI Failures

### Build Failures
Check the "Build C++ project" or "Build Rust bridge" step logs:
- Ensure all dependencies are installed
- Check for compilation errors
- Verify submodules are initialized

### Test Failures
Check the "Run tests" step:
- Review test output for specific failures
- Tests validate protocol signatures - may need updating if Overte protocol changes
- JSON parsing tests check both Vircadia and Overte field formats

### Cache Issues
If builds are slow or failing mysteriously:
1. Clear GitHub Actions cache (Repository Settings → Actions → Caches)
2. Re-run the workflow

## Adding New Tests

### C++ Tests
Edit `tests/TestHarness.cpp`:
```cpp
// Add new test case
{
    // Test setup
    bool testPassed = yourTestLogic();
    if (!testPassed) {
        std::cerr << "[FAIL] Your test description\n";
        ++failures;
    }
}
```

### Rust Tests
Add to `bridge/src/lib.rs`:
```rust
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_your_feature() {
        // Your test logic
        assert_eq!(result, expected);
    }
}
```

Run with:
```bash
cd bridge
cargo test
```

## Future Enhancements

- [ ] Add integration tests with mock Overte server
- [ ] Add performance benchmarks
- [ ] Add code coverage reporting
- [ ] Add automated release builds
- [ ] Add container-based testing
- [ ] Add cross-platform testing (macOS, Windows)
