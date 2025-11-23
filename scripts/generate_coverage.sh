#!/bin/bash
# Generate code coverage report for nhssta
# Usage: ./scripts/generate_coverage.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "=== Generating Code Coverage Report ==="
echo ""

# Check if gcov is available
if ! command -v gcov >/dev/null 2>&1; then
    echo "Error: gcov is not installed"
    echo "Install with: brew install gcc (macOS) or apt-get install gcc (Ubuntu)"
    exit 1
fi

# Clean previous coverage data
echo "Cleaning previous coverage data..."
make coverage-clean

# Build with coverage flags
echo "Building with coverage instrumentation..."
cd src
make coverage

# Check if lcov is available
if command -v lcov >/dev/null 2>&1; then
    echo ""
    echo "=== Coverage Summary ==="
    lcov --summary coverage/coverage.info 2>/dev/null || true
    echo ""
    echo "HTML report generated in: coverage/html/index.html"
    echo "Open with: open coverage/html/index.html (macOS) or xdg-open coverage/html/index.html (Linux)"
else
    echo ""
    echo "Note: lcov is not installed. Install for HTML reports:"
    echo "  macOS: brew install lcov"
    echo "  Ubuntu: apt-get install lcov"
    echo ""
    echo "Coverage .gcov files are available in src/ directory"
fi

cd "$PROJECT_ROOT"

