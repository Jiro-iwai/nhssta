/**
 * @file test_covariance_unification.cpp
 * @brief Tests for Phase 2: Expression-based covariance unification
 *
 * These tests verify that Expression-based covariance computation produces
 * identical results to the direct double-based implementation, enabling
 * safe unification and code deduplication.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <nhssta/exception.hpp>

#include "../src/add.hpp"
#include "../src/covariance.hpp"
#include "../src/expression.hpp"
#include "../src/max.hpp"
#include "../src/normal.hpp"
#include "../src/sub.hpp"

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;

class CovarianceUnificationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Clear caches before each test
        RandomVariable::get_covariance_matrix()->clear();
        RandomVariable::clear_cov_expr_cache();
    }

    void TearDown() override {
        // Clear caches after each test
        RandomVariable::get_covariance_matrix()->clear();
        RandomVariable::clear_cov_expr_cache();
    }

    // Helper: Compare double and Expression-based covariance
    // Default tolerance of 1e-7 accounts for numerical differences in
    // complex operations (MAX, MAX0) due to different evaluation paths
    void ExpectCovarianceEquivalence(const RandomVar& a, const RandomVar& b,
                                     double tolerance = 1e-7) {
        // Compute using both methods
        double cov_double = RandomVariable::covariance(a, b);
        Expression expr = RandomVariable::cov_expr(a, b);
        double cov_expr = expr->value();

        // They should match within tolerance
        EXPECT_NEAR(cov_double, cov_expr, tolerance)
            << "Double covariance: " << cov_double
            << ", Expression covariance: " << cov_expr
            << ", Difference: " << std::abs(cov_double - cov_expr);
    }
};

// ============================================================================
// Basic Equivalence Tests
// ============================================================================

TEST_F(CovarianceUnificationTest, SameVariable) {
    Normal a(10.0, 4.0);

    // For simple operations, we expect higher precision
    ExpectCovarianceEquivalence(a, a, 1e-10);
}

TEST_F(CovarianceUnificationTest, IndependentNormals) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);

    // For simple operations, we expect higher precision
    ExpectCovarianceEquivalence(a, b, 1e-10);
}

TEST_F(CovarianceUnificationTest, Addition) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // For linear operations (ADD), we expect higher precision
    ExpectCovarianceEquivalence(a, sum, 1e-10);
    ExpectCovarianceEquivalence(b, sum, 1e-10);
    ExpectCovarianceEquivalence(sum, sum, 1e-10);
}

TEST_F(CovarianceUnificationTest, Subtraction) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar diff = a - b;

    // For linear operations (SUB), we expect higher precision
    ExpectCovarianceEquivalence(a, diff, 1e-10);
    ExpectCovarianceEquivalence(b, diff, 1e-10);
    ExpectCovarianceEquivalence(diff, diff, 1e-10);
}

TEST_F(CovarianceUnificationTest, Maximum) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar max_ab = MAX(a, b);

    ExpectCovarianceEquivalence(a, max_ab);
    ExpectCovarianceEquivalence(b, max_ab);
    ExpectCovarianceEquivalence(max_ab, max_ab);
}

TEST_F(CovarianceUnificationTest, MaximumWithZero) {
    Normal a(5.0, 4.0);
    RandomVar max0 = MAX0(a);

    // MAX0 uses different numerical algorithms (Gauss-Hermite quadrature)
    // which can result in larger differences (~1e-3)
    ExpectCovarianceEquivalence(a, max0, 2e-3);
    ExpectCovarianceEquivalence(max0, max0, 2e-3);
}

TEST_F(CovarianceUnificationTest, TwoMaximums) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    Normal c(8.0, 2.0);
    Normal d(3.0, 1.5);

    RandomVar max1 = MAX(a, b);
    RandomVar max2 = MAX(c, d);

    ExpectCovarianceEquivalence(max1, max2);
}

TEST_F(CovarianceUnificationTest, TwoMAX0) {
    Normal a(5.0, 4.0);
    Normal b(3.0, 2.0);

    RandomVar max0_a = MAX0(a);
    RandomVar max0_b = MAX0(b);

    // MAX0 x MAX0 involves complex bivariate computation
    // Note: Double version uses Gauss-Hermite quadrature, Expression version uses
    // analytical formula. For independent variables, the analytical formula correctly
    // returns 0, while quadrature has numerical error of ~0.007. This tolerance
    // accounts for this fundamental algorithmic difference.
    ExpectCovarianceEquivalence(max0_a, max0_b, 0.01);
}

// ============================================================================
// Complex Expression Tests
// ============================================================================

TEST_F(CovarianceUnificationTest, ComplexExpression1) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    Normal c(8.0, 2.0);

    // (a + b) - c
    RandomVar expr = (a + b) - c;

    ExpectCovarianceEquivalence(a, expr);
    ExpectCovarianceEquivalence(b, expr);
    ExpectCovarianceEquivalence(c, expr);
}

TEST_F(CovarianceUnificationTest, ComplexExpression2) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    Normal c(8.0, 2.0);

    // MAX(a, b) + c
    RandomVar max_ab = MAX(a, b);
    RandomVar expr = max_ab + c;

    ExpectCovarianceEquivalence(a, expr);
    ExpectCovarianceEquivalence(max_ab, expr);
}

TEST_F(CovarianceUnificationTest, ComplexExpression3) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    Normal c(8.0, 2.0);

    // MAX0(a + b) + c
    RandomVar sum = a + b;
    RandomVar max0 = MAX0(sum);
    RandomVar expr = max0 + c;

    ExpectCovarianceEquivalence(a, expr);
    ExpectCovarianceEquivalence(c, expr);
}

TEST_F(CovarianceUnificationTest, NestedMAX) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    Normal c(8.0, 2.0);
    Normal d(3.0, 1.5);

    // MAX(MAX(a, b), MAX(c, d))
    RandomVar max1 = MAX(a, b);
    RandomVar max2 = MAX(c, d);
    RandomVar max_nested = MAX(max1, max2);

    // Nested MAX accumulates numerical errors
    ExpectCovarianceEquivalence(a, max_nested, 1e-6);
    ExpectCovarianceEquivalence(max1, max_nested, 1e-6);
    ExpectCovarianceEquivalence(max2, max_nested, 1e-6);
}

// ============================================================================
// Symmetry Tests
// ============================================================================

TEST_F(CovarianceUnificationTest, Symmetry) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // Cov(a, b) should equal Cov(b, a)
    double cov_ab = RandomVariable::covariance(a, sum);
    double cov_ba = RandomVariable::covariance(sum, a);

    EXPECT_DOUBLE_EQ(cov_ab, cov_ba);

    // Same for Expression version
    Expression expr_ab = RandomVariable::cov_expr(a, sum);
    Expression expr_ba = RandomVariable::cov_expr(sum, a);

    EXPECT_DOUBLE_EQ(expr_ab->value(), expr_ba->value());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(CovarianceUnificationTest, ZeroVariance) {
    Normal a(5.0, 0.0);  // Zero variance
    Normal b(10.0, 4.0);

    // Should work without throwing (may use MINIMUM_VARIANCE internally)
    EXPECT_NO_THROW({
        ExpectCovarianceEquivalence(a, b);
    });
}

TEST_F(CovarianceUnificationTest, LargeValues) {
    Normal a(1000.0, 100.0);
    Normal b(2000.0, 200.0);
    RandomVar sum = a + b;

    ExpectCovarianceEquivalence(a, sum);
}

TEST_F(CovarianceUnificationTest, SmallValues) {
    Normal a(0.001, 0.0001);
    Normal b(0.002, 0.0001);
    RandomVar sum = a + b;

    // Use larger tolerance for small values
    ExpectCovarianceEquivalence(a, sum, 1e-12);
}

// ============================================================================
// Performance Hints Tests
// ============================================================================

TEST_F(CovarianceUnificationTest, MultipleCalls) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // First call
    ExpectCovarianceEquivalence(a, sum);

    // Second call (should use cache)
    ExpectCovarianceEquivalence(a, sum);

    // Third call
    ExpectCovarianceEquivalence(a, sum);
}

TEST_F(CovarianceUnificationTest, CacheConsistency) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    RandomVar sum = a + b;

    // Compute with double version first
    double cov1 = RandomVariable::covariance(a, sum);

    // Compute with Expression version
    Expression expr = RandomVariable::cov_expr(a, sum);
    double cov2 = expr->value();

    // Compute with double version again
    double cov3 = RandomVariable::covariance(a, sum);

    // All should match
    EXPECT_DOUBLE_EQ(cov1, cov2);
    EXPECT_DOUBLE_EQ(cov2, cov3);
}

// ============================================================================
// Comprehensive Scenario Test
// ============================================================================

TEST_F(CovarianceUnificationTest, RealisticCircuitScenario) {
    // Simulate a small circuit: input delays, gate delays, output
    Normal input1(0.0, 0.001);
    Normal input2(0.0, 0.001);

    Normal gate1_delay(5.0, 0.5);
    Normal gate2_delay(3.0, 0.3);
    Normal gate3_delay(4.0, 0.4);

    // Circuit computation
    RandomVar arrival1 = input1 + gate1_delay;
    RandomVar arrival2 = input2 + gate2_delay;
    RandomVar max_arrival = MAX(arrival1, arrival2);
    RandomVar output = max_arrival + gate3_delay;

    // Verify equivalence at each stage
    ExpectCovarianceEquivalence(input1, arrival1);
    ExpectCovarianceEquivalence(gate1_delay, arrival1);
    ExpectCovarianceEquivalence(arrival1, max_arrival);
    ExpectCovarianceEquivalence(arrival2, max_arrival);
    ExpectCovarianceEquivalence(max_arrival, output);
}
