/**
 * @file test_expression_helpers.hpp
 * @brief Test-only Expression helper functions
 * 
 * These functions are used only in tests and are not part of the production API.
 * They provide Expression-level interfaces for testing purposes.
 */

#ifndef TEST_EXPRESSION_HELPERS_H
#define TEST_EXPRESSION_HELPERS_H

#include "expression.hpp"
#include "statistical_functions.hpp"

using RandomVariable::Phi_expr;

/**
 * @brief Mean of sum: E[A + B] = μ_A + μ_B
 * 
 * NOTE: This function is for testing purposes only.
 * In production code, OpADD::calc_mean_expr() is used instead.
 */
inline Expression add_mean_expr_test(const Expression& mu1, const Expression& mu2) {
    return mu1 + mu2;
}

/**
 * @brief Variance of sum: Var[A + B] = σ_A² + σ_B² + 2×Cov(A,B)
 * 
 * NOTE: This function is for testing purposes only.
 * In production code, OpADD::calc_var_expr() is used instead.
 * 
 * @param sigma1 Standard deviation of A
 * @param sigma2 Standard deviation of B
 * @param cov Covariance between A and B
 */
inline Expression add_var_expr_test(const Expression& sigma1, const Expression& sigma2,
                                    const Expression& cov) {
    return sigma1 * sigma1 + sigma2 * sigma2 + Const(2.0) * cov;
}

/**
 * @brief Mean of difference: E[A - B] = μ_A - μ_B
 * 
 * NOTE: This function is for testing purposes only.
 * In production code, OpSUB::calc_mean_expr() is used instead.
 */
inline Expression sub_mean_expr_test(const Expression& mu1, const Expression& mu2) {
    return mu1 - mu2;
}

/**
 * @brief Variance of difference: Var[A - B] = σ_A² + σ_B² - 2×Cov(A,B)
 * 
 * NOTE: This function is for testing purposes only.
 * In production code, OpSUB::calc_var_expr() is used instead.
 * 
 * @param sigma1 Standard deviation of A
 * @param sigma2 Standard deviation of B
 * @param cov Covariance between A and B
 */
inline Expression sub_var_expr_test(const Expression& sigma1, const Expression& sigma2,
                                    const Expression& cov) {
    return sigma1 * sigma1 + sigma2 * sigma2 - Const(2.0) * cov;
}

/**
 * @brief Cov(X, max0(Z)) where X and Z are jointly Gaussian
 * 
 * Formula: Cov(X, max0(Z)) = Cov(X, Z) × Φ(μ_Z/σ_Z)
 * 
 * NOTE: This function is for testing purposes only.
 * In production code, cov_x_max0_expr() in covariance.cpp is used instead.
 * 
 * @param cov_xz Covariance between X and Z: Cov(X, Z)
 * @param mu_z Mean of Z
 * @param sigma_z Standard deviation of Z (NOT variance)
 * @return Expression representing Cov(X, max0(Z))
 */
inline Expression cov_x_max0_expr_test(const Expression& cov_xz,
                                       const Expression& mu_z,
                                       const Expression& sigma_z) {
    // Cov(X, max0(Z)) = Cov(X, Z) × Φ(μ_Z/σ_Z)
    return cov_xz * Phi_expr(mu_z / sigma_z);
}

/**
 * @brief Bivariate standard normal CDF Φ₂(h, k; ρ)
 * 
 * Φ₂(h, k; ρ) = P(X ≤ h, Y ≤ k) where (X, Y) ~ N(0, 0, 1, 1, ρ)
 * 
 * Value: Computed using numerical integration (Gauss-Hermite)
 * Gradients (analytical):
 * - ∂Φ₂/∂h = φ(h) × Φ((k - ρh)/√(1-ρ²))
 * - ∂Φ₂/∂k = φ(k) × Φ((h - ρk)/√(1-ρ²))
 * - ∂Φ₂/∂ρ = φ₂(h, k; ρ)
 * 
 * NOTE: This function is for testing purposes only.
 * In production code, expected_prod_pos_expr() in expression.cpp creates PHI2 node directly.
 * 
 * @param h First threshold
 * @param k Second threshold
 * @param rho Correlation coefficient (-1 < ρ < 1)
 */
Expression Phi2_expr_test(const Expression& h, const Expression& k, const Expression& rho);

/**
 * @brief Bivariate standard normal PDF φ₂(x, y; ρ)
 * 
 * Formula: φ₂(x, y; ρ) = 1/(2π√(1-ρ²)) × exp(-(x² - 2ρxy + y²)/(2(1-ρ²)))
 * 
 * NOTE: This function is for testing purposes only.
 * In production code, expected_prod_pos_expr() in expression.cpp uses direct implementation.
 * 
 * @param x First variable
 * @param y Second variable
 * @param rho Correlation coefficient (-1 < ρ < 1)
 * @param one_minus_rho2 Precomputed (1-ρ²) expression (optional)
 * @param sqrt_one_minus_rho2 Precomputed √(1-ρ²) expression (optional)
 */
Expression phi2_expr_test(const Expression& x, const Expression& y, const Expression& rho,
                          const Expression& one_minus_rho2 = Expression(),
                          const Expression& sqrt_one_minus_rho2 = Expression());

/**
 * @brief Cov(max(0,D0), max(0,D1)) where D0, D1 are bivariate normal
 * 
 * Formula: Cov(D0⁺, D1⁺) = E[D0⁺ D1⁺] - E[D0⁺] × E[D1⁺]
 * 
 * NOTE: This function is for testing purposes only.
 * In production code, cov_max0_max0_expr() in covariance.cpp is used instead.
 * 
 * @param mu0 Mean of D0
 * @param sigma0 Standard deviation of D0 (> 0)
 * @param mu1 Mean of D1
 * @param sigma1 Standard deviation of D1 (> 0)
 * @param rho Correlation between D0 and D1
 */
Expression cov_max0_max0_expr_test(const Expression& mu0, const Expression& sigma0,
                                   const Expression& mu1, const Expression& sigma1,
                                   const Expression& rho);

#endif  // TEST_EXPRESSION_HELPERS_H

