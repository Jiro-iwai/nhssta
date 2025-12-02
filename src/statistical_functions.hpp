// -*- c++ -*-
// Author: Jiro Iwai

/**
 * @file statistical_functions.hpp
 * @brief Statistical functions for SSTA implemented as custom functions
 *
 * This file contains declarations for statistical functions used in SSTA
 * (Statistical Static Timing Analysis). All functions are implemented using
 * the CustomFunction mechanism for code clarity and consistency.
 */

#ifndef NHSSTA_STATISTICAL_FUNCTIONS_HPP
#define NHSSTA_STATISTICAL_FUNCTIONS_HPP

#include "expression.hpp"

namespace RandomVariable {

// =============================================================================
// Basic statistical functions
// =============================================================================

/**
 * @brief Standard normal PDF: φ(x) = exp(-x²/2) / √(2π)
 * 
 * @param x Expression to evaluate
 * @return Expression representing φ(x)
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression phi_expr(const Expression& x);

/**
 * @brief Standard normal CDF: Φ(x) = 0.5 × (1 + erf(x/√2))
 * 
 * @param x Expression to evaluate
 * @return Expression representing Φ(x)
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression Phi_expr(const Expression& x);

/**
 * @brief MeanMax function: E[max(a, x)] where x ~ N(0,1)
 * MeanMax(a) = φ(a) + a × Φ(a)
 * 
 * @param a Expression for the threshold
 * @return Expression representing MeanMax(a)
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression MeanMax_expr(const Expression& a);

/**
 * @brief MeanMax2 function: E[max(a, x)²] where x ~ N(0,1)
 * MeanMax2(a) = 1 + (a² - 1) × Φ(a) + a × φ(a)
 * 
 * @param a Expression for the threshold
 * @return Expression representing MeanMax2(a)
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression MeanMax2_expr(const Expression& a);

/**
 * @brief MeanPhiMax function: E[max(a, x) × x] where x ~ N(0,1)
 * MeanPhiMax(a) = Φ(-a) = 1 - Φ(a)
 * 
 * @param a Expression for the threshold
 * @return Expression representing MeanPhiMax(a)
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression MeanPhiMax_expr(const Expression& a);

// =============================================================================
// MAX0 functions for SSTA
// =============================================================================

/**
 * @brief Mean of max(0, D) where D ~ N(μ, σ²)
 * 
 * E[max(0, D)] = μ + σ × MeanMax(-μ/σ)
 *              = μ + σ × (φ(a) + a × Φ(a))  where a = -μ/σ
 * 
 * @param mu Mean of the underlying normal distribution
 * @param sigma Standard deviation (NOT variance)
 * @return Expression representing E[max(0, D)]
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression max0_mean_expr(const Expression& mu, const Expression& sigma);

/**
 * @brief Variance of max(0, D) where D ~ N(μ, σ²)
 * 
 * Var[max(0, D)] = σ² × (MeanMax2(a) - MeanMax(a)²)  where a = -μ/σ
 * 
 * @param mu Mean of the underlying normal distribution
 * @param sigma Standard deviation (NOT variance)
 * @return Expression representing Var[max(0, D)]
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression max0_var_expr(const Expression& mu, const Expression& sigma);

// =============================================================================
// Bivariate normal distribution functions
// =============================================================================

/**
 * @brief E[D0⁺ × D1⁺] where D0, D1 are bivariate normal
 * 
 * Formula:
 * E[D0⁺ D1⁺] = μ0 μ1 Φ₂(a0, a1; ρ) 
 *            + μ0 σ1 φ(a1) Φ((a0 - ρa1)/√(1-ρ²))
 *            + μ1 σ0 φ(a0) Φ((a1 - ρa0)/√(1-ρ²))
 *            + σ0 σ1 [ρ Φ₂(a0, a1; ρ) + (1-ρ²) φ₂(a0, a1; ρ)]
 * 
 * where a0 = μ0/σ0, a1 = μ1/σ1
 * 
 * @param mu0 Mean of D0
 * @param sigma0 Standard deviation of D0 (> 0)
 * @param mu1 Mean of D1
 * @param sigma1 Standard deviation of D1 (> 0)
 * @param rho Correlation between D0 and D1
 * @return Expression representing E[D0⁺ × D1⁺]
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho);

/**
 * @brief E[D0⁺ D1⁺] for ρ = 1 (perfectly correlated)
 *
 * When ρ = 1: D0 = μ0 + σ0·Z, D1 = μ1 + σ1·Z (same Z)
 * Both positive when Z > c where c = -min(a0, a1)
 * E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 + 1)·Φ(-c) + (a0 + a1 + c)·φ(c)]
 * 
 * @param mu0 Mean of D0
 * @param sigma0 Standard deviation of D0 (> 0)
 * @param mu1 Mean of D1
 * @param sigma1 Standard deviation of D1 (> 0)
 * @return Expression representing E[D0⁺ D1⁺] for ρ = 1
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression expected_prod_pos_rho1_expr(const Expression& mu0, const Expression& sigma0,
                                       const Expression& mu1, const Expression& sigma1);

/**
 * @brief E[D0⁺ D1⁺] for ρ = -1 (perfectly negatively correlated)
 *
 * When ρ = -1: D0 = μ0 + σ0·Z, D1 = μ1 - σ1·Z (opposite signs)
 * Both positive when -a0 < Z < a1 (if a0 + a1 > 0)
 * E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 - 1)·(Φ(a0) + Φ(a1) - 1) + a1·φ(a0) + a0·φ(a1)]
 * Returns 0 if a0 + a1 <= 0 (interval is empty)
 * 
 * @param mu0 Mean of D0
 * @param sigma0 Standard deviation of D0 (> 0)
 * @param mu1 Mean of D1
 * @param sigma1 Standard deviation of D1 (> 0)
 * @return Expression representing E[D0⁺ D1⁺] for ρ = -1
 * 
 * @note Implemented as a custom function for code clarity and consistency
 */
Expression expected_prod_pos_rho_neg1_expr(const Expression& mu0, const Expression& sigma0,
                                           const Expression& mu1, const Expression& sigma1);

// =============================================================================
// Cache statistics functions (for compatibility)
// =============================================================================

/**
 * @brief Get cache hit count for phi_expr (always returns 0, cache removed)
 * @return Always returns 0
 */
size_t get_phi_expr_cache_hits();

/**
 * @brief Get cache miss count for phi_expr (always returns 0, cache removed)
 * @return Always returns 0
 */
size_t get_phi_expr_cache_misses();

/**
 * @brief Get cache hit count for Phi_expr (always returns 0, cache removed)
 * @return Always returns 0
 */
size_t get_Phi_expr_cache_hits();

/**
 * @brief Get cache miss count for Phi_expr (always returns 0, cache removed)
 * @return Always returns 0
 */
size_t get_Phi_expr_cache_misses();

/**
 * @brief Get cache hit count for expected_prod_pos_expr (always returns 0, cache removed)
 * @return Always returns 0
 */
size_t get_expected_prod_pos_cache_hits();

/**
 * @brief Get cache miss count for expected_prod_pos_expr (always returns 0, cache removed)
 * @return Always returns 0
 */
size_t get_expected_prod_pos_cache_misses();

}  // namespace RandomVariable

#endif  // NHSSTA_STATISTICAL_FUNCTIONS_HPP

