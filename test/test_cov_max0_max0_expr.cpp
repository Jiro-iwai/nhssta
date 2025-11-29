/**
 * @file test_cov_max0_max0_expr.cpp
 * @brief Tests for Cov(max0, max0) Expression implementation (Issue #167 Phase 5)
 *
 * Tests for:
 * - φ₂(a, b; ρ) - bivariate normal PDF
 * - Φ₂(a, b; ρ) - bivariate normal CDF
 * - expected_prod_pos_expr - E[D0⁺ × D1⁺]
 * - cov_max0_max0_expr - Cov(max(0,D0), max(0,D1))
 */

#include <gtest/gtest.h>
#include <cmath>
#include <iomanip>
#include <iostream>

#include "expression.hpp"
#include "util_numerical.hpp"

namespace {

// Reference implementation: bivariate normal PDF
double phi2_reference(double x, double y, double rho) {
    if (std::abs(rho) > 0.9999) {
        // Degenerate case
        return 0.0;
    }
    double one_minus_rho2 = 1.0 - rho * rho;
    double coeff = 1.0 / (2.0 * M_PI * std::sqrt(one_minus_rho2));
    double Q = (x * x - 2.0 * rho * x * y + y * y) / one_minus_rho2;
    return coeff * std::exp(-Q / 2.0);
}

// Reference implementation: bivariate normal CDF using numerical integration
// Uses Simpson's rule
double Phi2_reference(double h, double k, double rho, int n_points = 1000) {
    if (std::abs(rho) > 0.9999) {
        if (rho > 0) return RandomVariable::normal_cdf(std::min(h, k));
        else return std::max(0.0, RandomVariable::normal_cdf(h) + RandomVariable::normal_cdf(k) - 1.0);
    }
    if (std::abs(rho) < 1e-10) {
        return RandomVariable::normal_cdf(h) * RandomVariable::normal_cdf(k);
    }

    double sigma_prime = std::sqrt(1.0 - rho * rho);
    double lower_bound = -8.0;
    double upper_bound = h;

    if (upper_bound < lower_bound) return 0.0;

    double sum = 0.0;
    double dx = (upper_bound - lower_bound) / n_points;

    for (int i = 0; i <= n_points; ++i) {
        double x = lower_bound + i * dx;
        double f_val = RandomVariable::normal_pdf(x) *
                       RandomVariable::normal_cdf((k - rho * x) / sigma_prime);
        double weight = (i == 0 || i == n_points) ? 1.0 : ((i % 2 == 0) ? 2.0 : 4.0);
        sum += weight * f_val;
    }
    return sum * dx / 3.0;
}

// ============================================================================
// Test Fixture
// ============================================================================

class CovMax0Max0ExprTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Reset all gradients before each test
        zero_all_grad();
    }

    // Numerical gradient using central difference
    double numerical_gradient(std::function<double(double)> f, double x, double h = 1e-5) {
        return (f(x + h) - f(x - h)) / (2.0 * h);
    }
};

// ============================================================================
// 5-3: φ₂(a, b; ρ) Tests
// ============================================================================

TEST_F(CovMax0Max0ExprTest, Phi2Pdf_ValueAtOrigin) {
    // φ₂(0, 0; ρ) = 1/(2π√(1-ρ²))
    Variable x, y, rho;
    x = 0.0;
    y = 0.0;
    rho = 0.0;

    Expression result = phi2_expr(x, y, rho);
    double expected = 1.0 / (2.0 * M_PI);  // ρ=0 case
    EXPECT_NEAR(result->value(), expected, 1e-10);
}

TEST_F(CovMax0Max0ExprTest, Phi2Pdf_ValueWithCorrelation) {
    Variable x, y, rho;
    x = 1.0;
    y = 1.0;
    rho = 0.5;

    Expression result = phi2_expr(x, y, rho);
    double expected = phi2_reference(1.0, 1.0, 0.5);
    EXPECT_NEAR(result->value(), expected, 1e-10);
}

TEST_F(CovMax0Max0ExprTest, Phi2Pdf_ValueNegativeCorrelation) {
    Variable x, y, rho;
    x = 0.5;
    y = -0.5;
    rho = -0.3;

    Expression result = phi2_expr(x, y, rho);
    double expected = phi2_reference(0.5, -0.5, -0.3);
    EXPECT_NEAR(result->value(), expected, 1e-10);
}

TEST_F(CovMax0Max0ExprTest, Phi2Pdf_GradientX) {
    Variable x, y, rho;
    double x_val = 1.0, y_val = 0.5, rho_val = 0.3;
    x = x_val;
    y = y_val;
    rho = rho_val;

    Expression result = phi2_expr(x, y, rho);
    result->value();
    zero_all_grad();
    result->backward();

    // Numerical gradient
    auto f = [&](double xv) { return phi2_reference(xv, y_val, rho_val); };
    double expected_grad = numerical_gradient(f, x_val);

    EXPECT_NEAR(x->gradient(), expected_grad, 1e-6);
}

TEST_F(CovMax0Max0ExprTest, Phi2Pdf_GradientY) {
    Variable x, y, rho;
    double x_val = 1.0, y_val = 0.5, rho_val = 0.3;
    x = x_val;
    y = y_val;
    rho = rho_val;

    Expression result = phi2_expr(x, y, rho);
    result->value();
    zero_all_grad();
    result->backward();

    auto f = [&](double yv) { return phi2_reference(x_val, yv, rho_val); };
    double expected_grad = numerical_gradient(f, y_val);

    EXPECT_NEAR(y->gradient(), expected_grad, 1e-6);
}

TEST_F(CovMax0Max0ExprTest, Phi2Pdf_GradientRho) {
    Variable x, y, rho;
    double x_val = 1.0, y_val = 0.5, rho_val = 0.3;
    x = x_val;
    y = y_val;
    rho = rho_val;

    Expression result = phi2_expr(x, y, rho);
    result->value();
    zero_all_grad();
    result->backward();

    auto f = [&](double rv) { return phi2_reference(x_val, y_val, rv); };
    double expected_grad = numerical_gradient(f, rho_val);

    EXPECT_NEAR(rho->gradient(), expected_grad, 1e-5);
}

// ============================================================================
// 5-4: Φ₂(a, b; ρ) Tests
// ============================================================================

TEST_F(CovMax0Max0ExprTest, Phi2Cdf_IndependentCase) {
    // Φ₂(h, k; 0) = Φ(h) × Φ(k)
    Variable h, k, rho;
    h = 1.0;
    k = 1.0;
    rho = 0.0;

    Expression result = Phi2_expr(h, k, rho);
    double expected = RandomVariable::normal_cdf(1.0) * RandomVariable::normal_cdf(1.0);
    EXPECT_NEAR(result->value(), expected, 1e-6);
}

TEST_F(CovMax0Max0ExprTest, Phi2Cdf_AtOrigin) {
    // Φ₂(0, 0; ρ) = 1/4 + arcsin(ρ)/(2π)
    Variable h, k, rho;
    h = 0.0;
    k = 0.0;
    rho = 0.5;

    Expression result = Phi2_expr(h, k, rho);
    double expected = 0.25 + std::asin(0.5) / (2.0 * M_PI);
    EXPECT_NEAR(result->value(), expected, 1e-5);
}

TEST_F(CovMax0Max0ExprTest, Phi2Cdf_WithCorrelation) {
    Variable h, k, rho;
    h = 1.0;
    k = 0.5;
    rho = 0.3;

    Expression result = Phi2_expr(h, k, rho);
    double expected = Phi2_reference(1.0, 0.5, 0.3);
    EXPECT_NEAR(result->value(), expected, 1e-4);
}

TEST_F(CovMax0Max0ExprTest, Phi2Cdf_NegativeCorrelation) {
    Variable h, k, rho;
    h = 0.5;
    k = 0.5;
    rho = -0.5;

    Expression result = Phi2_expr(h, k, rho);
    double expected = Phi2_reference(0.5, 0.5, -0.5);
    EXPECT_NEAR(result->value(), expected, 1e-4);
}

TEST_F(CovMax0Max0ExprTest, Phi2Cdf_GradientH) {
    // ∂Φ₂/∂h = φ(h) × Φ((k - ρh)/√(1-ρ²))
    Variable h, k, rho;
    double h_val = 1.0, k_val = 0.5, rho_val = 0.3;
    h = h_val;
    k = k_val;
    rho = rho_val;

    Expression result = Phi2_expr(h, k, rho);
    result->value();
    zero_all_grad();
    result->backward();

    // Analytical gradient
    double sigma = std::sqrt(1.0 - rho_val * rho_val);
    double expected_grad = RandomVariable::normal_pdf(h_val) *
                           RandomVariable::normal_cdf((k_val - rho_val * h_val) / sigma);

    EXPECT_NEAR(h->gradient(), expected_grad, 1e-5);
}

TEST_F(CovMax0Max0ExprTest, Phi2Cdf_GradientK) {
    // ∂Φ₂/∂k = φ(k) × Φ((h - ρk)/√(1-ρ²))
    Variable h, k, rho;
    double h_val = 1.0, k_val = 0.5, rho_val = 0.3;
    h = h_val;
    k = k_val;
    rho = rho_val;

    Expression result = Phi2_expr(h, k, rho);
    result->value();
    zero_all_grad();
    result->backward();

    double sigma = std::sqrt(1.0 - rho_val * rho_val);
    double expected_grad = RandomVariable::normal_pdf(k_val) *
                           RandomVariable::normal_cdf((h_val - rho_val * k_val) / sigma);

    EXPECT_NEAR(k->gradient(), expected_grad, 1e-5);
}

TEST_F(CovMax0Max0ExprTest, Phi2Cdf_GradientRho) {
    // ∂Φ₂/∂ρ = φ₂(h, k; ρ)
    Variable h, k, rho;
    double h_val = 1.0, k_val = 0.5, rho_val = 0.3;
    h = h_val;
    k = k_val;
    rho = rho_val;

    Expression result = Phi2_expr(h, k, rho);
    result->value();
    zero_all_grad();
    result->backward();

    double expected_grad = phi2_reference(h_val, k_val, rho_val);
    EXPECT_NEAR(rho->gradient(), expected_grad, 1e-5);
}

// ============================================================================
// 5-5: expected_prod_pos_expr Tests
// ============================================================================

TEST_F(CovMax0Max0ExprTest, ExpectedProdPos_IndependentStandardNormal) {
    // E[X⁺ × Y⁺] where X, Y ~ N(0,1) independent
    // = E[X⁺]² = (1/√(2π))² = 1/(2π) ≈ 0.1592
    // Note: Analytical formula gives 0.159 (theoretical), Gauss-Hermite gives ~0.166 (~4% error)
    Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 0.0;
    sigma0 = 1.0;
    mu1 = 0.0;
    sigma1 = 1.0;
    rho = 0.0;

    Expression result = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    // Verify against theoretical value (1/(2π) ≈ 0.1592)
    double theoretical = 1.0 / (2.0 * M_PI);
    EXPECT_NEAR(result->value(), theoretical, 1e-4);

    // Also compare with RV (allow ~5% tolerance due to different numerical methods)
    double rv_result = RandomVariable::expected_prod_pos(0.0, 1.0, 0.0, 1.0, 0.0);
    EXPECT_NEAR(result->value(), rv_result, 0.01);  // ~5% tolerance
}

TEST_F(CovMax0Max0ExprTest, ExpectedProdPos_PositiveMeans) {
    // For practical SSTA (μ > 2σ), Expression and RV should match well
    Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 2.0;
    sigma0 = 1.0;
    mu1 = 2.0;
    sigma1 = 1.0;
    rho = 0.5;

    Expression result = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    double rv_result = RandomVariable::expected_prod_pos(2.0, 1.0, 2.0, 1.0, 0.5);
    // Allow 2% tolerance for moderate μ/σ ratio
    EXPECT_NEAR(result->value(), rv_result, rv_result * 0.02);
}

TEST_F(CovMax0Max0ExprTest, ExpectedProdPos_LargeMeans) {
    // For large μ >> σ, E[D0⁺ D1⁺] ≈ E[D0 D1] = μ0μ1 + ρσ0σ1
    Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 10.0;
    sigma0 = 1.0;
    mu1 = 10.0;
    sigma1 = 1.0;
    rho = 0.5;

    Expression result = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    double rv_result = RandomVariable::expected_prod_pos(10.0, 1.0, 10.0, 1.0, 0.5);
    double approx = 10.0 * 10.0 + 0.5 * 1.0 * 1.0;  // μ0μ1 + ρσ0σ1

    EXPECT_NEAR(result->value(), rv_result, 1e-3);
    EXPECT_NEAR(result->value(), approx, 1e-3);  // Should be close to approximation
}

TEST_F(CovMax0Max0ExprTest, ExpectedProdPos_GradientMu0) {
    // Use Expression's own numerical gradient for consistency
    Variable mu0, sigma0, mu1, sigma1, rho;
    double mu0_val = 2.0, sigma0_val = 1.0, mu1_val = 2.0, sigma1_val = 1.0, rho_val = 0.3;
    mu0 = mu0_val;
    sigma0 = sigma0_val;
    mu1 = mu1_val;
    sigma1 = sigma1_val;
    rho = rho_val;

    Expression result = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    result->value();
    zero_all_grad();
    result->backward();

    // Numerical gradient using Expression
    auto f = [&](double m0) {
        Variable v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho;
        v_mu0 = m0;
        v_sigma0 = sigma0_val;
        v_mu1 = mu1_val;
        v_sigma1 = sigma1_val;
        v_rho = rho_val;
        Expression e = expected_prod_pos_expr(v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho);
        return e->value();
    };
    double expected_grad = numerical_gradient(f, mu0_val);

    EXPECT_NEAR(mu0->gradient(), expected_grad, 1e-3);
}

TEST_F(CovMax0Max0ExprTest, ExpectedProdPos_GradientSigma0) {
    Variable mu0, sigma0, mu1, sigma1, rho;
    double mu0_val = 2.0, sigma0_val = 1.0, mu1_val = 2.0, sigma1_val = 1.0, rho_val = 0.3;
    mu0 = mu0_val;
    sigma0 = sigma0_val;
    mu1 = mu1_val;
    sigma1 = sigma1_val;
    rho = rho_val;

    Expression result = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    result->value();
    zero_all_grad();
    result->backward();

    auto f = [&](double s0) {
        Variable v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho;
        v_mu0 = mu0_val;
        v_sigma0 = s0;
        v_mu1 = mu1_val;
        v_sigma1 = sigma1_val;
        v_rho = rho_val;
        Expression e = expected_prod_pos_expr(v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho);
        return e->value();
    };
    double expected_grad = numerical_gradient(f, sigma0_val);

    EXPECT_NEAR(sigma0->gradient(), expected_grad, 1e-3);
}

TEST_F(CovMax0Max0ExprTest, ExpectedProdPos_GradientRho) {
    Variable mu0, sigma0, mu1, sigma1, rho;
    double mu0_val = 2.0, sigma0_val = 1.0, mu1_val = 2.0, sigma1_val = 1.0, rho_val = 0.3;
    mu0 = mu0_val;
    sigma0 = sigma0_val;
    mu1 = mu1_val;
    sigma1 = sigma1_val;
    rho = rho_val;

    Expression result = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    result->value();
    zero_all_grad();
    result->backward();

    auto f = [&](double r) {
        Variable v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho;
        v_mu0 = mu0_val;
        v_sigma0 = sigma0_val;
        v_mu1 = mu1_val;
        v_sigma1 = sigma1_val;
        v_rho = r;
        Expression e = expected_prod_pos_expr(v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho);
        return e->value();
    };
    double expected_grad = numerical_gradient(f, rho_val);

    EXPECT_NEAR(rho->gradient(), expected_grad, 1e-3);
}

// ============================================================================
// 5-6: cov_max0_max0_expr Tests
// ============================================================================

TEST_F(CovMax0Max0ExprTest, CovMax0Max0_IndependentCase) {
    // Cov(D0⁺, D1⁺) = E[D0⁺D1⁺] - E[D0⁺]E[D1⁺]
    // For independent: should be 0 (theoretical)
    Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 5.0;  // Use larger μ for better numerical stability
    sigma0 = 1.0;
    mu1 = 5.0;
    sigma1 = 1.0;
    rho = 0.0;

    Expression result = cov_max0_max0_expr(mu0, sigma0, mu1, sigma1, rho);
    double rv_result = RandomVariable::covariance_max0_max0(5.0, 1.0, 5.0, 1.0, 0.0);
    // For large μ, both should be very close to 0
    EXPECT_NEAR(result->value(), 0.0, 1e-3);
    EXPECT_NEAR(rv_result, 0.0, 1e-3);
}

TEST_F(CovMax0Max0ExprTest, CovMax0Max0_PositiveCorrelation) {
    // Use realistic gate delay parameters (μ > 2σ)
    Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 5.0;
    sigma0 = 1.0;
    mu1 = 5.0;
    sigma1 = 1.0;
    rho = 0.5;

    Expression result = cov_max0_max0_expr(mu0, sigma0, mu1, sigma1, rho);
    double rv_result = RandomVariable::covariance_max0_max0(5.0, 1.0, 5.0, 1.0, 0.5);
    // For large μ, Cov(D0⁺, D1⁺) ≈ Cov(D0, D1) = ρσ0σ1 = 0.5
    EXPECT_NEAR(result->value(), 0.5, 0.01);
    EXPECT_NEAR(rv_result, 0.5, 0.01);
    EXPECT_GT(result->value(), 0.0);
}

TEST_F(CovMax0Max0ExprTest, CovMax0Max0_NegativeCorrelation) {
    // Use realistic gate delay parameters
    Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 5.0;
    sigma0 = 1.0;
    mu1 = 5.0;
    sigma1 = 1.0;
    rho = -0.5;

    Expression result = cov_max0_max0_expr(mu0, sigma0, mu1, sigma1, rho);
    double rv_result = RandomVariable::covariance_max0_max0(5.0, 1.0, 5.0, 1.0, -0.5);
    // For large μ, Cov ≈ ρσ0σ1 = -0.5
    EXPECT_NEAR(result->value(), -0.5, 0.01);
    EXPECT_NEAR(rv_result, -0.5, 0.01);
}

TEST_F(CovMax0Max0ExprTest, CovMax0Max0_LargeMeans) {
    // For large μ, max(0, D) ≈ D, so Cov(D0⁺, D1⁺) ≈ Cov(D0, D1) = ρσ0σ1
    Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 10.0;
    sigma0 = 1.0;
    mu1 = 10.0;
    sigma1 = 1.0;
    rho = 0.5;

    Expression result = cov_max0_max0_expr(mu0, sigma0, mu1, sigma1, rho);
    double rv_result = RandomVariable::covariance_max0_max0(10.0, 1.0, 10.0, 1.0, 0.5);
    double approx = 0.5 * 1.0 * 1.0;  // ρσ0σ1

    EXPECT_NEAR(result->value(), rv_result, 1e-3);
    EXPECT_NEAR(result->value(), approx, 1e-3);
}

TEST_F(CovMax0Max0ExprTest, CovMax0Max0_GradientMu0) {
    Variable mu0, sigma0, mu1, sigma1, rho;
    double mu0_val = 5.0, sigma0_val = 1.0, mu1_val = 5.0, sigma1_val = 1.0, rho_val = 0.3;
    mu0 = mu0_val;
    sigma0 = sigma0_val;
    mu1 = mu1_val;
    sigma1 = sigma1_val;
    rho = rho_val;

    Expression result = cov_max0_max0_expr(mu0, sigma0, mu1, sigma1, rho);
    result->value();
    zero_all_grad();
    result->backward();

    auto f = [&](double m0) {
        Variable v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho;
        v_mu0 = m0;
        v_sigma0 = sigma0_val;
        v_mu1 = mu1_val;
        v_sigma1 = sigma1_val;
        v_rho = rho_val;
        Expression e = cov_max0_max0_expr(v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho);
        return e->value();
    };
    double expected_grad = numerical_gradient(f, mu0_val);

    EXPECT_NEAR(mu0->gradient(), expected_grad, 1e-3);
}

TEST_F(CovMax0Max0ExprTest, CovMax0Max0_GradientRho) {
    Variable mu0, sigma0, mu1, sigma1, rho;
    double mu0_val = 5.0, sigma0_val = 1.0, mu1_val = 5.0, sigma1_val = 1.0, rho_val = 0.3;
    mu0 = mu0_val;
    sigma0 = sigma0_val;
    mu1 = mu1_val;
    sigma1 = sigma1_val;
    rho = rho_val;

    Expression result = cov_max0_max0_expr(mu0, sigma0, mu1, sigma1, rho);
    result->value();
    zero_all_grad();
    result->backward();

    auto f = [&](double r) {
        Variable v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho;
        v_mu0 = mu0_val;
        v_sigma0 = sigma0_val;
        v_mu1 = mu1_val;
        v_sigma1 = sigma1_val;
        v_rho = r;
        Expression e = cov_max0_max0_expr(v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho);
        return e->value();
    };
    double expected_grad = numerical_gradient(f, rho_val);

    EXPECT_NEAR(rho->gradient(), expected_grad, 1e-3);
}

// ============================================================================
// Consistency Tests
// ============================================================================

TEST_F(CovMax0Max0ExprTest, ConsistencyWithRandomVariable) {
    // Test practical SSTA parameter combinations (μ > 2σ)
    std::vector<std::tuple<double, double, double, double, double>> test_cases = {
        {3.0, 1.0, 3.0, 1.0, 0.0},    // μ/σ = 3, independent
        {3.0, 1.0, 3.0, 1.0, 0.5},    // μ/σ = 3, moderate correlation
        {5.0, 1.0, 5.0, 1.0, 0.3},    // μ/σ = 5, typical case
        {5.0, 0.5, 4.0, 0.8, 0.6},    // Different μ, σ
        {10.0, 1.0, 10.0, 1.0, 0.5},  // Large μ/σ
    };

    std::cout << "\n=== Consistency Test: Expression vs RandomVariable ===\n";
    std::cout << std::fixed << std::setprecision(6);

    for (const auto& tc : test_cases) {
        double mu0_val = std::get<0>(tc);
        double sigma0_val = std::get<1>(tc);
        double mu1_val = std::get<2>(tc);
        double sigma1_val = std::get<3>(tc);
        double rho_val = std::get<4>(tc);

        Variable mu0, sigma0, mu1, sigma1, rho;
        mu0 = mu0_val;
        sigma0 = sigma0_val;
        mu1 = mu1_val;
        sigma1 = sigma1_val;
        rho = rho_val;

        Expression expr_result = cov_max0_max0_expr(mu0, sigma0, mu1, sigma1, rho);
        double rv_result = RandomVariable::covariance_max0_max0(
            mu0_val, sigma0_val, mu1_val, sigma1_val, rho_val);

        double diff = std::abs(expr_result->value() - rv_result);
        double expected_cov = rho_val * sigma0_val * sigma1_val;  // ρσ0σ1 for large μ

        std::cout << "μ0=" << mu0_val << " σ0=" << sigma0_val
                  << " μ1=" << mu1_val << " σ1=" << sigma1_val
                  << " ρ=" << rho_val
                  << " Expr=" << expr_result->value()
                  << " RV=" << rv_result
                  << " Expected=" << expected_cov
                  << " Diff=" << diff << "\n";

        // For practical SSTA, both should be close to ρσ0σ1
        EXPECT_NEAR(expr_result->value(), expected_cov, 0.02);
        EXPECT_NEAR(rv_result, expected_cov, 0.02);
    }
}

}  // namespace

