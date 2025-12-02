// -*- c++ -*-
// Author: Jiro Iwai

#include "statistical_functions.hpp"
#include "expression.hpp"

#include <cmath>
#include <memory>

namespace RandomVariable {

// =============================================================================
// Basic statistical functions
// =============================================================================

// phi_expr implemented as a custom function
static CustomFunction phi_func = CustomFunction::create(
    1,
    [](const std::vector<Variable>& v) {
        static constexpr double INV_SQRT_2PI = 0.3989422804014327;  // 1/√(2π)
        const Expression& x = v[0];
        return INV_SQRT_2PI * exp(-(x * x) / Const(2.0));
    },
    "phi"
);

Expression phi_expr(const Expression& x) {
    // φ(x) = exp(-x²/2) / √(2π)
    return phi_func(x);
}

// Phi_expr implemented as a custom function
static CustomFunction Phi_func = CustomFunction::create(
    1,
    [](const std::vector<Variable>& v) {
        static constexpr double INV_SQRT_2 = 0.7071067811865476;  // 1/√2
        const Expression& x = v[0];
        return Const(0.5) * (Const(1.0) + erf(x * INV_SQRT_2));
    },
    "Phi"
);

Expression Phi_expr(const Expression& x) {
    // Φ(x) = 0.5 × (1 + erf(x/√2))
    return Phi_func(x);
}

// MeanMax_expr implemented as a custom function
static CustomFunction MeanMax_func = CustomFunction::create(
    1,
    [](const std::vector<Variable>& v) {
        const Expression& a = v[0];
        return phi_expr(a) + a * Phi_expr(a);
    },
    "MeanMax"
);

// MeanMax2_expr implemented as a custom function
static CustomFunction MeanMax2_func = CustomFunction::create(
    1,
    [](const std::vector<Variable>& v) {
        const Expression& a = v[0];
        return Const(1.0) + (a * a - Const(1.0)) * Phi_expr(a) + a * phi_expr(a);
    },
    "MeanMax2"
);

// MeanPhiMax_expr implemented as a custom function
static CustomFunction MeanPhiMax_func = CustomFunction::create(
    1,
    [](const std::vector<Variable>& v) {
        const Expression& a = v[0];
        return Const(1.0) - Phi_expr(a);
    },
    "MeanPhiMax"
);

Expression MeanMax_expr(const Expression& a) {
    // MeanMax(a) = φ(a) + a × Φ(a)
    return MeanMax_func(a);
}

Expression MeanMax2_expr(const Expression& a) {
    // MeanMax2(a) = 1 + (a² - 1) × Φ(a) + a × φ(a)
    return MeanMax2_func(a);
}

Expression MeanPhiMax_expr(const Expression& a) {
    // MeanPhiMax(a) = Φ(-a) = 1 - Φ(a)
    return MeanPhiMax_func(a);
}

// =============================================================================
// MAX0 functions for SSTA
// =============================================================================

// max0_mean_expr implemented as a custom function
static CustomFunction max0_mean_func = CustomFunction::create(
    2,
    [](const std::vector<Variable>& v) {
        const Expression& mu = v[0];
        const Expression& sigma = v[1];
        Expression a = -(mu / sigma);  // normalized threshold
        return mu + sigma * MeanMax_expr(a);
    },
    "max0_mean"
);

Expression max0_mean_expr(const Expression& mu, const Expression& sigma) {
    // E[max(0, D)] = μ + σ × MeanMax(-μ/σ)
    return max0_mean_func(mu, sigma);
}

// max0_var_expr implemented as a custom function
static CustomFunction max0_var_func = CustomFunction::create(
    2,
    [](const std::vector<Variable>& v) {
        const Expression& mu = v[0];
        const Expression& sigma = v[1];
        Expression a = -(mu / sigma);  // normalized threshold
        Expression mm = MeanMax_expr(a);
        Expression mm2 = MeanMax2_expr(a);
        return sigma * sigma * (mm2 - mm * mm);
    },
    "max0_var"
);

Expression max0_var_expr(const Expression& mu, const Expression& sigma) {
    // Var[max(0, D)] = σ² × (MeanMax2(a) - MeanMax(a)²)  where a = -μ/σ
    return max0_var_func(mu, sigma);
}

// =============================================================================
// Bivariate normal distribution functions
// =============================================================================

// expected_prod_pos_expr implemented as a custom function
static CustomFunction expected_prod_pos_func = CustomFunction::create(
    5,
    [](const std::vector<Variable>& v) {
        const Expression& mu0 = v[0];
        const Expression& sigma0 = v[1];
        const Expression& mu1 = v[2];
        const Expression& sigma1 = v[3];
        const Expression& rho = v[4];
        
        // E[D0⁺ D1⁺] where D0, D1 are bivariate normal
        // Formula:
        // E[D0⁺ D1⁺] = μ0 μ1 Φ₂(a0, a1; ρ)
        //            + μ0 σ1 φ(a1) Φ((a0 - ρa1)/√(1-ρ²))
        //            + μ1 σ0 φ(a0) Φ((a1 - ρa0)/√(1-ρ²))
        //            + σ0 σ1 [ρ Φ₂(a0, a1; ρ) + (1-ρ²) φ₂(a0, a1; ρ)]
        // where a0 = μ0/σ0, a1 = μ1/σ1
        
        Expression a0 = mu0 / sigma0;
        Expression a1 = mu1 / sigma1;
        Expression one_minus_rho2 = Const(1.0) - rho * rho;
        Expression sqrt_one_minus_rho2 = sqrt(one_minus_rho2);

        // Φ₂(a0, a1; ρ) - create PHI2 node directly to avoid dependency on Phi2_expr
        Expression Phi2_a0_a1 = Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::PHI2, a0, a1, rho));

        // φ(a0) and φ(a1)
        Expression phi_a0 = phi_expr(a0);
        Expression phi_a1 = phi_expr(a1);

        // Φ((a0 - ρa1)/√(1-ρ²)) and Φ((a1 - ρa0)/√(1-ρ²))
        Expression Phi_cond_0 = Phi_expr((a0 - rho * a1) / sqrt_one_minus_rho2);
        Expression Phi_cond_1 = Phi_expr((a1 - rho * a0) / sqrt_one_minus_rho2);

        // φ₂(a0, a1; ρ) = 1/(2π√(1-ρ²)) × exp(-(a0² - 2ρa0a1 + a1²)/(2(1-ρ²)))
        // Direct implementation to avoid dependency on phi2_expr
        Expression coeff_phi2 = Const(1.0) / (Const(2.0 * M_PI) * sqrt_one_minus_rho2);
        Expression Q_phi2 = (a0 * a0 - Const(2.0) * rho * a0 * a1 + a1 * a1) / one_minus_rho2;
        Expression phi2_a0_a1 = coeff_phi2 * exp(-Q_phi2 / Const(2.0));

        // Build the formula
        Expression term1 = mu0 * mu1 * Phi2_a0_a1;
        Expression term2 = mu0 * sigma1 * phi_a1 * Phi_cond_0;
        Expression term3 = mu1 * sigma0 * phi_a0 * Phi_cond_1;
        Expression term4 = sigma0 * sigma1 * (rho * Phi2_a0_a1 + one_minus_rho2 * phi2_a0_a1);
        return term1 + term2 + term3 + term4;
    },
    "expected_prod_pos"
);

Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho) {
    // E[D0⁺ D1⁺] where D0, D1 are bivariate normal
    return expected_prod_pos_func(mu0, sigma0, mu1, sigma1, rho);
}

// expected_prod_pos_rho1_expr implemented as a custom function
// Note: min(a0, a1) is computed using the differentiable formula:
// min(a0, a1) = (a0 + a1 - sqrt((a0 - a1)^2 + epsilon)) / 2
// where epsilon is a small constant for numerical stability
static CustomFunction expected_prod_pos_rho1_func = CustomFunction::create(
    4,
    [](const std::vector<Variable>& v) -> Expression {
        const Expression& mu0 = v[0];
        const Expression& sigma0 = v[1];
        const Expression& mu1 = v[2];
        const Expression& sigma1 = v[3];
        
        // E[D0⁺ D1⁺] for ρ = 1 (perfectly correlated)
        // When ρ = 1: D0 = μ0 + σ0·Z, D1 = μ1 + σ1·Z (same Z)
        // Both positive when Z > c where c = -min(a0, a1), a0 = μ0/σ0, a1 = μ1/σ1
        // E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 + 1)·Φ(-c) + (a0 + a1 + c)·φ(c)]
        
        Expression a0 = mu0 / sigma0;
        Expression a1 = mu1 / sigma1;
        
        // c = -min(a0, a1)
        // 
        // Original implementation used runtime value checking:
        //   double a0_val = a0->value();
        //   double a1_val = a1->value();
        //   Expression c = (a0_val < a1_val) ? (-a0) : (-a1);
        //
        // In custom functions, we cannot access runtime values, so we need a
        // differentiable expression that approximates min(a0, a1).
        //
        // Mathematical formula:
        //   min(a, b) = (a + b - |a - b|) / 2
        //
        // The absolute value |a - b| is not differentiable at a = b, so we use
        // a smooth approximation:
        //   |x| ≈ sqrt(x^2 + epsilon)
        //
        // This approximation:
        //   - Is differentiable everywhere (unlike |x|)
        //   - Approaches |x| as epsilon → 0
        //   - Has error O(epsilon) when |x| >> sqrt(epsilon)
        //   - Has error O(sqrt(epsilon)) when |x| ≈ 0
        //
        // For epsilon = 1e-10:
        //   - When |a0 - a1| >> 1e-5: error is negligible (< 1e-10)
        //   - When |a0 - a1| ≈ 0: error is about 1e-5 (acceptable for numerical stability)
        //
        // Alternative approaches considered:
        //   1. tanh(k * x) / k: Smooth but requires tanh function (not available in Expression)
        //   2. x * erf(k * x): Smooth but requires erf (available but more expensive)
        //   3. sqrt(x^2 + epsilon): Simple, efficient, and sufficient for our needs
        //
        static constexpr double epsilon = 1e-10;
        Expression diff = a0 - a1;
        Expression abs_diff = sqrt(diff * diff + Const(epsilon));  // |diff| ≈ sqrt(diff^2 + epsilon)
        Expression min_a0_a1 = (a0 + a1 - abs_diff) / Const(2.0);  // min(a0, a1)
        Expression c = Const(-1.0) * min_a0_a1;  // c = -min(a0, a1)
        
        // E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 + 1)·Φ(-c) + (a0 + a1 + c)·φ(c)]
        Expression Phi_neg_c = Phi_expr(Const(-1.0) * c);
        Expression phi_c = phi_expr(c);
        
        return sigma0 * sigma1 *
               ((a0 * a1 + Const(1.0)) * Phi_neg_c + (a0 + a1 + c) * phi_c);
    },
    "expected_prod_pos_rho1"
);

Expression expected_prod_pos_rho1_expr(const Expression& mu0, const Expression& sigma0,
                                       const Expression& mu1, const Expression& sigma1) {
    // E[D0⁺ D1⁺] for ρ = 1 (perfectly correlated)
    return expected_prod_pos_rho1_func(mu0, sigma0, mu1, sigma1);
}

// expected_prod_pos_rho_neg1_expr implemented as a custom function
// Note: The original implementation checks if a0 + a1 <= 0 at runtime and returns 0.
// In a custom function, we cannot do runtime checks, so we use a smooth approximation:
// We multiply the result by max(0, a0 + a1) / (a0 + a1 + epsilon) to approximate the step function.
// max(0, x) = (x + sqrt(x^2 + epsilon)) / 2 for smooth approximation.
static CustomFunction expected_prod_pos_rho_neg1_func = CustomFunction::create(
    4,
    [](const std::vector<Variable>& v) -> Expression {
        const Expression& mu0 = v[0];
        const Expression& sigma0 = v[1];
        const Expression& mu1 = v[2];
        const Expression& sigma1 = v[3];
        
        // E[D0⁺ D1⁺] for ρ = -1 (perfectly negatively correlated)
        // When ρ = -1: D0 = μ0 + σ0·Z, D1 = μ1 - σ1·Z (opposite signs)
        // Both positive when -a0 < Z < a1 (if a0 + a1 > 0)
        // E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 - 1)·(Φ(a0) + Φ(a1) - 1) + a1·φ(a0) + a0·φ(a1)]
        // Returns 0 if a0 + a1 <= 0 (interval is empty)
        
        Expression a0 = mu0 / sigma0;
        Expression a1 = mu1 / sigma1;
        
        // Original implementation checked runtime condition:
        //   double a0_val = a0->value();
        //   double a1_val = a1->value();
        //   if (a0_val + a1_val <= 0.0) {
        //       return Const(0.0);  // Interval is empty
        //   }
        //
        // In custom functions, we cannot do runtime checks, so we need a
        // differentiable expression that approximates the step function:
        //   step(x) = 1 if x > 0, else 0
        //
        // We use a smooth approximation based on max(0, x):
        //   max(0, x) = (x + |x|) / 2
        //
        // Using the smooth absolute value approximation:
        //   |x| ≈ sqrt(x^2 + epsilon)
        //
        // Therefore:
        //   max(0, x) ≈ (x + sqrt(x^2 + epsilon)) / 2
        //
        // To approximate step(x) = 1 if x > 0 else 0, we use:
        //   step(x) ≈ max(0, x) / (x + epsilon)
        //
        // This approximation:
        //   - Approaches 1 when x >> epsilon (x > 0)
        //   - Approaches 0 when x << -epsilon (x < 0)
        //   - Is smooth and differentiable everywhere
        //   - Has a smooth transition near x = 0
        //
        // Alternative approaches considered:
        //   1. tanh(k * x): Smooth sigmoid, but tanh is not available in Expression
        //   2. (1 + erf(k * x)) / 2: Smooth but requires erf (more expensive)
        //   3. max(0, x) / (x + epsilon): Simple, efficient, and sufficient
        //
        // For our use case (a0 + a1 > 0), we want:
        //   - result * step_factor ≈ result when a0 + a1 > 0
        //   - result * step_factor ≈ 0 when a0 + a1 <= 0
        //
        static constexpr double epsilon = 1e-10;
        Expression sum = a0 + a1;
        Expression abs_sum = sqrt(sum * sum + Const(epsilon));  // |sum| ≈ sqrt(sum^2 + epsilon)
        Expression max_sum = (sum + abs_sum) / Const(2.0);  // max(0, sum)
        // Step function approximation: step(sum) ≈ max(0, sum) / (sum + epsilon)
        Expression step_factor = max_sum / (sum + Const(epsilon));
        
        // E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 - 1)·(Φ(a0) + Φ(a1) - 1) + a1·φ(a0) + a0·φ(a1)]
        Expression Phi_a0 = Phi_expr(a0);
        Expression Phi_a1 = Phi_expr(a1);
        Expression phi_a0 = phi_expr(a0);
        Expression phi_a1 = phi_expr(a1);
        
        Expression result = sigma0 * sigma1 *
                            ((a0 * a1 - Const(1.0)) * (Phi_a0 + Phi_a1 - Const(1.0)) +
                             a1 * phi_a0 + a0 * phi_a1);
        
        // Multiply by step factor to approximate the condition a0 + a1 > 0
        return result * step_factor;
    },
    "expected_prod_pos_rho_neg1"
);

Expression expected_prod_pos_rho_neg1_expr(const Expression& mu0, const Expression& sigma0,
                                           const Expression& mu1, const Expression& sigma1) {
    // E[D0⁺ D1⁺] for ρ = -1 (perfectly negatively correlated)
    return expected_prod_pos_rho_neg1_func(mu0, sigma0, mu1, sigma1);
}

}  // namespace RandomVariable

