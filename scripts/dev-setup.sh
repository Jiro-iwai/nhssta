#!/bin/bash
# Development environment setup script for nhssta
# Usage: ./scripts/dev-setup.sh
#
# This script helps new developers set up their development environment
# by checking prerequisites and providing guidance.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "=== nhssta Development Environment Setup ==="
echo ""

# Check for C++ compiler
echo "Checking C++ compiler..."
if command -v g++ >/dev/null 2>&1; then
    GCC_VERSION=$(g++ --version | head -n1)
    echo "  ✓ Found: $GCC_VERSION"
elif command -v clang++ >/dev/null 2>&1; then
    CLANG_VERSION=$(clang++ --version | head -n1)
    echo "  ✓ Found: $CLANG_VERSION"
else
    echo "  ✗ Error: No C++ compiler found"
    echo "    Install with:"
    echo "      macOS: Xcode Command Line Tools (xcode-select --install)"
    echo "      Ubuntu: sudo apt-get install build-essential"
    exit 1
fi

# Check for Google Test
echo ""
echo "Checking Google Test..."
GTEST_DIR="${GTEST_DIR:-/opt/homebrew/opt/googletest}"
if [ -d "$GTEST_DIR/include" ] && [ -d "$GTEST_DIR/lib" ]; then
    echo "  ✓ Found Google Test at: $GTEST_DIR"
elif [ -d "/usr/local/opt/googletest/include" ] && [ -d "/usr/local/opt/googletest/lib" ]; then
    echo "  ✓ Found Google Test at: /usr/local/opt/googletest"
    echo "    Consider setting: export GTEST_DIR=/usr/local/opt/googletest"
elif [ -d "/usr/include/gtest" ] && [ -d "/usr/lib" ]; then
    echo "  ✓ Found Google Test in system directories"
    echo "    Consider setting: export GTEST_DIR=/usr"
else
    echo "  ⚠ Google Test not found in common locations"
    echo "    Install with:"
    echo "      macOS: brew install googletest"
    echo "      Ubuntu: sudo apt-get install libgtest-dev"
    echo ""
    echo "    If installed elsewhere, set GTEST_DIR environment variable:"
    echo "      export GTEST_DIR=/path/to/googletest"
fi

# Check for clang-format
echo ""
echo "Checking clang-format..."
if command -v clang-format >/dev/null 2>&1; then
    CLANG_FORMAT_VERSION=$(clang-format --version | head -n1)
    echo "  ✓ Found: $CLANG_FORMAT_VERSION"
else
    echo "  ⚠ clang-format not found (optional but recommended)"
    echo "    Install with:"
    echo "      macOS: brew install llvm"
    echo "      Ubuntu: sudo apt-get install clang-format"
fi

# Check for clang-tidy
echo ""
echo "Checking clang-tidy..."
if command -v clang-tidy >/dev/null 2>&1; then
    CLANG_TIDY_VERSION=$(clang-tidy --version | head -n1)
    echo "  ✓ Found: $CLANG_TIDY_VERSION"
else
    echo "  ⚠ clang-tidy not found (optional but recommended)"
    echo "    Install with:"
    echo "      macOS: brew install llvm"
    echo "      Ubuntu: sudo apt-get install clang-tidy"
fi

# Check for lcov (for coverage)
echo ""
echo "Checking lcov (for code coverage)..."
if command -v lcov >/dev/null 2>&1; then
    LCOV_VERSION=$(lcov --version | head -n1)
    echo "  ✓ Found: $LCOV_VERSION"
else
    echo "  ⚠ lcov not found (optional, for code coverage)"
    echo "    Install with:"
    echo "      macOS: brew install lcov"
    echo "      Ubuntu: sudo apt-get install lcov"
fi

# Try to build
echo ""
echo "Attempting to build nhssta..."
cd src
if make clean >/dev/null 2>&1 && make >/dev/null 2>&1; then
    echo "  ✓ Build successful"
else
    echo "  ⚠ Build failed (this may be expected if dependencies are missing)"
    echo "    Run 'make' manually to see detailed error messages"
fi

# Try to build tests
echo ""
echo "Attempting to build tests..."
if make test-build >/dev/null 2>&1; then
    echo "  ✓ Test build successful"
else
    echo "  ⚠ Test build failed"
    echo "    Make sure GTEST_DIR is set correctly if Google Test is installed"
    echo "    Run 'make test-build' manually to see detailed error messages"
fi

echo ""
echo "=== Setup Check Complete ==="
echo ""
echo "Next steps:"
echo "  1. Run tests: make test"
echo "  2. Run all checks: ./scripts/run-all-checks.sh"
echo "  3. See README.md for more information"

cd "$PROJECT_ROOT"

