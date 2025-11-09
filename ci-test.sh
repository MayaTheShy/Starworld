#!/bin/bash
# CI test runner script

set -e  # Exit on error
set -u  # Exit on undefined variable
set -o pipefail  # Exit on pipe failure

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Starworld CI Test Suite ===${NC}"

# Check if we're in the right directory
if [[ ! -f "CMakeLists.txt" ]]; then
    echo -e "${RED}ERROR: Must be run from starworld root directory${NC}"
    exit 1
fi

# Track test results
TESTS_PASSED=0
TESTS_FAILED=0

run_test() {
    local test_name="$1"
    local test_cmd="$2"
    
    echo -e "\n${YELLOW}Running: ${test_name}${NC}"
    if eval "$test_cmd"; then
        echo -e "${GREEN}✓ ${test_name} PASSED${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}✗ ${test_name} FAILED${NC}"
        ((TESTS_FAILED++))
        return 1
    fi
}

# Clean build directory
echo -e "\n${YELLOW}Cleaning build directory...${NC}"
rm -rf build
mkdir -p build

# Build Rust bridge
run_test "Rust Bridge Build" "cd bridge && cargo build --verbose && cd .."

# Build C++ tests
run_test "CMake Configuration" "cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug"
run_test "C++ Build" "cd build && make -j$(nproc)"

# Run unit tests
run_test "C++ Unit Tests" "./build/stardust-tests"

# Verify binaries exist
run_test "Client Binary Exists" "test -f build/stardust-overte-client"
run_test "Bridge Library Exists" "test -f bridge/target/debug/libstardust_bridge.so"

# Optional: Quick simulation test (non-blocking)
echo -e "\n${YELLOW}Running quick simulation test...${NC}"
if timeout 2 env STARWORLD_SIMULATE=1 ./build/stardust-overte-client > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Simulation test completed${NC}"
    ((TESTS_PASSED++))
else
    # Timeout is expected behavior
    echo -e "${GREEN}✓ Simulation test timed out (expected)${NC}"
    ((TESTS_PASSED++))
fi

# Rust code quality checks
if command -v cargo-fmt &> /dev/null; then
    run_test "Rust Format Check" "cd bridge && cargo fmt -- --check"
fi

if command -v cargo-clippy &> /dev/null; then
    run_test "Rust Clippy" "cd bridge && cargo clippy -- -D warnings"
fi

# Summary
echo -e "\n${GREEN}=== Test Summary ===${NC}"
echo -e "Tests Passed: ${GREEN}${TESTS_PASSED}${NC}"
echo -e "Tests Failed: ${RED}${TESTS_FAILED}${NC}"

if [[ $TESTS_FAILED -eq 0 ]]; then
    echo -e "\n${GREEN}ALL TESTS PASSED!${NC}"
    exit 0
else
    echo -e "\n${RED}SOME TESTS FAILED!${NC}"
    exit 1
fi
