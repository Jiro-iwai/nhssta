# nhssta Test Suite

This directory contains the test suite for nhssta, using Google Test framework.

## Test Structure

### Unit Tests
- `test_randomvariable.cpp` - Tests for RandomVariable base class
- `test_normal.cpp` - Tests for Normal distribution class
- `test_add.cpp` - Tests for addition operation (ADD)
- `test_max.cpp` - Tests for maximum operation (MAX)
- `test_covariance.cpp` - Tests for covariance calculations

### Integration Tests
- `test_integration.cpp` - Integration tests based on example/nhssta_test

## Running Tests

### Prerequisites
- Google Test must be installed (via Homebrew on macOS: `brew install googletest`)
- Boost libraries (currently required for main program, will be addressed in Phase 1)

### Build and Run Tests

From the project root:
```bash
make test
```

Or from the src directory:
```bash
cd src
make test
```

### Running Individual Test Files

Tests are compiled into a single test binary:
```bash
./test/nhssta_test
```

## Test Status

**Phase 0 (Test Infrastructure)**: ✅ Complete
- Test framework setup (Google Test)
- Unit test structure created
- Integration test structure created
- CI/CD configuration added

**Note**: Currently, tests cannot be fully executed due to Boost dependencies in the main program. This will be addressed in Phase 1 (Modern C++ Migration).

## Future Improvements

- Phase 1: Resolve Boost dependencies to enable full test execution
- Phase 2: Expand test coverage for Gate, Parser, and Ssta classes
- Phase 3: Add performance benchmarks

