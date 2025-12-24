// -*- c++ -*-
// Author: IWAI Jiro

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <nhssta/exception.hpp>

#include "add.hpp"
#include "covariance.hpp"
#include "expression.hpp"
#include "max.hpp"
#include "statistical_functions.hpp"
#include "statistics.hpp"
#include "sub.hpp"
#include "util_numerical.hpp"

namespace RandomVariable {

static CovarianceMatrix covariance_matrix;
CovarianceMatrix& get_covariance_matrix() {
    return covariance_matrix;
}

bool CovarianceMatrixImpl::lookup(const RandomVariable& a, const RandomVariable& b,
                                double& cov) const {
    // Step 1: Cache lookup
    // Normalize key (ensure a <= b ordering) and search cache
    // This allows handling both (a, b) and (b, a) with a single search
    RowCol rowcol = normalize(a, b);
    auto i = cmat.find(rowcol);
    if (i != cmat.end()) {
        // Cache hit: return precomputed covariance value
        cov = i->second;
        return true;
    }

    // Step 2: Fallback handling (cache miss)
    // Check if both variables are NormalImpl (normal distribution) type
    // For NormalImpl types, we can compute immediately using statistical properties
    const auto* a_normal = dynamic_cast<const NormalImpl*>(a.get());
    const auto* b_normal = dynamic_cast<const NormalImpl*>(b.get());
    if (a_normal != nullptr && b_normal != nullptr) {
        // Both are normal distributions
        if (a == b) {
            // Same variable: use property Cov(X, X) = Var(X)
            cov = a->variance();
        } else {
            // Different variables: assume independent normal distributions (cov = 0)
            // If correlation exists, it will be computed recursively in the caller's
            // covariance() function
            cov = 0.0;
        }
        return true;
    }

    // Step 3: Fallback failed
    // For non-NormalImpl types (OpADD, OpMAX, OpMAX0, etc.),
    // delegate to more complex computation logic in the caller's covariance() function
    return false;
}

// covariance_x_max0_y(x, y):
// y must be OpMAX0 (y = max(0, Z)), and (x, Z) must be joint Gaussian.
// Computes Cov(x, max(0, Z)) = Cov(x, Z) * Φ(-μ_Z/σ_Z)
// WARNING: Do not use when x is OpMAX or OpMAX0 (non-Gaussian).
static double covariance_x_max0_y(const RandomVariable& x, const RandomVariable& y) {
    const auto* y_max0 = dynamic_cast<const OpMAX0*>(y.get());
    if (y_max0 == nullptr) {
        throw Nh::RuntimeException("covariance_x_max0_y: y must be OpMAX0 type");
    }
    const RandomVariable& z = y->left();
    double c = covariance(x, z);
    double mu = z->mean();
    double vz = z->variance();
    if (vz <= 0.0) {
        throw Nh::RuntimeException("covariance_x_max0_y: variance must be positive, got " +
                                   std::to_string(vz));
    }
    double sz = sqrt(vz);
    double ms = -mu / sz;
    double mpx = MeanPhiMax(ms);
    double cov = c * mpx;
    if (std::isnan(cov)) {
        throw Nh::RuntimeException("covariance_x_max0_y: covariance calculation resulted in NaN");
    }
    return (cov);
}

// covariance_max0_max0(a, b):
// Both a and b must be OpMAX0: a = max(0, D0), b = max(0, D1)
// Uses 1D Gauss-Hermite quadrature to compute Cov(max(0,D0), max(0,D1))
// where D0, D1 are assumed to be jointly Gaussian.
static double covariance_max0_max0(const RandomVariable& a, const RandomVariable& b) {
    // Get the underlying random variables D0 and D1
    const RandomVariable& d0 = a->left();
    const RandomVariable& d1 = b->left();

    // Get statistics for D0
    double mu0 = d0->mean();
    double v0 = d0->variance();
    if (v0 <= 0.0) {
        throw Nh::RuntimeException("covariance_max0_max0: D0 variance must be positive");
    }
    double sigma0 = sqrt(v0);

    // Get statistics for D1
    double mu1 = d1->mean();
    double v1 = d1->variance();
    if (v1 <= 0.0) {
        throw Nh::RuntimeException("covariance_max0_max0: D1 variance must be positive");
    }
    double sigma1 = sqrt(v1);

    // Get covariance between D0 and D1 (recursive call)
    double covD = covariance(d0, d1);
    if (std::isnan(covD) || std::isinf(covD)) {
        throw Nh::RuntimeException("covariance_max0_max0: covariance(d0, d1) is NaN or Inf");
    }

    // Compute correlation coefficient ρ = Cov(D0,D1) / (σ0 * σ1)
    // Check for zero denominator to avoid division by zero
    if (sigma0 * sigma1 <= 0.0) {
        throw Nh::RuntimeException("covariance_max0_max0: sigma0 * sigma1 must be positive");
    }
    double rho = covD / (sigma0 * sigma1);
    rho = std::min(rho, 1.0);
    rho = std::max(rho, -1.0);

    // E[D0⁺] and E[D1⁺]
    double E0pos = expected_positive_part(mu0, sigma0);
    double E1pos = expected_positive_part(mu1, sigma1);

    // E[D0⁺ D1⁺] via Gauss-Hermite quadrature
    double Eprod = expected_prod_pos(mu0, sigma0, mu1, sigma1, rho);

    // Cov(D0⁺, D1⁺) = E[D0⁺ D1⁺] - E[D0⁺] * E[D1⁺]
    double cov = Eprod - (E0pos * E1pos);

    if (std::isnan(cov)) {
        throw Nh::RuntimeException("covariance_max0_max0: result is NaN");
    }

    return cov;
}

// covariance_max_max(max1, max2):
// Computes Cov(MAX(A,B), MAX(C,D)) using Gaussian closure rule:
// Expands to 4 terms to avoid recursive calls and ensure exact calculation
// Cov(MAX(A,B), MAX(C,D)) = T1·T2·Cov(A,C) + T1·(1-T2)·Cov(A,D)
//                          + (1-T1)·T2·Cov(B,C) + (1-T1)·(1-T2)·Cov(B,D)
static double covariance_max_max(const RandomVariable& max1, const RandomVariable& max2) {
    const auto* max1_ptr = dynamic_cast<const OpMAX*>(max1.get());
    const auto* max2_ptr = dynamic_cast<const OpMAX*>(max2.get());

    if (max1_ptr == nullptr || max2_ptr == nullptr) {
        throw Nh::RuntimeException("covariance_max_max: both arguments must be OpMAX type");
    }

    const RandomVariable& A = max1->left();
    const RandomVariable& B = max1->right();
    const RandomVariable& C = max2->left();
    const RandomVariable& D = max2->right();

    // Compute T1 = Φ(α1) for MAX(A,B)
    double mu_A = A->mean();
    double mu_B = B->mean();
    double var_A = A->variance();
    double var_B = B->variance();
    double cov_AB = covariance(A, B);

    double theta1_sq = var_A + var_B - 2.0 * cov_AB;
    if (theta1_sq < MINIMUM_VARIANCE) {
        theta1_sq = MINIMUM_VARIANCE;
    }
    double theta1 = std::sqrt(theta1_sq);
    double alpha1 = (mu_A - mu_B) / theta1;
    double T1 = normal_cdf(alpha1);

    // Compute T2 = Φ(α2) for MAX(C,D)
    double mu_C = C->mean();
    double mu_D = D->mean();
    double var_C = C->variance();
    double var_D = D->variance();
    double cov_CD = covariance(C, D);

    double theta2_sq = var_C + var_D - 2.0 * cov_CD;
    if (theta2_sq < MINIMUM_VARIANCE) {
        theta2_sq = MINIMUM_VARIANCE;
    }
    double theta2 = std::sqrt(theta2_sq);
    double alpha2 = (mu_C - mu_D) / theta2;
    double T2 = normal_cdf(alpha2);

    // Compute all 4 covariance terms
    double cov_AC = covariance(A, C);
    double cov_AD = covariance(A, D);
    double cov_BC = covariance(B, C);
    double cov_BD = covariance(B, D);

    // Combine: Cov(MAX(A,B), MAX(C,D)) = T1·T2·Cov(A,C) + T1·(1-T2)·Cov(A,D)
    //                                    + (1-T1)·T2·Cov(B,C) + (1-T1)·(1-T2)·Cov(B,D)
    double cov = T1 * T2 * cov_AC +
                 T1 * (1.0 - T2) * cov_AD +
                 (1.0 - T1) * T2 * cov_BC +
                 (1.0 - T1) * (1.0 - T2) * cov_BD;

    if (std::isnan(cov)) {
        throw Nh::RuntimeException("covariance_max_max: result is NaN");
    }

    return cov;
}

// covariance_max_w(max_ab, w):
// Computes Cov(MAX(A,B), W) using Gaussian closure rule:
// Cov(MAX(A,B), W) = T·Cov(A,W) + (1-T)·Cov(B,W)
// where T = Φ(α), α = (μ_A - μ_B) / θ, θ = √(Var(A) + Var(B) - 2Cov(A,B))
static double covariance_max_w(const RandomVariable& max_ab, const RandomVariable& w) {
    const auto* max_ptr = dynamic_cast<const OpMAX*>(max_ab.get());
    if (max_ptr == nullptr) {
        throw Nh::RuntimeException("covariance_max_w: first argument must be OpMAX type");
    }

    const RandomVariable& A = max_ab->left();
    const RandomVariable& B = max_ab->right();

    double mu_A = A->mean();
    double mu_B = B->mean();
    double var_A = A->variance();
    double var_B = B->variance();
    double cov_AB = covariance(A, B);

    // θ² = Var(A) + Var(B) - 2*Cov(A,B)
    double theta_sq = var_A + var_B - 2.0 * cov_AB;
    if (theta_sq < MINIMUM_VARIANCE) {
        theta_sq = MINIMUM_VARIANCE;
    }
    double theta = std::sqrt(theta_sq);

    // α = (μ_A - μ_B) / θ
    double alpha = (mu_A - mu_B) / theta;

    // T = Φ(α)
    double T = normal_cdf(alpha);

    // Cov(MAX(A,B), W) = T·Cov(A,W) + (1-T)·Cov(B,W)
    double cov_AW = covariance(A, w);
    double cov_BW = covariance(B, w);
    double cov = T * cov_AW + (1.0 - T) * cov_BW;

    if (std::isnan(cov)) {
        throw Nh::RuntimeException("covariance_max_w: result is NaN");
    }

    return cov;
}

// Check and clamp covariance to valid range: [-max_cov, max_cov]
// where max_cov = sqrt(variance(a) * variance(b)) = σ_a * σ_b
// This ensures |cov| <= max_cov, which is equivalent to |correlation| <= 1
// (since correlation = cov / (σ_a * σ_b))
//
// Refactored from Issue #180: Simplified implementation using std::clamp
// instead of the previous complex logic with dimension mismatches and
// redundant correlation coefficient calculations.
static void check_covariance(double& cov, const RandomVariable& a, const RandomVariable& b) {
    double var_a = a->variance();
    double var_b = b->variance();
    // Check for negative variance before sqrt
    if (var_a < 0.0 || var_b < 0.0) {
        throw Nh::RuntimeException("check_covariance: variance must be non-negative");
    }
    double max_cov = std::sqrt(var_a * var_b);
    
    // Clamp covariance to valid range: [-max_cov, max_cov]
    // This ensures |cov| <= max_cov, which is equivalent to |correlation| <= 1
    cov = std::clamp(cov, -max_cov, max_cov);
}

double covariance(const Normal& a, const Normal& b) {
    double cov = 0.0;
    covariance_matrix->lookup(a, b, cov);
    return cov;
}

// ============================================================================
// Expression-based covariance (Phase C-5 of #167)
// ============================================================================

// Cache for Expression-based covariance (similar to CovarianceMatrixImpl)
static std::unordered_map<std::pair<RandomVariable, RandomVariable>, Expression, PairHash>
    cov_expr_cache;

void clear_cov_expr_cache() {
    cov_expr_cache.clear();
}

// Helper: Cov(X, MAX0(Z)) as Expression
// X must be jointly Gaussian with Z (X is not MAX/MAX0)
static Expression cov_x_max0_expr(const RandomVariable& x, const RandomVariable& y_max0) {
    const RandomVariable& z = y_max0->left();

    // Cov(X, max0(Z)) = Cov(X, Z) × Φ(μ_Z/σ_Z)
    Expression cov_xz = cov_expr(x, z);
    Expression mu_z = z->mean_expr();
    Expression sigma_z = z->std_expr();

    return cov_xz * Phi_expr(mu_z / sigma_z);
}

// Helper: Cov(MAX0(D0), MAX0(D1)) as Expression
static Expression cov_max0_max0_expr(const RandomVariable& a, const RandomVariable& b) {
    const RandomVariable& d0 = a->left();
    const RandomVariable& d1 = b->left();

    // Get parameters as Expression
    Expression mu0 = d0->mean_expr();
    Expression sigma0 = d0->std_expr();
    Expression mu1 = d1->mean_expr();
    Expression sigma1 = d1->std_expr();

    // ρ = Cov(D0, D1) / (σ0 * σ1)
    Expression cov_d0d1 = cov_expr(d0, d1);
    double rho_val = cov_d0d1->value() / (sigma0->value() * sigma1->value());

    // E[D0⁺] and E[D1⁺]
    Expression E_D0_pos = max0_mean_expr(mu0, sigma0);
    Expression E_D1_pos = max0_mean_expr(mu1, sigma1);

    // Use specialized formulas for ρ ≈ ±1 to avoid numerical issues with 1/√(1-ρ²)
    constexpr double RHO_THRESHOLD = 0.9999;

    Expression E_prod;
    if (rho_val > RHO_THRESHOLD) {
        // ρ ≈ 1: Use analytical formula for perfectly correlated case
        E_prod = expected_prod_pos_rho1_expr(mu0, sigma0, mu1, sigma1);
    } else if (rho_val < -RHO_THRESHOLD) {
        // ρ ≈ -1: Use analytical formula for perfectly negatively correlated case
        E_prod = expected_prod_pos_rho_neg1_expr(mu0, sigma0, mu1, sigma1);
    } else {
        // General case: use bivariate normal formula
        Expression rho = cov_d0d1 / (sigma0 * sigma1);
        E_prod = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    }

    // Cov(D0⁺, D1⁺) = E[D0⁺ D1⁺] - E[D0⁺] × E[D1⁺]
    Expression result = E_prod - E_D0_pos * E_D1_pos;

    return result;
}

// Helper: Cov(MAX(A,B), MAX(C,D)) as Expression using Gaussian closure rule
// Expands to 4 terms to avoid recursive calls and ensure exact calculation
// Cov(MAX(A,B), MAX(C,D)) = T1·T2·Cov(A,C) + T1·(1-T2)·Cov(A,D)
//                          + (1-T1)·T2·Cov(B,C) + (1-T1)·(1-T2)·Cov(B,D)
static Expression cov_max_max_expr(const RandomVariable& max1, const RandomVariable& max2) {
    const RandomVariable& A = max1->left();
    const RandomVariable& B = max1->right();
    const RandomVariable& C = max2->left();
    const RandomVariable& D = max2->right();

    // Compute T1 = Φ(α1) for MAX(A,B)
    Expression mu_A = A->mean_expr();
    Expression mu_B = B->mean_expr();
    Expression var_A = A->var_expr();
    Expression var_B = B->var_expr();
    Expression cov_AB = cov_expr(A, B);

    Expression theta1_sq = var_A + var_B - Const(2.0) * cov_AB + Const(MINIMUM_VARIANCE);
    Expression theta1 = sqrt(theta1_sq);
    Expression alpha1 = (mu_A - mu_B) / theta1;
    Expression T1 = Phi_expr(alpha1);

    // Compute T2 = Φ(α2) for MAX(C,D)
    Expression mu_C = C->mean_expr();
    Expression mu_D = D->mean_expr();
    Expression var_C = C->var_expr();
    Expression var_D = D->var_expr();
    Expression cov_CD = cov_expr(C, D);

    Expression theta2_sq = var_C + var_D - Const(2.0) * cov_CD + Const(MINIMUM_VARIANCE);
    Expression theta2 = sqrt(theta2_sq);
    Expression alpha2 = (mu_C - mu_D) / theta2;
    Expression T2 = Phi_expr(alpha2);

    // Compute all 4 covariance terms
    Expression cov_AC = cov_expr(A, C);
    Expression cov_AD = cov_expr(A, D);
    Expression cov_BC = cov_expr(B, C);
    Expression cov_BD = cov_expr(B, D);

    // Combine: Cov(MAX(A,B), MAX(C,D)) = T1·T2·Cov(A,C) + T1·(1-T2)·Cov(A,D)
    //                                    + (1-T1)·T2·Cov(B,C) + (1-T1)·(1-T2)·Cov(B,D)
    return T1 * T2 * cov_AC +
           T1 * (Const(1.0) - T2) * cov_AD +
           (Const(1.0) - T1) * T2 * cov_BC +
           (Const(1.0) - T1) * (Const(1.0) - T2) * cov_BD;
}

// Helper: Cov(MAX(A,B), W) as Expression using Gaussian closure rule
// Cov(MAX(A,B), W) = T·Cov(A,W) + (1-T)·Cov(B,W)
// where T = Φ(α), α = (μ_A - μ_B) / θ, θ = √(Var(A) + Var(B) - 2Cov(A,B))
static Expression cov_max_w_expr(const RandomVariable& max_ab, const RandomVariable& w) {
    const RandomVariable& A = max_ab->left();
    const RandomVariable& B = max_ab->right();

    Expression mu_A = A->mean_expr();
    Expression mu_B = B->mean_expr();
    Expression var_A = A->var_expr();
    Expression var_B = B->var_expr();
    Expression cov_AB = cov_expr(A, B);

    // θ² = Var(A) + Var(B) - 2*Cov(A,B), with minimum variance protection
    Expression theta_sq = var_A + var_B - Const(2.0) * cov_AB + Const(MINIMUM_VARIANCE);
    Expression theta = sqrt(theta_sq);

    // α = (μ_A - μ_B) / θ
    Expression alpha = (mu_A - mu_B) / theta;

    // T = Φ(α)
    Expression T = Phi_expr(alpha);

    // Cov(MAX(A,B), W) = T·Cov(A,W) + (1-T)·Cov(B,W)
    Expression cov_AW = cov_expr(A, w);
    Expression cov_BW = cov_expr(B, w);
    return T * cov_AW + (Const(1.0) - T) * cov_BW;
}

Expression cov_expr(const RandomVariable& a, const RandomVariable& b) {
    // Check cache first (with symmetry)
    auto it = cov_expr_cache.find({a, b});
    if (it != cov_expr_cache.end()) {
        return it->second;
    }
    it = cov_expr_cache.find({b, a});
    if (it != cov_expr_cache.end()) {
        return it->second;
    }

    Expression result;

    // C-5.2: Same variable → Variance
    if (a == b) {
        result = a->var_expr();
    }
    // C-5.3: ADD linear expansion - Cov(A+B, X) = Cov(A,X) + Cov(B,X)
    else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
        Expression cov_left = cov_expr(a->left(), b);
        Expression cov_right = cov_expr(a->right(), b);
        result = cov_left + cov_right;
    } else if (dynamic_cast<const OpADD*>(b.get()) != nullptr) {
        Expression cov_left = cov_expr(a, b->left());
        Expression cov_right = cov_expr(a, b->right());
        result = cov_left + cov_right;
    }
    // C-5.3: SUB linear expansion - Cov(A-B, X) = Cov(A,X) - Cov(B,X)
    else if (dynamic_cast<const OpSUB*>(a.get()) != nullptr) {
        result = cov_expr(a->left(), b) - cov_expr(a->right(), b);
    } else if (dynamic_cast<const OpSUB*>(b.get()) != nullptr) {
        result = cov_expr(a, b->left()) - cov_expr(a, b->right());
    }
    // C-5.6: MAX using Gaussian closure rule
    else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr &&
             dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
        // Both are OpMAX: use dedicated function for exact 4-term expansion
        result = cov_max_max_expr(a, b);
    } else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr) {
        result = cov_max_w_expr(a, b);
    } else if (dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
        result = cov_max_w_expr(b, a);
    }
    // Handle nested MAX0(MAX0(...)) - pass through
    else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
             dynamic_cast<const OpMAX0*>(a->left().get()) != nullptr) {
        result = cov_expr(a->left(), b);
    } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr &&
               dynamic_cast<const OpMAX0*>(b->left().get()) != nullptr) {
        result = cov_expr(a, b->left());
    }
    // C-5.5: MAX0 × MAX0
    else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
             dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
        if (a->left() == b->left()) {
            // max0(D) with itself: Cov = Var(max0(D))
            result = a->var_expr();
        } else {
            result = cov_max0_max0_expr(a, b);
        }
    }
    // C-5.4: MAX0 × X (X is not MAX0)
    else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr) {
        result = cov_x_max0_expr(b, a);
    } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
        result = cov_x_max0_expr(a, b);
    }
    // C-5.2: Normal × Normal (independent)
    else if (dynamic_cast<const NormalImpl*>(a.get()) != nullptr &&
             dynamic_cast<const NormalImpl*>(b.get()) != nullptr) {
        result = Const(0.0);
    }
    // Unsupported combination
    else {
        throw Nh::RuntimeException(
            "cov_expr: unsupported RandomVariable type combination");
    }

    // Cache the result
    cov_expr_cache[{a, b}] = result;

    return result;
}

double covariance(const RandomVariable& a, const RandomVariable& b) {
    double cov = 0.0;

    if (!covariance_matrix->lookup(a, b, cov)) {
        if (a == b) {
            cov = a->variance();

        } else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
            double cov0 = covariance(a->left(), b);
            double cov1 = covariance(a->right(), b);
            cov = cov0 + cov1;

        } else if (dynamic_cast<const OpADD*>(b.get()) != nullptr) {
            double cov0 = covariance(a, b->left());
            double cov1 = covariance(a, b->right());
            cov = cov0 + cov1;

        } else if (dynamic_cast<const OpSUB*>(a.get()) != nullptr) {
            double cov0 = covariance(a->left(), b);
            double cov1 = covariance(a->right(), b);
            cov = cov0 - cov1;

        } else if (dynamic_cast<const OpSUB*>(b.get()) != nullptr) {
            double cov0 = covariance(a, b->left());
            double cov1 = covariance(a, b->right());
            cov = cov0 - cov1;

        } else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr &&
                   dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
            // Both are OpMAX: use dedicated function for exact 4-term expansion
            // Cov(MAX(A,B), MAX(C,D)) = T1·T2·Cov(A,C) + T1·(1-T2)·Cov(A,D)
            //                          + (1-T1)·T2·Cov(B,C) + (1-T1)·(1-T2)·Cov(B,D)
            cov = covariance_max_max(a, b);

        } else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr) {
            // Only a is OpMAX: Cov(MAX(A,B), W) using Gaussian closure rule
            cov = covariance_max_w(a, b);

        } else if (dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
            // Only b is OpMAX: Cov(W, MAX(A,B)) = Cov(MAX(A,B), W) (symmetry)
            cov = covariance_max_w(b, a);

        } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
                   dynamic_cast<const OpMAX0*>(a->left().get()) != nullptr) {
            cov = covariance(a->left(), b);

        } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr &&
                   dynamic_cast<const OpMAX0*>(b->left().get()) != nullptr) {
            cov = covariance(a, b->left());

        } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
                   dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
            // Both a and b are OpMAX0: use dedicated covariance_max0_max0
            // which uses Gauss-Hermite quadrature for correct bivariate normal handling
            if (a->left() == b->left()) {
                // max(0,D) with itself: Cov = Var(max(0,D))
                cov = a->variance();
            } else {
                cov = covariance_max0_max0(a, b);
            }

        } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr) {
            cov = covariance_x_max0_y(b, a);

        } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
            cov = covariance_x_max0_y(a, b);

        } else if (dynamic_cast<const NormalImpl*>(a.get()) != nullptr &&
                   dynamic_cast<const NormalImpl*>(b.get()) != nullptr) {
            cov = 0.0;

        } else {
            throw Nh::RuntimeException(
                "covariance: unsupported RandomVariable type combination for covariance "
                "calculation");
        }

        check_covariance(cov, a, b);
        covariance_matrix->set(a, b, cov);
    }

    return cov;
}
}  // namespace RandomVariable
