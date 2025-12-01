/**
 * @file test_cov_x_max0_expr_test.cpp
 * @brief Tests for Cov(X, max0(Z)) Expression function (Phase 4 of #167)
 *
 * Formula: Cov(X, max0(Z)) = Cov(X, Z) × MeanPhiMax(-μ_Z/σ_Z)
 *                         = Cov(X, Z) × Φ(μ_Z/σ_Z)
 *
 * where Z ~ N(μ_Z, σ_Z²) and X, Z are jointly Gaussian.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <iostream>

#include "covariance.hpp"
#include "expression.hpp"
#include "max.hpp"
#include "normal.hpp"
#include "test_expression_helpers.hpp"
#include "util_numerical.hpp"

// Numerical gradient using central difference
namespace {
double numerical_gradient(std::function<double(double)> f, double x, double h = 1e-6) {
    return (f(x + h) - f(x - h)) / (2.0 * h);
}
}  // namespace

class CovXMax0ExprTest : public ::testing::Test {
   protected:
    void SetUp() override { ExpressionImpl::zero_all_grad(); }

    void TearDown() override { ExpressionImpl::zero_all_grad(); }
};

// =============================================================================
// cov_x_max0_expr_test value tests
// =============================================================================

TEST_F(CovXMax0ExprTest, CovXMax0ExprValue_BasicCases) {
    // Test that cov_x_max0_expr_test matches the RandomVariable implementation

    // Test cases: {cov_xz, mu_z, sigma_z}
    std::vector<std::tuple<double, double, double>> test_cases = {
        {1.0, 0.0, 1.0},    // Standard case
        {1.0, 1.0, 1.0},    // Positive mean
        {1.0, -1.0, 1.0},   // Negative mean
        {2.0, 0.0, 2.0},    // Larger variance
        {0.5, 2.0, 1.5},    // General case
        {-1.0, 0.0, 1.0},   // Negative covariance
        {0.0, 1.0, 1.0},    // Zero covariance
    };

    for (const auto& [cov_xz, mu_z, sigma_z] : test_cases) {
        // Expression implementation
        Variable cov_expr, mu_expr, sigma_expr;
        cov_expr = cov_xz;
        mu_expr = mu_z;
        sigma_expr = sigma_z;

        Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);
        double expr_value = result->value();

        // Analytical: Cov(X, max0(Z)) = Cov(X,Z) × Φ(μ/σ)
        double expected = cov_xz * RandomVariable::normal_cdf(mu_z / sigma_z);

        EXPECT_NEAR(expr_value, expected, 1e-10)
            << "at cov=" << cov_xz << ", μ=" << mu_z << ", σ=" << sigma_z;
    }
}

TEST_F(CovXMax0ExprTest, CovXMax0ExprValue_MatchesRandomVariable) {
    // Create correlated random variables and compare with actual covariance

    // X ~ N(10, 4), Z ~ N(5, 9)
    // For independent normals, Cov(X, Z) = 0
    // So Cov(X, max0(Z)) = 0
    RandomVariable::Normal X(10.0, 4.0);   // μ=10, σ²=4
    RandomVariable::Normal Z(5.0, 9.0);    // μ=5, σ²=9
    RandomVariable::RandomVariable max0_Z = RandomVariable::MAX0(Z);

    // Get actual covariance from RandomVariable
    double rv_cov = RandomVariable::covariance(X, max0_Z);

    // Expression calculation (X and Z are independent, so cov_xz = 0)
    Variable cov_expr, mu_expr, sigma_expr;
    cov_expr = 0.0;      // Cov(X, Z) = 0 (independent)
    mu_expr = 5.0;       // μ_Z
    sigma_expr = 3.0;    // σ_Z

    Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);

    EXPECT_NEAR(result->value(), rv_cov, 1e-10);
    EXPECT_NEAR(result->value(), 0.0, 1e-10);  // Independent → 0
}

TEST_F(CovXMax0ExprTest, CovXMax0ExprValue_WithCorrelation) {
    // Test with actual correlated variables
    // When X and Z are the same variable: Cov(X, X) = Var(X)
    // Cov(X, max0(X)) = Var(X) × Φ(μ_X/σ_X)

    RandomVariable::Normal X(2.0, 4.0);  // μ=2, σ²=4, σ=2
    RandomVariable::RandomVariable max0_X = RandomVariable::MAX0(X);

    double rv_cov = RandomVariable::covariance(X, max0_X);

    // Expression: Cov(X, max0(X)) = Var(X) × Φ(μ/σ)
    Variable cov_expr, mu_expr, sigma_expr;
    cov_expr = 4.0;      // Cov(X, X) = Var(X) = 4
    mu_expr = 2.0;       // μ_X
    sigma_expr = 2.0;    // σ_X

    Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);

    // Allow small tolerance for table lookup vs analytical
    EXPECT_NEAR(result->value(), rv_cov, 0.01);
}

// =============================================================================
// cov_x_max0_expr_test gradient tests
// =============================================================================

TEST_F(CovXMax0ExprTest, CovXMax0ExprGradient_Cov) {
    // Test gradient w.r.t. cov_xz
    // f = cov × Φ(μ/σ)
    // ∂f/∂cov = Φ(μ/σ)

    Variable cov_expr, mu_expr, sigma_expr;
    double cov_xz = 1.5, mu_z = 1.0, sigma_z = 2.0;

    cov_expr = cov_xz;
    mu_expr = mu_z;
    sigma_expr = sigma_z;

    Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);
    result->backward();

    double expected_grad = RandomVariable::normal_cdf(mu_z / sigma_z);
    EXPECT_NEAR(cov_expr->gradient(), expected_grad, 1e-10);
}

TEST_F(CovXMax0ExprTest, CovXMax0ExprGradient_Mu) {
    // Test gradient w.r.t. μ_z
    // f = cov × Φ(μ/σ)
    // ∂f/∂μ = cov × φ(μ/σ) × (1/σ)

    Variable cov_expr, mu_expr, sigma_expr;
    double cov_xz = 1.5, mu_z = 1.0, sigma_z = 2.0;

    cov_expr = cov_xz;
    mu_expr = mu_z;
    sigma_expr = sigma_z;

    Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);
    result->backward();

    double t = mu_z / sigma_z;
    double expected_grad = cov_xz * RandomVariable::normal_pdf(t) / sigma_z;
    EXPECT_NEAR(mu_expr->gradient(), expected_grad, 1e-10);
}

TEST_F(CovXMax0ExprTest, CovXMax0ExprGradient_Sigma) {
    // Test gradient w.r.t. σ_z
    // f = cov × Φ(μ/σ)
    // ∂f/∂σ = cov × φ(μ/σ) × (-μ/σ²)

    Variable cov_expr, mu_expr, sigma_expr;
    double cov_xz = 1.5, mu_z = 1.0, sigma_z = 2.0;

    cov_expr = cov_xz;
    mu_expr = mu_z;
    sigma_expr = sigma_z;

    Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);
    result->backward();

    double t = mu_z / sigma_z;
    double expected_grad = cov_xz * RandomVariable::normal_pdf(t) * (-mu_z / (sigma_z * sigma_z));
    EXPECT_NEAR(sigma_expr->gradient(), expected_grad, 1e-10);
}

TEST_F(CovXMax0ExprTest, CovXMax0ExprGradient_Numerical) {
    // Verify gradients using numerical differentiation
    double cov_xz = 1.5, mu_z = 1.0, sigma_z = 2.0;

    auto f_cov = [mu_z, sigma_z](double c) {
        Variable cv, mu, sigma;
        cv = c;
        mu = mu_z;
        sigma = sigma_z;
        return cov_x_max0_expr_test(cv, mu, sigma)->value();
    };

    auto f_mu = [cov_xz, sigma_z](double m) {
        Variable cv, mu, sigma;
        cv = cov_xz;
        mu = m;
        sigma = sigma_z;
        return cov_x_max0_expr_test(cv, mu, sigma)->value();
    };

    auto f_sigma = [cov_xz, mu_z](double s) {
        Variable cv, mu, sigma;
        cv = cov_xz;
        mu = mu_z;
        sigma = s;
        return cov_x_max0_expr_test(cv, mu, sigma)->value();
    };

    Variable cov_expr, mu_expr, sigma_expr;
    cov_expr = cov_xz;
    mu_expr = mu_z;
    sigma_expr = sigma_z;

    Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);
    result->backward();

    EXPECT_NEAR(cov_expr->gradient(), numerical_gradient(f_cov, cov_xz), 1e-6);
    EXPECT_NEAR(mu_expr->gradient(), numerical_gradient(f_mu, mu_z), 1e-6);
    EXPECT_NEAR(sigma_expr->gradient(), numerical_gradient(f_sigma, sigma_z), 1e-6);
}

// =============================================================================
// Edge case tests
// =============================================================================

TEST_F(CovXMax0ExprTest, CovXMax0Expr_LargeMu) {
    // When μ >> 0: max0(Z) ≈ Z, so Cov(X, max0(Z)) ≈ Cov(X, Z)
    Variable cov_expr, mu_expr, sigma_expr;
    double cov_xz = 2.0, mu_z = 10.0, sigma_z = 1.0;

    cov_expr = cov_xz;
    mu_expr = mu_z;
    sigma_expr = sigma_z;

    Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);

    // Φ(10/1) ≈ 1, so result ≈ cov_xz
    EXPECT_NEAR(result->value(), cov_xz, 0.01);
}

TEST_F(CovXMax0ExprTest, CovXMax0Expr_LargeNegativeMu) {
    // When μ << 0: max0(Z) ≈ 0, so Cov(X, max0(Z)) ≈ 0
    Variable cov_expr, mu_expr, sigma_expr;
    double cov_xz = 2.0, mu_z = -10.0, sigma_z = 1.0;

    cov_expr = cov_xz;
    mu_expr = mu_z;
    sigma_expr = sigma_z;

    Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);

    // Φ(-10/1) ≈ 0, so result ≈ 0
    EXPECT_NEAR(result->value(), 0.0, 0.01);
}

TEST_F(CovXMax0ExprTest, CovXMax0Expr_ZeroMu) {
    // When μ = 0: Φ(0) = 0.5, so Cov(X, max0(Z)) = 0.5 × Cov(X, Z)
    Variable cov_expr, mu_expr, sigma_expr;
    double cov_xz = 2.0, mu_z = 0.0, sigma_z = 1.0;

    cov_expr = cov_xz;
    mu_expr = mu_z;
    sigma_expr = sigma_z;

    Expression result = cov_x_max0_expr_test(cov_expr, mu_expr, sigma_expr);

    EXPECT_NEAR(result->value(), cov_xz * 0.5, 1e-10);
}

// =============================================================================
// Sensitivity analysis example
// =============================================================================

TEST_F(CovXMax0ExprTest, SensitivityAnalysisExample) {
    // Demonstrate how cov_x_max0_expr_test enables sensitivity analysis
    // for path delays involving MAX operations

    // Scenario: Two gates with delays X ~ N(μ_X, σ_X²), Z ~ N(μ_Z, σ_Z²)
    // Path delay involves max(0, Z-X)
    // We want: ∂Cov(X, max0(Z))/∂μ_Z

    Variable cov_xz, mu_z, sigma_z;
    cov_xz = 1.5;    // Cov(X, Z)
    mu_z = 2.0;      // μ_Z
    sigma_z = 1.5;   // σ_Z

    Expression cov_result = cov_x_max0_expr_test(cov_xz, mu_z, sigma_z);

    std::cout << "\n=== Sensitivity Analysis Example ===\n";
    std::cout << "Cov(X, max0(Z)) = " << cov_result->value() << "\n";

    cov_result->backward();
    std::cout << "∂Cov/∂Cov(X,Z) = " << cov_xz->gradient() << "\n";
    std::cout << "∂Cov/∂μ_Z      = " << mu_z->gradient() << "\n";
    std::cout << "∂Cov/∂σ_Z      = " << sigma_z->gradient() << "\n";

    // Verify gradients are reasonable
    EXPECT_GT(cov_xz->gradient(), 0.0);  // More base correlation → more result
    EXPECT_GT(mu_z->gradient(), 0.0);    // Larger μ → more likely positive → more correlation
}


