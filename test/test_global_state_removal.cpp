/**
 * @file test_global_state_removal.cpp
 * @brief Tests for Phase 1 (continued): Complete global state removal
 *
 * This test suite verifies that all functionality works without
 * static global state, using thread-local default contexts instead.
 *
 * Key requirements:
 * 1. No static CovarianceMatrix (replaced with thread_local default context)
 * 2. Each thread has its own default context
 * 3. Explicit context via ActiveContextGuard still works
 * 4. Backward compatibility: existing tests work without changes
 */

#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "../src/add.hpp"
#include "../src/covariance.hpp"
#include "../src/covariance_context.hpp"
#include "../src/max.hpp"
#include "../src/normal.hpp"
#include "../src/sub.hpp"

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;
using CovarianceContext = ::RandomVariable::CovarianceContext;
using ActiveContextGuard = ::RandomVariable::ActiveContextGuard;

class GlobalStateRemovalTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Get reference to default context and clear it
        // This simulates what used to be get_covariance_matrix()->clear()
        auto* default_ctx = ::RandomVariable::get_default_context();
        if (default_ctx != nullptr) {
            default_ctx->clear();
            default_ctx->clear_expr_cache();
        }
    }

    void TearDown() override {
        // Clear default context after each test
        auto* default_ctx = ::RandomVariable::get_default_context();
        if (default_ctx != nullptr) {
            default_ctx->clear();
            default_ctx->clear_expr_cache();
        }
    }
};

// ============================================================================
// Basic Functionality Tests (without explicit context)
// ============================================================================

TEST_F(GlobalStateRemovalTest, BasicCovarianceWithoutExplicitContext) {
    // This test verifies that covariance works without setting
    // any explicit context (using thread-local default context)

    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // Should work without any ActiveContextGuard
    double cov_a_sum = RandomVariable::covariance(a, sum);
    EXPECT_DOUBLE_EQ(cov_a_sum, 4.0);  // Var(a)

    double cov_b_sum = RandomVariable::covariance(b, sum);
    EXPECT_DOUBLE_EQ(cov_b_sum, 1.0);  // Var(b)
}

TEST_F(GlobalStateRemovalTest, ExpressionCovarianceWithoutExplicitContext) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // Expression-based covariance should also work
    Expression expr_a_sum = RandomVariable::cov_expr(a, sum);
    EXPECT_DOUBLE_EQ(expr_a_sum->value(), 4.0);

    Expression expr_b_sum = RandomVariable::cov_expr(b, sum);
    EXPECT_DOUBLE_EQ(expr_b_sum->value(), 1.0);
}

TEST_F(GlobalStateRemovalTest, CacheWorksWithDefaultContext) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // First call should populate cache
    double cov1 = RandomVariable::covariance(a, sum);

    // Get default context to check cache
    auto* default_ctx = ::RandomVariable::get_default_context();
    ASSERT_NE(default_ctx, nullptr);
    EXPECT_GT(default_ctx->cache_size(), 0u);

    // Second call should use cache
    double cov2 = RandomVariable::covariance(a, sum);
    EXPECT_DOUBLE_EQ(cov1, cov2);
}

// ============================================================================
// Thread-Local Context Tests
// ============================================================================

TEST_F(GlobalStateRemovalTest, EachThreadHasOwnDefaultContext) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    std::vector<double> thread_results(3, 0.0);
    std::vector<std::thread> threads;

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([i, &thread_results, a, b]() {
            // Each thread should get its own default context
            RandomVar sum = a + b;

            // Compute covariance (uses thread's default context)
            double cov = RandomVariable::covariance(a, sum);
            thread_results[i] = cov;

            // Verify this thread's default context has cache entries
            auto* default_ctx = ::RandomVariable::get_default_context();
            EXPECT_NE(default_ctx, nullptr);
            EXPECT_GT(default_ctx->cache_size(), 0u);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All threads should get the same correct result
    for (double result : thread_results) {
        EXPECT_DOUBLE_EQ(result, 4.0);
    }
}

TEST_F(GlobalStateRemovalTest, ThreadsDoNotInterfere) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    std::vector<size_t> cache_sizes(2, 0);
    std::vector<std::thread> threads;

    // Thread 1: Compute some covariances
    threads.emplace_back([&cache_sizes, a, b]() {
        RandomVar sum = a + b;
        RandomVar diff = a - b;

        RandomVariable::covariance(a, sum);
        RandomVariable::covariance(b, diff);

        auto* default_ctx = ::RandomVariable::get_default_context();
        cache_sizes[0] = default_ctx->cache_size();
    });

    // Thread 2: Should have empty cache initially
    threads.emplace_back([&cache_sizes, a, b]() {
        // Don't compute anything yet
        auto* default_ctx = ::RandomVariable::get_default_context();
        EXPECT_EQ(default_ctx->cache_size(), 0u);

        // Now compute something different
        RandomVar max_ab = MAX(a, b);
        RandomVariable::covariance(a, max_ab);

        cache_sizes[1] = default_ctx->cache_size();
    });

    for (auto& t : threads) {
        t.join();
    }

    // Both threads should have populated their own caches independently
    EXPECT_GT(cache_sizes[0], 0u);
    EXPECT_GT(cache_sizes[1], 0u);
}

// ============================================================================
// Explicit Context Tests (ActiveContextGuard still works)
// ============================================================================

TEST_F(GlobalStateRemovalTest, ExplicitContextOverridesDefault) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // Create explicit context
    CovarianceContext explicit_ctx;

    // Use explicit context
    {
        ActiveContextGuard guard(&explicit_ctx);
        double cov = RandomVariable::covariance(a, sum);
        EXPECT_DOUBLE_EQ(cov, 4.0);
    }

    // Explicit context should have cache entries
    EXPECT_GT(explicit_ctx.cache_size(), 0u);

    // Default context should still be empty (wasn't used)
    auto* default_ctx = ::RandomVariable::get_default_context();
    EXPECT_EQ(default_ctx->cache_size(), 0u);
}

TEST_F(GlobalStateRemovalTest, ContextSwitching) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    CovarianceContext ctx1;
    CovarianceContext ctx2;

    // Use context 1
    {
        ActiveContextGuard guard(&ctx1);
        RandomVariable::covariance(a, sum);
    }

    // Use context 2
    {
        ActiveContextGuard guard(&ctx2);
        RandomVariable::covariance(b, sum);
    }

    // Use default context
    RandomVariable::covariance(a, b);

    // Each context should have different cache entries
    EXPECT_GT(ctx1.cache_size(), 0u);
    EXPECT_GT(ctx2.cache_size(), 0u);

    auto* default_ctx = ::RandomVariable::get_default_context();
    EXPECT_GT(default_ctx->cache_size(), 0u);
}

// ============================================================================
// Backward Compatibility Tests
// ============================================================================

TEST_F(GlobalStateRemovalTest, GetDefaultContextNeverReturnsNull) {
    // get_default_context() should always return a valid context
    auto* ctx = ::RandomVariable::get_default_context();
    ASSERT_NE(ctx, nullptr);

    // Should be able to use it immediately
    Normal a(10.0, 4.0);
    RandomVar sum = a + a;
    double cov = RandomVariable::covariance(a, sum);
    EXPECT_DOUBLE_EQ(cov, 8.0);  // Cov(a, a+a) = Cov(a, 2*a) = 2*Var(a) = 8
}

TEST_F(GlobalStateRemovalTest, ClearDefaultContext) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // Populate cache
    RandomVariable::covariance(a, sum);

    auto* default_ctx = ::RandomVariable::get_default_context();
    EXPECT_GT(default_ctx->cache_size(), 0u);

    // Clear should work
    default_ctx->clear();
    EXPECT_EQ(default_ctx->cache_size(), 0u);
}

TEST_F(GlobalStateRemovalTest, ExpressionCacheClearWorksWithDefaultContext) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // Populate expression cache
    RandomVariable::cov_expr(a, sum);

    auto* default_ctx = ::RandomVariable::get_default_context();
    EXPECT_GT(default_ctx->expr_cache_size(), 0u);

    // Clear expression cache
    default_ctx->clear_expr_cache();
    EXPECT_EQ(default_ctx->expr_cache_size(), 0u);
}

// ============================================================================
// Complex Scenario Tests
// ============================================================================

TEST_F(GlobalStateRemovalTest, MixedDefaultAndExplicitContexts) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    Normal c(8.0, 2.0);

    // Use default context for a + b
    RandomVar sum_ab = a + b;
    double cov1 = RandomVariable::covariance(a, sum_ab);

    // Use explicit context for b + c
    CovarianceContext explicit_ctx;
    double cov2;
    {
        ActiveContextGuard guard(&explicit_ctx);
        RandomVar sum_bc = b + c;
        cov2 = RandomVariable::covariance(b, sum_bc);
    }

    // Back to default context for a + c
    RandomVar sum_ac = a + c;
    double cov3 = RandomVariable::covariance(a, sum_ac);

    // All should compute correctly
    EXPECT_DOUBLE_EQ(cov1, 4.0);  // Var(a)
    EXPECT_DOUBLE_EQ(cov2, 1.0);   // Var(b)
    EXPECT_DOUBLE_EQ(cov3, 4.0);  // Var(a)

    // Caches should be populated appropriately
    auto* default_ctx = ::RandomVariable::get_default_context();
    EXPECT_GT(default_ctx->cache_size(), 0u);
    EXPECT_GT(explicit_ctx.cache_size(), 0u);
}
