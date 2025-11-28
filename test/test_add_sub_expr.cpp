/**
 * @file test_add_sub_expr.cpp
 * @brief Tests for ADD/SUB Expression functions (Phase 3 of #167)
 *
 * Tests:
 * - add_mean_expr: E[A + B] = μ_A + μ_B
 * - add_var_expr: Var[A + B] = σ_A² + σ_B² + 2×Cov(A,B)
 * - sub_mean_expr: E[A - B] = μ_A - μ_B
 * - sub_var_expr: Var[A - B] = σ_A² + σ_B² - 2×Cov(A,B)
 * - Comparison with RandomVariable implementation
 * - Gradient verification
 */

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <iostream>

#include "add.hpp"
#include "covariance.hpp"
#include "expression.hpp"
#include "normal.hpp"
#include "sub.hpp"

// Numerical gradient using central difference
namespace {
double numerical_gradient(std::function<double(double)> f, double x, double h = 1e-6) {
    return (f(x + h) - f(x - h)) / (2.0 * h);
}
}  // namespace

class AddSubExprTest : public ::testing::Test {
   protected:
    void SetUp() override { ExpressionImpl::zero_all_grad(); }

    void TearDown() override { ExpressionImpl::zero_all_grad(); }
};

// =============================================================================
// add_mean_expr tests
// =============================================================================

TEST_F(AddSubExprTest, AddMeanExprValue) {
    // Test that add_mean_expr matches the RandomVariable implementation

    std::vector<std::pair<double, double>> test_cases = {
        {0.0, 0.0},
        {1.0, 2.0},
        {-1.0, 3.0},
        {5.0, -3.0},
        {10.0, 20.0},
        {-10.0, -5.0},
    };

    for (const auto& [mu1, mu2] : test_cases) {
        double sigma1 = 1.0, sigma2 = 1.5;

        // RandomVariable implementation
        RandomVariable::Normal A(mu1, sigma1 * sigma1);
        RandomVariable::Normal B(mu2, sigma2 * sigma2);
        RandomVariable::RandomVariable sum_rv = A + B;
        double rv_mean = sum_rv->mean();

        // Expression implementation
        Variable mu1_expr, mu2_expr;
        mu1_expr = mu1;
        mu2_expr = mu2;
        Expression mean_expr = add_mean_expr(mu1_expr, mu2_expr);
        double expr_mean = mean_expr->value();

        EXPECT_DOUBLE_EQ(expr_mean, rv_mean) << "at μ1=" << mu1 << ", μ2=" << mu2;
        EXPECT_DOUBLE_EQ(expr_mean, mu1 + mu2) << "analytical check";
    }
}

TEST_F(AddSubExprTest, AddMeanExprGradientMu1) {
    // Test gradient w.r.t. μ1: ∂(μ1+μ2)/∂μ1 = 1
    Variable mu1_expr, mu2_expr;

    for (double mu1 : {-2.0, 0.0, 1.0, 5.0}) {
        double mu2 = 3.0;
        ExpressionImpl::zero_all_grad();

        mu1_expr = mu1;
        mu2_expr = mu2;
        Expression mean_expr = add_mean_expr(mu1_expr, mu2_expr);
        mean_expr->backward();

        // Analytical: ∂(μ1+μ2)/∂μ1 = 1
        EXPECT_DOUBLE_EQ(mu1_expr->gradient(), 1.0) << "∂mean/∂μ1 at μ1=" << mu1;
        EXPECT_DOUBLE_EQ(mu2_expr->gradient(), 1.0) << "∂mean/∂μ2 at μ1=" << mu1;
    }
}

// =============================================================================
// add_var_expr tests
// =============================================================================

TEST_F(AddSubExprTest, AddVarExprValue) {
    // Test that add_var_expr matches the RandomVariable implementation

    std::vector<std::tuple<double, double, double>> test_cases = {
        // {σ1, σ2, cov}
        {1.0, 1.0, 0.0},     // Independent
        {1.0, 1.0, 0.5},     // Positive correlation
        {1.0, 1.0, -0.5},    // Negative correlation
        {2.0, 3.0, 0.0},     // Different variances, independent
        {2.0, 3.0, 2.0},     // Positive covariance
        {2.0, 3.0, -2.0},    // Negative covariance
        {1.5, 2.5, 1.0},     // General case
    };

    for (const auto& [sigma1, sigma2, cov] : test_cases) {
        // Expression implementation
        Variable sigma1_expr, sigma2_expr, cov_expr;
        sigma1_expr = sigma1;
        sigma2_expr = sigma2;
        cov_expr = cov;
        Expression var_expr = add_var_expr(sigma1_expr, sigma2_expr, cov_expr);
        double expr_var = var_expr->value();

        // Analytical: Var = σ1² + σ2² + 2×cov
        double expected = sigma1 * sigma1 + sigma2 * sigma2 + 2 * cov;

        EXPECT_DOUBLE_EQ(expr_var, expected)
            << "at σ1=" << sigma1 << ", σ2=" << sigma2 << ", cov=" << cov;
    }
}

TEST_F(AddSubExprTest, AddVarExprGradients) {
    // Test gradients
    // Var = σ1² + σ2² + 2×cov
    // ∂Var/∂σ1 = 2×σ1
    // ∂Var/∂σ2 = 2×σ2
    // ∂Var/∂cov = 2

    Variable sigma1_expr, sigma2_expr, cov_expr;
    double sigma1 = 2.0, sigma2 = 3.0, cov = 1.0;

    sigma1_expr = sigma1;
    sigma2_expr = sigma2;
    cov_expr = cov;
    Expression var_expr = add_var_expr(sigma1_expr, sigma2_expr, cov_expr);
    var_expr->backward();

    EXPECT_DOUBLE_EQ(sigma1_expr->gradient(), 2.0 * sigma1);
    EXPECT_DOUBLE_EQ(sigma2_expr->gradient(), 2.0 * sigma2);
    EXPECT_DOUBLE_EQ(cov_expr->gradient(), 2.0);
}

TEST_F(AddSubExprTest, AddVarExprNumericalGradient) {
    // Numerical gradient verification
    double sigma1 = 2.0, sigma2 = 3.0, cov = 1.0;

    auto f_sigma1 = [sigma2, cov](double s1) {
        Variable s1_v, s2_v, cov_v;
        s1_v = s1;
        s2_v = sigma2;
        cov_v = cov;
        return add_var_expr(s1_v, s2_v, cov_v)->value();
    };

    auto f_sigma2 = [sigma1, cov](double s2) {
        Variable s1_v, s2_v, cov_v;
        s1_v = sigma1;
        s2_v = s2;
        cov_v = cov;
        return add_var_expr(s1_v, s2_v, cov_v)->value();
    };

    auto f_cov = [sigma1, sigma2](double c) {
        Variable s1_v, s2_v, cov_v;
        s1_v = sigma1;
        s2_v = sigma2;
        cov_v = c;
        return add_var_expr(s1_v, s2_v, cov_v)->value();
    };

    Variable sigma1_expr, sigma2_expr, cov_expr;
    sigma1_expr = sigma1;
    sigma2_expr = sigma2;
    cov_expr = cov;
    Expression var_expr = add_var_expr(sigma1_expr, sigma2_expr, cov_expr);
    var_expr->backward();

    EXPECT_NEAR(sigma1_expr->gradient(), numerical_gradient(f_sigma1, sigma1), 1e-6);
    EXPECT_NEAR(sigma2_expr->gradient(), numerical_gradient(f_sigma2, sigma2), 1e-6);
    EXPECT_NEAR(cov_expr->gradient(), numerical_gradient(f_cov, cov), 1e-6);
}

// =============================================================================
// sub_mean_expr tests
// =============================================================================

TEST_F(AddSubExprTest, SubMeanExprValue) {
    // Test that sub_mean_expr matches the RandomVariable implementation

    std::vector<std::pair<double, double>> test_cases = {
        {0.0, 0.0},
        {1.0, 2.0},
        {-1.0, 3.0},
        {5.0, -3.0},
        {10.0, 20.0},
        {-10.0, -5.0},
    };

    for (const auto& [mu1, mu2] : test_cases) {
        double sigma1 = 1.0, sigma2 = 1.5;

        // RandomVariable implementation
        RandomVariable::Normal A(mu1, sigma1 * sigma1);
        RandomVariable::Normal B(mu2, sigma2 * sigma2);
        RandomVariable::RandomVariable diff_rv = A - B;
        double rv_mean = diff_rv->mean();

        // Expression implementation
        Variable mu1_expr, mu2_expr;
        mu1_expr = mu1;
        mu2_expr = mu2;
        Expression mean_expr = sub_mean_expr(mu1_expr, mu2_expr);
        double expr_mean = mean_expr->value();

        EXPECT_DOUBLE_EQ(expr_mean, rv_mean) << "at μ1=" << mu1 << ", μ2=" << mu2;
        EXPECT_DOUBLE_EQ(expr_mean, mu1 - mu2) << "analytical check";
    }
}

TEST_F(AddSubExprTest, SubMeanExprGradientMu1) {
    // Test gradient w.r.t. μ1: ∂(μ1-μ2)/∂μ1 = 1, ∂(μ1-μ2)/∂μ2 = -1
    Variable mu1_expr, mu2_expr;

    for (double mu1 : {-2.0, 0.0, 1.0, 5.0}) {
        double mu2 = 3.0;
        ExpressionImpl::zero_all_grad();

        mu1_expr = mu1;
        mu2_expr = mu2;
        Expression mean_expr = sub_mean_expr(mu1_expr, mu2_expr);
        mean_expr->backward();

        // Analytical: ∂(μ1-μ2)/∂μ1 = 1, ∂(μ1-μ2)/∂μ2 = -1
        EXPECT_DOUBLE_EQ(mu1_expr->gradient(), 1.0) << "∂mean/∂μ1 at μ1=" << mu1;
        EXPECT_DOUBLE_EQ(mu2_expr->gradient(), -1.0) << "∂mean/∂μ2 at μ1=" << mu1;
    }
}

// =============================================================================
// sub_var_expr tests
// =============================================================================

TEST_F(AddSubExprTest, SubVarExprValue) {
    // Test that sub_var_expr matches the RandomVariable implementation

    std::vector<std::tuple<double, double, double>> test_cases = {
        // {σ1, σ2, cov}
        {1.0, 1.0, 0.0},     // Independent
        {1.0, 1.0, 0.5},     // Positive correlation
        {1.0, 1.0, -0.5},    // Negative correlation
        {2.0, 3.0, 0.0},     // Different variances, independent
        {2.0, 3.0, 2.0},     // Positive covariance
        {2.0, 3.0, -2.0},    // Negative covariance
        {1.5, 2.5, 1.0},     // General case
    };

    for (const auto& [sigma1, sigma2, cov] : test_cases) {
        // Expression implementation
        Variable sigma1_expr, sigma2_expr, cov_expr;
        sigma1_expr = sigma1;
        sigma2_expr = sigma2;
        cov_expr = cov;
        Expression var_expr = sub_var_expr(sigma1_expr, sigma2_expr, cov_expr);
        double expr_var = var_expr->value();

        // Analytical: Var = σ1² + σ2² - 2×cov
        double expected = sigma1 * sigma1 + sigma2 * sigma2 - 2 * cov;

        EXPECT_DOUBLE_EQ(expr_var, expected)
            << "at σ1=" << sigma1 << ", σ2=" << sigma2 << ", cov=" << cov;
    }
}

TEST_F(AddSubExprTest, SubVarExprGradients) {
    // Test gradients
    // Var = σ1² + σ2² - 2×cov
    // ∂Var/∂σ1 = 2×σ1
    // ∂Var/∂σ2 = 2×σ2
    // ∂Var/∂cov = -2

    Variable sigma1_expr, sigma2_expr, cov_expr;
    double sigma1 = 2.0, sigma2 = 3.0, cov = 1.0;

    sigma1_expr = sigma1;
    sigma2_expr = sigma2;
    cov_expr = cov;
    Expression var_expr = sub_var_expr(sigma1_expr, sigma2_expr, cov_expr);
    var_expr->backward();

    EXPECT_DOUBLE_EQ(sigma1_expr->gradient(), 2.0 * sigma1);
    EXPECT_DOUBLE_EQ(sigma2_expr->gradient(), 2.0 * sigma2);
    EXPECT_DOUBLE_EQ(cov_expr->gradient(), -2.0);
}

TEST_F(AddSubExprTest, SubVarExprNumericalGradient) {
    // Numerical gradient verification
    double sigma1 = 2.0, sigma2 = 3.0, cov = 1.0;

    auto f_sigma1 = [sigma2, cov](double s1) {
        Variable s1_v, s2_v, cov_v;
        s1_v = s1;
        s2_v = sigma2;
        cov_v = cov;
        return sub_var_expr(s1_v, s2_v, cov_v)->value();
    };

    auto f_sigma2 = [sigma1, cov](double s2) {
        Variable s1_v, s2_v, cov_v;
        s1_v = sigma1;
        s2_v = s2;
        cov_v = cov;
        return sub_var_expr(s1_v, s2_v, cov_v)->value();
    };

    auto f_cov = [sigma1, sigma2](double c) {
        Variable s1_v, s2_v, cov_v;
        s1_v = sigma1;
        s2_v = sigma2;
        cov_v = c;
        return sub_var_expr(s1_v, s2_v, cov_v)->value();
    };

    Variable sigma1_expr, sigma2_expr, cov_expr;
    sigma1_expr = sigma1;
    sigma2_expr = sigma2;
    cov_expr = cov;
    Expression var_expr = sub_var_expr(sigma1_expr, sigma2_expr, cov_expr);
    var_expr->backward();

    EXPECT_NEAR(sigma1_expr->gradient(), numerical_gradient(f_sigma1, sigma1), 1e-6);
    EXPECT_NEAR(sigma2_expr->gradient(), numerical_gradient(f_sigma2, sigma2), 1e-6);
    EXPECT_NEAR(cov_expr->gradient(), numerical_gradient(f_cov, cov), 1e-6);
}

// =============================================================================
// Comparison with RandomVariable tests
// =============================================================================

TEST_F(AddSubExprTest, AddExprMatchesRandomVariable) {
    // Create correlated random variables and verify ADD variance matches
    RandomVariable::Normal A(10.0, 4.0);  // μ=10, σ²=4, σ=2
    RandomVariable::Normal B(20.0, 9.0);  // μ=20, σ²=9, σ=3
    // A and B are independent (cov = 0)

    RandomVariable::RandomVariable sum_rv = A + B;

    double rv_mean = sum_rv->mean();
    double rv_var = sum_rv->variance();

    // Expression calculation
    Variable mu1, mu2, sigma1, sigma2, cov;
    mu1 = 10.0;
    mu2 = 20.0;
    sigma1 = 2.0;
    sigma2 = 3.0;
    cov = 0.0;  // Independent

    Expression mean_expr = add_mean_expr(mu1, mu2);
    Expression var_expr = add_var_expr(sigma1, sigma2, cov);

    EXPECT_DOUBLE_EQ(mean_expr->value(), rv_mean);
    EXPECT_NEAR(var_expr->value(), rv_var, 1e-10);
}

TEST_F(AddSubExprTest, SubExprMatchesRandomVariable) {
    // Create random variables and verify SUB variance matches
    RandomVariable::Normal A(10.0, 4.0);  // μ=10, σ²=4, σ=2
    RandomVariable::Normal B(20.0, 9.0);  // μ=20, σ²=9, σ=3

    RandomVariable::RandomVariable diff_rv = A - B;

    double rv_mean = diff_rv->mean();
    double rv_var = diff_rv->variance();

    // Expression calculation
    Variable mu1, mu2, sigma1, sigma2, cov;
    mu1 = 10.0;
    mu2 = 20.0;
    sigma1 = 2.0;
    sigma2 = 3.0;
    cov = 0.0;  // Independent

    Expression mean_expr = sub_mean_expr(mu1, mu2);
    Expression var_expr = sub_var_expr(sigma1, sigma2, cov);

    EXPECT_DOUBLE_EQ(mean_expr->value(), rv_mean);
    EXPECT_NEAR(var_expr->value(), rv_var, 1e-10);
}

// =============================================================================
// Path delay sensitivity analysis example
// =============================================================================

TEST_F(AddSubExprTest, PathDelaySensitivity) {
    // Simulate a simple path: gate1 -> gate2 -> output
    // Path delay = delay1 + delay2
    // We want to compute ∂(path_mean)/∂μ1, ∂(path_mean)/∂μ2

    Variable mu1, mu2, sigma1, sigma2, cov12;
    mu1 = 10.0;    // Gate 1 mean delay
    mu2 = 15.0;    // Gate 2 mean delay
    sigma1 = 2.0;  // Gate 1 std dev
    sigma2 = 3.0;  // Gate 2 std dev
    cov12 = 0.5;   // Covariance between gate delays

    // Path mean = μ1 + μ2
    Expression path_mean = add_mean_expr(mu1, mu2);
    EXPECT_DOUBLE_EQ(path_mean->value(), 25.0);

    // Path variance = σ1² + σ2² + 2×cov
    Expression path_var = add_var_expr(sigma1, sigma2, cov12);
    EXPECT_DOUBLE_EQ(path_var->value(), 4.0 + 9.0 + 1.0);  // 14.0

    // Sensitivity of path mean
    path_mean->backward();
    EXPECT_DOUBLE_EQ(mu1->gradient(), 1.0);  // ∂mean/∂μ1 = 1
    EXPECT_DOUBLE_EQ(mu2->gradient(), 1.0);  // ∂mean/∂μ2 = 1

    // Sensitivity of path variance
    ExpressionImpl::zero_all_grad();
    path_var->backward();
    EXPECT_DOUBLE_EQ(sigma1->gradient(), 4.0);   // ∂var/∂σ1 = 2σ1 = 4
    EXPECT_DOUBLE_EQ(sigma2->gradient(), 6.0);   // ∂var/∂σ2 = 2σ2 = 6
    EXPECT_DOUBLE_EQ(cov12->gradient(), 2.0);    // ∂var/∂cov = 2
}

// =============================================================================
// Edge case tests
// =============================================================================

TEST_F(AddSubExprTest, ZeroCovarianceCase) {
    Variable sigma1, sigma2, cov;
    sigma1 = 2.0;
    sigma2 = 3.0;
    cov = 0.0;

    Expression add_var = add_var_expr(sigma1, sigma2, cov);
    Expression sub_var = sub_var_expr(sigma1, sigma2, cov);

    // When cov = 0, ADD and SUB have same variance
    EXPECT_DOUBLE_EQ(add_var->value(), sub_var->value());
    EXPECT_DOUBLE_EQ(add_var->value(), 4.0 + 9.0);  // σ1² + σ2²
}

TEST_F(AddSubExprTest, PerfectCorrelationCase) {
    Variable sigma1, sigma2, cov;
    sigma1 = 2.0;
    sigma2 = 3.0;
    cov = 6.0;  // Perfect correlation: cov = σ1 × σ2

    Expression add_var = add_var_expr(sigma1, sigma2, cov);
    Expression sub_var = sub_var_expr(sigma1, sigma2, cov);

    // Var[A+B] = σ1² + σ2² + 2×σ1×σ2 = (σ1 + σ2)²
    EXPECT_DOUBLE_EQ(add_var->value(), 25.0);  // (2 + 3)² = 25

    // Var[A-B] = σ1² + σ2² - 2×σ1×σ2 = (σ1 - σ2)²
    EXPECT_DOUBLE_EQ(sub_var->value(), 1.0);  // (2 - 3)² = 1
}


