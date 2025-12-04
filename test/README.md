# nhssta Test Suite

This directory contains the test suite for nhssta, using Google Test framework.

## Test Structure

### Unit Tests - Core SSTA
- `test_randomvariable.cpp` - Tests for RandomVariable base class
- `test_normal.cpp` - Tests for Normal distribution class
- `test_add.cpp` - Tests for addition operation (ADD)
- `test_max.cpp` - Tests for maximum operation (MAX)
- `test_max_order.cpp` - Tests for MAX operation order independence
- `test_covariance.cpp` - Tests for covariance calculations
- `test_order_dependency.cpp` - Tests for covariance computation order independence
- `test_small_rho.cpp` - Tests for small correlation coefficient behavior

### Unit Tests - Expression (自動微分・感度解析用)
- `test_expression_print.cpp` - Expression の表示機能テスト
- `test_expression_assert_migration.cpp` - Expression の算術演算テスト
- `test_expression_derivative.cpp` - 逆伝播による勾配計算テスト

> **Note**: Expression クラスは逆伝播（Reverse-mode AD）による自動微分を実装しています。
> SSTA コアとの統合は将来の課題です。詳細は `src/expression.hpp` を参照。

### Integration Tests
- `test_integration.cpp` - Integration tests based on example/nhssta_test

## Running Tests

### Prerequisites
- **C++17対応のコンパイラ**（g++ 7.0以上、clang++ 5.0以上）
- **Google Test** をインストール:
  - macOS: `brew install googletest`
  - Ubuntu: `sudo apt-get install libgtest-dev`

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

**Phase 1 (Modern C++ Migration)**: ✅ Complete
- Boost dependencies removed
- All 351 tests passing

## Current Test Coverage

- **781 tests** across **70 test suites**
- Unit tests for core components (RandomVariable, Gate, Parser, Ssta)
- Expression tests (将来の感度解析機能用)
- Integration tests for end-to-end functionality
- Performance benchmarks
- Code coverage measurement support

## Future Improvements

- Expand test coverage for edge cases
- Add more performance benchmarks
- Enhance integration test scenarios

