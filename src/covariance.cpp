// -*- c++ -*-
// Author: IWAI Jiro

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <nhssta/exception.hpp>

#include "add.hpp"
#include "covariance.hpp"
#include "expression.hpp"
#include "max.hpp"
#include "profiling.hpp"
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
    RowCol rowcol0(a, b);
    auto i = cmat.find(rowcol0);
    if (i != cmat.end()) {
        cov = i->second;
        return true;
    }

    RowCol rowcol1(b, a);
    i = cmat.find(rowcol1);
    if (i != cmat.end()) {
        cov = i->second;
        return true;
    }

    const auto* a_normal = dynamic_cast<const NormalImpl*>(a.get());
    const auto* b_normal = dynamic_cast<const NormalImpl*>(b.get());
    if (a_normal != nullptr && b_normal != nullptr) {
        if (a == b) {
            cov = a->variance();
        } else {
            cov = 0.0;
        }
        return true;
    }
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
    // Clamp to [-1, 1] for numerical stability (also clamped in expected_prod_pos)
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

// Check and clamp covariance to valid range: [-max_cov, max_cov]
// where max_cov = sqrt(variance(a) * variance(b)) = σ_a * σ_b
// This ensures |cov| <= max_cov, which is equivalent to |correlation| <= 1
// (since correlation = cov / (σ_a * σ_b))
//
// Refactored from Issue #180: Simplified implementation using std::clamp
// instead of the previous complex logic with dimension mismatches and
// redundant correlation coefficient calculations.
static void check_covariance(double& cov, const RandomVariable& a, const RandomVariable& b) {
    double max_cov = std::sqrt(a->variance() * b->variance());
    
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

#ifdef DEBUG
// Profiling statistics for cov_expr
struct CovExprStats {
    size_t total_calls = 0;
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    size_t normal_normal_cases = 0;
    size_t add_expansions = 0;
    size_t sub_expansions = 0;
    size_t max_expansions = 0;
    size_t max0_expansions = 0;
    size_t recursive_calls = 0;
    size_t max_recursion_depth = 0;
    std::unordered_map<std::string, size_t> case_counts;
    
    void reset() {
        total_calls = 0;
        cache_hits = 0;
        cache_misses = 0;
        normal_normal_cases = 0;
        add_expansions = 0;
        sub_expansions = 0;
        max_expansions = 0;
        max0_expansions = 0;
        recursive_calls = 0;
        max_recursion_depth = 0;
        case_counts.clear();
    }
    
    void print(std::ostream& os = std::cerr) const {
        os << "\n=== cov_expr() Detailed Analysis ===" << std::endl;
        os << "Total calls: " << total_calls << std::endl;
        if (total_calls > 0) {
            os << "Cache hits: " << cache_hits << " (" << (100.0 * cache_hits / total_calls) << "%)" << std::endl;
            os << "Cache misses: " << cache_misses << " (" << (100.0 * cache_misses / total_calls) << "%)" << std::endl;
        }
        os << "\nCase breakdown:" << std::endl;
        os << "  Normal-Normal: " << normal_normal_cases << std::endl;
        os << "  ADD expansion: " << add_expansions << std::endl;
        os << "  SUB expansion: " << sub_expansions << std::endl;
        os << "  MAX expansion: " << max_expansions << std::endl;
        os << "  MAX0 expansion: " << max0_expansions << std::endl;
        os << "  Recursive calls: " << recursive_calls << std::endl;
        os << "  Max recursion depth: " << max_recursion_depth << std::endl;
        os << "\nCase type distribution:" << std::endl;
        for (const auto& pair : case_counts) {
            os << "  " << pair.first << ": " << pair.second << std::endl;
        }
        os << std::endl;
    }
};

static CovExprStats cov_expr_stats;

void reset_cov_expr_stats() {
    cov_expr_stats.reset();
}

void print_cov_expr_stats() {
    cov_expr_stats.print();
}
#else
// Dummy implementations for non-DEBUG builds
void reset_cov_expr_stats() {}
void print_cov_expr_stats() {}
#endif

void clear_cov_expr_cache() {
    cov_expr_cache.clear();
}

// Helper: Cov(X, MAX0(Z)) as Expression
// X must be jointly Gaussian with Z (X is not MAX/MAX0)
static Expression cov_x_max0_expr(const RandomVariable& x, const RandomVariable& y_max0) {
    const RandomVariable& z = y_max0->left();

    // Cov(X, max0(Z)) = Cov(X, Z) × Φ(μ_Z/σ_Z)
    Expression cov_xz = cov_expr(x, z, 0);  // Reset depth for helper functions
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
    Expression cov_d0d1 = cov_expr(d0, d1, 0);  // Reset depth for helper functions
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

Expression cov_expr(const RandomVariable& a, const RandomVariable& b, int depth) {
    PROFILE_FUNCTION();
#ifdef DEBUG
    cov_expr_stats.total_calls++;
    if (depth > static_cast<int>(cov_expr_stats.max_recursion_depth)) {
        cov_expr_stats.max_recursion_depth = depth;
    }
    if (depth > 0) {
        cov_expr_stats.recursive_calls++;
    }
#endif
    // Check cache first (with symmetry)
    auto it = cov_expr_cache.find({a, b});
    if (it != cov_expr_cache.end()) {
#ifdef DEBUG
        cov_expr_stats.cache_hits++;
#endif
        return it->second;
    }
    it = cov_expr_cache.find({b, a});
    if (it != cov_expr_cache.end()) {
#ifdef DEBUG
        cov_expr_stats.cache_hits++;
#endif
        return it->second;
    }
#ifdef DEBUG
    cov_expr_stats.cache_misses++;
#endif

    Expression result;

    // C-5.2: Same variable → Variance
    if (a == b) {
#ifdef DEBUG
        cov_expr_stats.case_counts["SameVariable"]++;
#endif
        result = a->var_expr();
    }
    // C-5.3: ADD linear expansion - Cov(A+B, X) = Cov(A,X) + Cov(B,X)
    else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.add_expansions++;
        cov_expr_stats.case_counts["ADD_a"]++;
#endif
        Expression cov_left = cov_expr(a->left(), b, depth + 1);
        Expression cov_right = cov_expr(a->right(), b, depth + 1);
        result = cov_left + cov_right;
    } else if (dynamic_cast<const OpADD*>(b.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.add_expansions++;
        cov_expr_stats.case_counts["ADD_b"]++;
#endif
        Expression cov_left = cov_expr(a, b->left(), depth + 1);
        Expression cov_right = cov_expr(a, b->right(), depth + 1);
        result = cov_left + cov_right;
    }
    // C-5.3: SUB linear expansion - Cov(A-B, X) = Cov(A,X) - Cov(B,X)
    else if (dynamic_cast<const OpSUB*>(a.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.sub_expansions++;
        cov_expr_stats.case_counts["SUB_a"]++;
#endif
        result = cov_expr(a->left(), b, depth + 1) - cov_expr(a->right(), b, depth + 1);
    } else if (dynamic_cast<const OpSUB*>(b.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.sub_expansions++;
        cov_expr_stats.case_counts["SUB_b"]++;
#endif
        result = cov_expr(a, b->left(), depth + 1) - cov_expr(a, b->right(), depth + 1);
    }
    // C-5.6: MAX expansion - MAX(A,B) = A + MAX0(B-A)
    else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.max_expansions++;
        cov_expr_stats.case_counts["MAX_a"]++;
#endif
        const RandomVariable& x = a->left();
        auto m = a.dynamic_pointer_cast<const OpMAX>();
        const RandomVariable& z = m->max0();
        result = cov_expr(x, b, depth + 1) + cov_expr(z, b, depth + 1);
    } else if (dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.max_expansions++;
        cov_expr_stats.case_counts["MAX_b"]++;
#endif
        const RandomVariable& x = b->left();
        auto m = b.dynamic_pointer_cast<const OpMAX>();
        const RandomVariable& z = m->max0();
        result = cov_expr(a, x, depth + 1) + cov_expr(a, z, depth + 1);
    }
    // Handle nested MAX0(MAX0(...)) - pass through
    else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
             dynamic_cast<const OpMAX0*>(a->left().get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.case_counts["NestedMAX0_a"]++;
#endif
        result = cov_expr(a->left(), b, depth + 1);
    } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr &&
               dynamic_cast<const OpMAX0*>(b->left().get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.case_counts["NestedMAX0_b"]++;
#endif
        result = cov_expr(a, b->left(), depth + 1);
    }
    // C-5.5: MAX0 × MAX0
    else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
             dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.max0_expansions++;
        if (a->left() == b->left()) {
            cov_expr_stats.case_counts["MAX0_MAX0_same"]++;
        } else {
            cov_expr_stats.case_counts["MAX0_MAX0_diff"]++;
        }
#endif
        if (a->left() == b->left()) {
            // max0(D) with itself: Cov = Var(max0(D))
            result = a->var_expr();
        } else {
            result = cov_max0_max0_expr(a, b);
        }
    }
    // C-5.4: MAX0 × X (X is not MAX0)
    else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.case_counts["MAX0_X_a"]++;
#endif
        result = cov_x_max0_expr(b, a);
    } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.case_counts["MAX0_X_b"]++;
#endif
        result = cov_x_max0_expr(a, b);
    }
    // C-5.2: Normal × Normal (independent)
    else if (dynamic_cast<const NormalImpl*>(a.get()) != nullptr &&
             dynamic_cast<const NormalImpl*>(b.get()) != nullptr) {
#ifdef DEBUG
        cov_expr_stats.normal_normal_cases++;
        cov_expr_stats.case_counts["Normal_Normal"]++;
#endif
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

        } else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr) {
            const RandomVariable& x = a->left();
            auto m = a.dynamic_pointer_cast<const OpMAX>();
            const RandomVariable& z = m->max0();
            double cov0 = covariance(z, b);
            double cov1 = covariance(x, b);
            cov = cov0 + cov1;

        } else if (dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
            const RandomVariable& x = b->left();
            auto m = b.dynamic_pointer_cast<const OpMAX>();
            const RandomVariable& z = m->max0();
            double cov0 = covariance(z, a);
            double cov1 = covariance(x, a);
            cov = cov0 + cov1;

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
