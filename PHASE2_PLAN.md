# Phase 2: Expression Integration & Code Deduplication

**Status**: Planning
**Started**: 2025-12-24
**Branch**: refactoring-analysis

## Goal

Complete Phase C-5 of the Expression integration by unifying `covariance()` and `cov_expr()` implementations, eliminating ~300 lines of duplicate code.

## Current State Analysis

### Code Duplication

```
covariance.cpp:          626 lines
covariance_context.cpp:  534 lines
Total:                  1160 lines

Duplicate logic branches: 26 dynamic_cast checks
Estimated duplicate code: ~300-350 lines
```

### Duplicate Pattern

Both `covariance()` (line 532-607) and `cov_expr()` (line 448-529) implement the same dispatch logic:

```cpp
// covariance() - returns double
if (a == b) {
    cov = a->variance();
} else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
    double cov0 = covariance(a->left(), b);
    double cov1 = covariance(a->right(), b);
    cov = cov0 + cov1;
} // ... 12 more branches

// cov_expr() - returns Expression
if (a == b) {
    result = a->var_expr();
} else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
    Expression cov_left = cov_expr(a->left(), b);
    Expression cov_right = cov_expr(a->right(), b);
    result = cov_left + cov_right;
} // ... 12 more branches (same logic!)
```

### Problem

1. **Maintenance burden**: Bug fixes must be applied twice
2. **Logic drift risk**: Changes in one may not reflect in the other
3. **Testing overhead**: Need to test both paths separately

## Proposed Solution

### Option A: Unified via Expression (Recommended)

Implement `covariance()` using `cov_expr()`:

```cpp
double covariance(const RandomVariable& a, const RandomVariable& b) {
    if (active_context_ != nullptr) {
        return active_context_->covariance(a, b);
    }

    // Use Expression-based implementation
    Expression expr = cov_expr(a, b);
    return expr->value();
}
```

**Pros**:
- Single source of truth
- Phase C-5 completion
- ~300 lines of code removed
- Consistent behavior guaranteed

**Cons**:
- Potential performance overhead (Expression evaluation)
- Need to verify numerical accuracy
- Slightly more memory usage (Expression tree)

### Option B: Keep Both Implementations

Maintain current dual implementation.

**Pros**:
- No risk of performance regression
- Clear separation of concerns

**Cons**:
- Ongoing maintenance burden
- Risk of logic drift
- More code to test

## Implementation Plan

### Step 1: Create Performance Baseline

```bash
# Measure current performance
./build/bin/nhssta -l -c -p -d example/gaussdelay.dlib -b example/s820.bench

# Record:
# - Execution time
# - Memory usage
# - Numerical accuracy
```

### Step 2: Write Verification Tests

Create tests to ensure Expression-based covariance produces identical results:

```cpp
TEST(CovarianceUnificationTest, ExpressionEquivalence) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // Direct calculation
    double cov_direct = covariance_direct(a, sum);

    // Expression-based calculation
    Expression expr = cov_expr(a, sum);
    double cov_expr_val = expr->value();

    // Must match within numerical tolerance
    EXPECT_NEAR(cov_direct, cov_expr_val, 1e-10);
}
```

### Step 3: Implement Unified Covariance

1. Add `covariance_direct()` function (backup of original logic)
2. Modify `covariance()` to use `cov_expr()`
3. Run all tests
4. Compare performance benchmarks

### Step 4: Performance Validation

**Acceptance Criteria**:
- All 805 tests pass
- Performance degradation < 5%
- Numerical accuracy matches (within 1e-10)
- Memory increase < 10%

**If acceptance criteria not met**:
- Revert to Option B (keep both implementations)
- Or optimize Expression evaluation

### Step 5: Code Cleanup

If validation passes:
1. Remove duplicate `covariance()` implementation
2. Mark Phase C-5 as complete
3. Update documentation

## Risk Assessment

### High Risk Items

1. **Performance Regression**
   - Mitigation: Benchmark before/after
   - Fallback: Revert if > 5% slower

2. **Numerical Accuracy**
   - Mitigation: Comprehensive numerical tests
   - Fallback: Keep both implementations if issues found

3. **Memory Usage**
   - Mitigation: Profile memory usage
   - Optimization: Expression tree pruning if needed

### Medium Risk Items

1. **Test Coverage**
   - Mitigation: Add tests for edge cases
   - Action: Review all covariance test cases

2. **Expression Caching**
   - Risk: Cache may grow too large
   - Mitigation: Monitor cache size, add limits

## Success Metrics

- [ ] All 805 tests pass
- [ ] 10 integration tests pass
- [ ] Performance within 5% of baseline
- [ ] ~300 lines of code removed
- [ ] Single source of truth for covariance logic
- [ ] Documentation updated

## Timeline

- **Step 1-2**: 1 session (Baseline + Tests)
- **Step 3**: 1 session (Implementation)
- **Step 4**: 1 session (Validation)
- **Step 5**: 1 session (Cleanup)

Total: ~4 sessions

## Decision Point

Before proceeding with implementation, we need to:

1. ✅ Create performance baseline
2. ✅ Write verification tests
3. ⚠️ Get user approval for approach

**Recommendation**: Proceed with Option A (Unified via Expression) if:
- Performance impact < 5%
- All tests pass
- Numerical accuracy maintained

Otherwise, document findings and keep Option B.

## Findings (2025-12-24)

### Test Results

Created `test_covariance_unification.cpp` with 19 comprehensive tests comparing double-based and Expression-based covariance computations.

**Results**: 824/824 tests pass ✅

### Numerical Precision Analysis

| Operation Type | Tolerance Required | Notes |
|---------------|-------------------|-------|
| Linear (ADD, SUB) | 1e-10 | Exact match, high precision |
| MAX operations | 1e-7 | Small differences due to Clark's formula application |
| MAX0 operations | 2e-3 | Gauss-Hermite quadrature differences |
| MAX0×MAX0 (independent) | 0.01 | **Algorithmic difference discovered** |

### Critical Finding: MAX0×MAX0 Algorithmic Difference

**Root Cause**: The double and Expression versions use fundamentally different algorithms for computing E[D0⁺ D1⁺]:

1. **Double version** (`util_numerical.cpp:339`): Uses Gauss-Hermite quadrature
   - For independent variables (ρ=0): Returns ~15.048 for E[a⁺ b⁺]
   - Expected product: 15.055 (E[a⁺] × E[b⁺])
   - Covariance error: -0.007 (numerical approximation error)

2. **Expression version** (`statistical_functions.cpp:181`): Uses exact analytical formula
   ```
   E[D0⁺ D1⁺] = μ0 μ1 Φ₂(a0, a1; ρ)
               + μ0 σ1 φ(a1) Φ((a0 - ρa1)/√(1-ρ²))
               + μ1 σ0 φ(a0) Φ((a1 - ρa0)/√(1-ρ²))
               + σ0 σ1 [ρ Φ₂(a0, a1; ρ) + (1-ρ²) φ₂(a0, a1; ρ)]
   ```
   - For independent variables (ρ=0): Correctly returns 0
   - This is mathematically exact

**Implication**: The Expression version is **more accurate** for MAX0×MAX0 computations.

### Performance Impact

Performance baseline: 2.111s for s820.bench (not yet tested with unified implementation)

### Decision

**Status**: ✅ **DECIDED - Option B (Keep Separate Implementations)**

### Performance Measurement Results (2025-12-24)

**Benchmark**: s820.bench with gaussdelay.dlib (-l -c -p)

| Implementation | Run 1 | Run 2 | Run 3 | Average | vs Baseline |
|---------------|-------|-------|-------|---------|-------------|
| **Baseline (Separate)** | 2.46s | 1.80s | 1.80s | **2.02s** | - |
| **Option A (Unified)** | 21.14s | 19.73s | 19.63s | **20.2s** | **+1000%** ❌ |

**Conclusion**: Option A causes **10x performance degradation** (1000%), far exceeding the 5% acceptance threshold.

### Final Decision: Option B (Keep Separate Implementations)

#### Reasons for Rejection of Option A:
1. ❌ **Unacceptable performance impact**: 10x slower (1000% vs 5% threshold)
2. ❌ **Expression tree overhead**: Constant cov_expr() calls create/evaluate Expression trees
3. ❌ **Cache inefficiency**: Expression value() evaluation is expensive

#### Option B Benefits (Retained):
- ✅ **Performance preserved**: No degradation
- ✅ **Stability**: No behavioral changes
- ✅ **Battle-tested**: Current implementation is well-validated
- ⚠️ **Trade-off accepted**: Maintenance burden of ~300 duplicate lines

#### Future Optimization Opportunities:
If performance can be improved to <5% impact:
1. Optimize Expression tree creation (lazy evaluation)
2. Improve cov_expr_cache effectiveness
3. Fast-path for simple operations (Normal × Normal)
4. Profile and optimize Expression::value() evaluation

**For now, duplicate code is the lesser evil compared to 10x performance loss.**

## Notes

- This is Phase C-5 completion (mentioned in code comments)
- Expression tree already implemented and tested
- CovarianceContext already uses this pattern internally
- Main risk is performance, which we can measure objectively
- **NEW**: Expression version provides higher accuracy for MAX0×MAX0 operations
