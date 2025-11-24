#!/bin/bash
# Run all development checks for nhssta
# Usage: ./scripts/run-all-checks.sh
#
# This script runs all standard development checks:
# - Build
# - Tests
# - Linting (clang-tidy)
# - Code coverage (if available)
#
# Exit codes:
#   0: All checks passed
#   1: One or more checks failed

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Track overall success
OVERALL_SUCCESS=0

# Function to print section header
print_section() {
    echo ""
    echo "=========================================="
    echo "$1"
    echo "=========================================="
}

# Function to print success
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Function to print failure
print_failure() {
    echo -e "${RED}✗ $1${NC}"
    OVERALL_SUCCESS=1
}

# Function to print warning
print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# 1. Build
print_section "1. Building nhssta"
cd "$PROJECT_ROOT/src"
if make clean >/dev/null 2>&1 && make >/dev/null 2>&1; then
    print_success "Build successful"
else
    print_failure "Build failed"
    echo "Run 'make' manually to see detailed error messages"
fi

# 2. Run tests
print_section "2. Running tests"
cd "$PROJECT_ROOT/src"
if make test >/dev/null 2>&1; then
    print_success "All tests passed"
    # Show test summary
    if [ -f "../test/nhssta_test" ]; then
        ../test/nhssta_test --gtest_brief=1 2>&1 | tail -3 || true
    elif [ -f "test/nhssta_test" ]; then
        test/nhssta_test --gtest_brief=1 2>&1 | tail -3 || true
    fi
else
    print_failure "Tests failed"
    echo "Run 'make test' manually to see detailed test output"
fi

# 3. Linting (clang-tidy)
print_section "3. Running clang-tidy"
cd "$PROJECT_ROOT/src"

# Find clang-tidy - check standard PATH first, then Homebrew locations
CLANG_TIDY_FOUND=0
if command -v clang-tidy >/dev/null 2>&1; then
    CLANG_TIDY_FOUND=1
elif [ -f "/opt/homebrew/opt/llvm/bin/clang-tidy" ]; then
    # Add Homebrew llvm bin to PATH for this session
    export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
    CLANG_TIDY_FOUND=1
elif [ -f "/usr/local/opt/llvm/bin/clang-tidy" ]; then
    # Add Homebrew llvm bin to PATH for this session (Intel Mac)
    export PATH="/usr/local/opt/llvm/bin:$PATH"
    CLANG_TIDY_FOUND=1
fi

if [ $CLANG_TIDY_FOUND -eq 1 ]; then
    if make tidy >/dev/null 2>&1; then
        print_success "clang-tidy check passed (no critical issues)"
    else
        print_warning "clang-tidy found issues (check output above)"
        # Don't fail overall for clang-tidy warnings
    fi
else
    print_warning "clang-tidy not found, skipping lint check"
    echo "Install with: brew install llvm (macOS) or apt-get install clang-tidy (Ubuntu)"
    echo "Note: If installed via Homebrew, ensure /opt/homebrew/opt/llvm/bin is in PATH"
fi

# 4. Code coverage (optional)
print_section "4. Generating code coverage report"
cd "$PROJECT_ROOT/src"
if command -v gcov >/dev/null 2>&1 && command -v lcov >/dev/null 2>&1; then
    if make coverage >/dev/null 2>&1; then
        print_success "Coverage report generated"
        if [ -f "coverage/html/index.html" ]; then
            echo "  Coverage report: coverage/html/index.html"
        fi
    else
        print_warning "Coverage generation failed (non-critical)"
    fi
else
    print_warning "Coverage tools not available, skipping coverage check"
    echo "Install with: brew install gcc lcov (macOS) or apt-get install gcc lcov (Ubuntu)"
fi

# Summary
echo ""
echo "=========================================="
if [ $OVERALL_SUCCESS -eq 0 ]; then
    echo -e "${GREEN}All checks passed!${NC}"
    exit 0
else
    echo -e "${RED}Some checks failed. See output above for details.${NC}"
    exit 1
fi

cd "$PROJECT_ROOT"

