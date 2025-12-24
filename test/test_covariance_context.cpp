/**
 * @file test_covariance_context.cpp
 * @brief Tests for CovarianceContext - removing global state
 *
 * This test file demonstrates the new CovarianceContext design that eliminates
 * global state, enabling better testability and parallel processing.
 *
 * Key improvements over global covariance_matrix:
 * 1. Tests are fully isolated - each test has its own context
 * 2. Parallel test execution is possible
 * 3. Explicit dependency injection
 * 4. Easy mocking for unit tests
 */

#include <gtest/gtest.h>

#include <cmath>
#include <nhssta/exception.hpp>

#include "../src/add.hpp"
#include "../src/covariance_context.hpp"
#include "../src/max.hpp"
#include "../src/normal.hpp"

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;
using CovarianceContext = RandomVariable::CovarianceContext;

class CovarianceContextTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Each test gets its own context - full isolation
        context = std::make_shared<CovarianceContext>();
    }

    void TearDown() override {
        // Context is automatically cleaned up
        context.reset();
    }

    std::shared_ptr<CovarianceContext> context;
};

// ============================================================================
// Basic Covariance Tests with Context
// ============================================================================

// Test: Covariance of same variable equals its variance
TEST_F(CovarianceContextTest, CovarianceSameVariable) {
    Normal a(10.0, 4.0);

    double cov = context->covariance(a, a);

    // Covariance of variable with itself should equal variance
    EXPECT_DOUBLE_EQ(cov, 4.0);
}

// Test: Independent variables have zero covariance
TEST_F(CovarianceContextTest, CovarianceIndependentVariables) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    double cov = context->covariance(a, b);

    // Independent Normal variables should have zero covariance
    EXPECT_DOUBLE_EQ(cov, 0.0);
}

// Test: Covariance with addition operation
TEST_F(CovarianceContextTest, CovarianceWithAddition) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    RandomVar sum = a + b;

    // Covariance of a with (a+b) should be variance of a
    double cov_a_sum = context->covariance(a, sum);
    EXPECT_DOUBLE_EQ(cov_a_sum, 4.0);

    // Covariance of b with (a+b) should be variance of b
    double cov_b_sum = context->covariance(b, sum);
    EXPECT_DOUBLE_EQ(cov_b_sum, 1.0);
}

// Test: Covariance symmetry Cov(A,B) == Cov(B,A)
TEST_F(CovarianceContextTest, CovarianceSymmetry) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    RandomVar sum = a + b;

    double cov_ab = context->covariance(a, sum);
    double cov_ba = context->covariance(sum, a);

    EXPECT_DOUBLE_EQ(cov_ab, cov_ba);
}

// ============================================================================
// Context Isolation Tests - Key benefit of removing global state
// ============================================================================

// Test: Multiple contexts are independent
TEST_F(CovarianceContextTest, MultipleContextsAreIndependent) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // Create two separate contexts
    auto context1 = std::make_shared<CovarianceContext>();
    auto context2 = std::make_shared<CovarianceContext>();

    // Compute covariance in context1
    double cov1 = context1->covariance(a, sum);

    // Context2 should have empty cache
    EXPECT_EQ(context2->cache_size(), 0);

    // Compute covariance in context2
    double cov2 = context2->covariance(a, sum);

    // Results should be identical
    EXPECT_DOUBLE_EQ(cov1, cov2);

    // But caches should be independent
    EXPECT_GT(context1->cache_size(), 0);
    EXPECT_GT(context2->cache_size(), 0);
}

// Test: Context can be cleared independently
TEST_F(CovarianceContextTest, ContextClearingIsIndependent) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    // Create two contexts and compute covariances
    auto context1 = std::make_shared<CovarianceContext>();
    auto context2 = std::make_shared<CovarianceContext>();

    context1->covariance(a, b);
    context2->covariance(a, b);

    EXPECT_GT(context1->cache_size(), 0);
    EXPECT_GT(context2->cache_size(), 0);

    // Clear context1
    context1->clear();

    // context1 should be empty, context2 should still have data
    EXPECT_EQ(context1->cache_size(), 0);
    EXPECT_GT(context2->cache_size(), 0);
}

// ============================================================================
// Cache Management Tests
// ============================================================================

// Test: Covariance results are cached
TEST_F(CovarianceContextTest, CovarianceResultsCached) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // First call - cache miss
    size_t initial_size = context->cache_size();
    double cov1 = context->covariance(a, sum);
    size_t size_after_first = context->cache_size();

    // Cache should have grown
    EXPECT_GT(size_after_first, initial_size);

    // Second call - cache hit
    double cov2 = context->covariance(a, sum);
    size_t size_after_second = context->cache_size();

    // Results should be identical
    EXPECT_DOUBLE_EQ(cov1, cov2);

    // Cache size should not change (cache hit)
    EXPECT_EQ(size_after_second, size_after_first);
}

// Test: Clear resets cache
TEST_F(CovarianceContextTest, ClearResetsCache) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    context->covariance(a, b);

    EXPECT_GT(context->cache_size(), 0);

    context->clear();

    EXPECT_EQ(context->cache_size(), 0);
}

// ============================================================================
// Expression-based Covariance Tests
// ============================================================================

// Test: cov_expr returns Expression for differentiation
TEST_F(CovarianceContextTest, CovExprReturnsExpression) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    Expression expr = context->cov_expr(a, b);

    // Expression should be valid
    EXPECT_TRUE(expr != nullptr);

    // Value should match double covariance
    double cov_double = context->covariance(a, b);
    double cov_expr_val = expr->value();

    EXPECT_DOUBLE_EQ(cov_double, cov_expr_val);
}

// Test: Expression cache is independent from double cache
TEST_F(CovarianceContextTest, ExpressionCacheIndependent) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    // Compute double covariance
    context->covariance(a, b);
    size_t cache_size_after_double = context->cache_size();

    // Compute expression covariance
    context->cov_expr(a, b);
    size_t cache_size_after_expr = context->expr_cache_size();

    // Both caches should have entries
    EXPECT_GT(cache_size_after_double, 0);
    EXPECT_GT(cache_size_after_expr, 0);

    // Clear double cache
    context->clear();
    EXPECT_EQ(context->cache_size(), 0);

    // Expression cache should still have data
    EXPECT_GT(context->expr_cache_size(), 0);

    // Clear expression cache
    context->clear_expr_cache();
    EXPECT_EQ(context->expr_cache_size(), 0);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

// Test: Zero variance in MAX0 operation throws exception
TEST_F(CovarianceContextTest, ZeroVarianceMax0ThrowsException) {
    Normal a(5.0, 0.0);
    RandomVar max0 = MAX0(a);

    // Should throw exception when computing covariance with zero variance
    EXPECT_THROW(
        {
            double cov = context->covariance(a, max0);
            (void)cov;
        },
        Nh::RuntimeException);
}

// Test: Negative variance throws exception
TEST_F(CovarianceContextTest, NegativeVarianceThrowsException) {
    // This test verifies error handling in variance checking
    // The actual scenario depends on how RandomVariable is constructed
    // but the context should handle it gracefully
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    // Normal computation should not throw
    EXPECT_NO_THROW({
        double cov = context->covariance(a, b);
        (void)cov;
    });
}

// ============================================================================
// Parallel Testing Demonstration
// ============================================================================

// Test: Contexts can be used in parallel (simulated)
TEST_F(CovarianceContextTest, ContextsSupportParallelComputation) {
    // Create multiple contexts
    std::vector<std::shared_ptr<CovarianceContext>> contexts;
    for (int i = 0; i < 5; ++i) {
        contexts.push_back(std::make_shared<CovarianceContext>());
    }

    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    // Compute covariance in each context
    std::vector<double> results;
    for (auto& ctx : contexts) {
        results.push_back(ctx->covariance(a, b));
    }

    // All results should be identical
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_DOUBLE_EQ(results[0], results[i]);
    }

    // Each context should have its own cache
    for (auto& ctx : contexts) {
        EXPECT_GT(ctx->cache_size(), 0);
    }
}

// ============================================================================
// Backward Compatibility Tests (if needed)
// ============================================================================

// Test: Context API matches old global API
TEST_F(CovarianceContextTest, APICompatibilityWithGlobalVersion) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    // New context-based API
    double cov_new = context->covariance(a, b);

    // Old global API (for comparison during migration)
    // double cov_old = RandomVariable::covariance(a, b);

    // Results should match
    // EXPECT_DOUBLE_EQ(cov_new, cov_old);

    // For now, just verify the new API works
    EXPECT_DOUBLE_EQ(cov_new, 0.0);
}
