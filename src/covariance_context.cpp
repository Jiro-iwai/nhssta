// -*- c++ -*-
// Author: IWAI Jiro

#include "covariance_context.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <nhssta/exception.hpp>

#include "add.hpp"
#include "max.hpp"
#include "statistical_functions.hpp"
#include "statistics.hpp"
#include "sub.hpp"
#include "util_numerical.hpp"

namespace RandomVariable {

// ============================================================================
// CovarianceContextImpl - Private implementation
// ============================================================================

class CovarianceContextImpl {
   public:
    CovarianceContextImpl()
        : matrix_(std::make_shared<CovarianceMatrixImpl>()) {}

    // Covariance matrix for double values
    CovarianceMatrix matrix_;

    // Cache for Expression-based covariance
    std::unordered_map<std::pair<RandomVariable, RandomVariable>, Expression, PairHash>
        expr_cache_;

    // ========================================================================
    // Helper functions (same logic as in covariance.cpp)
    // These are static as they don't access instance data
    // ========================================================================

    // covariance_x_max0_y: Computes Cov(x, max(0, Z))
    static double covariance_x_max0_y(const RandomVariable& x, const RandomVariable& y);

    // covariance_max0_max0: Computes Cov(max(0,D0), max(0,D1))
    static double covariance_max0_max0(const RandomVariable& a, const RandomVariable& b);

    // covariance_max_max: Computes Cov(MAX(A,B), MAX(C,D))
    static double covariance_max_max(const RandomVariable& max1, const RandomVariable& max2);

    // covariance_max_w: Computes Cov(MAX(A,B), W)
    static double covariance_max_w(const RandomVariable& max_ab, const RandomVariable& w);

    // check_covariance: Clamps covariance to valid range
    static void check_covariance(double& cov, const RandomVariable& a, const RandomVariable& b);

    // ========================================================================
    // Expression-based helper functions
    // These are static as they don't access instance data
    // ========================================================================

    static Expression cov_x_max0_expr(const RandomVariable& x, const RandomVariable& y_max0);
    static Expression cov_max0_max0_expr(const RandomVariable& a, const RandomVariable& b);
    static Expression cov_max_max_expr(const RandomVariable& max1, const RandomVariable& max2);
    static Expression cov_max_w_expr(const RandomVariable& max_ab, const RandomVariable& w);
};

// ============================================================================
// Helper function implementations
// ============================================================================

double CovarianceContextImpl::covariance_x_max0_y(const RandomVariable& x,
                                                  const RandomVariable& y) {
    const auto* y_max0 = dynamic_cast<const OpMAX0*>(y.get());
    if (y_max0 == nullptr) {
        throw Nh::RuntimeException("covariance_x_max0_y: y must be OpMAX0 type");
    }
    const RandomVariable& z = y->left();
    double c = covariance(x, z);  // Recursive call to context's covariance
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
    return cov;
}

double CovarianceContextImpl::covariance_max0_max0(const RandomVariable& a,
                                                   const RandomVariable& b) {
    const RandomVariable& d0 = a->left();
    const RandomVariable& d1 = b->left();

    double mu0 = d0->mean();
    double v0 = d0->variance();
    if (v0 <= 0.0) {
        throw Nh::RuntimeException("covariance_max0_max0: D0 variance must be positive");
    }
    double sigma0 = sqrt(v0);

    double mu1 = d1->mean();
    double v1 = d1->variance();
    if (v1 <= 0.0) {
        throw Nh::RuntimeException("covariance_max0_max0: D1 variance must be positive");
    }
    double sigma1 = sqrt(v1);

    double covD = covariance(d0, d1);  // Recursive call
    if (std::isnan(covD) || std::isinf(covD)) {
        throw Nh::RuntimeException("covariance_max0_max0: covariance(d0, d1) is NaN or Inf");
    }

    if (sigma0 * sigma1 <= 0.0) {
        throw Nh::RuntimeException("covariance_max0_max0: sigma0 * sigma1 must be positive");
    }
    double rho = covD / (sigma0 * sigma1);
    rho = std::min(rho, 1.0);
    rho = std::max(rho, -1.0);

    double E0pos = expected_positive_part(mu0, sigma0);
    double E1pos = expected_positive_part(mu1, sigma1);
    double Eprod = expected_prod_pos(mu0, sigma0, mu1, sigma1, rho);

    double cov = Eprod - (E0pos * E1pos);

    if (std::isnan(cov)) {
        throw Nh::RuntimeException("covariance_max0_max0: result is NaN");
    }

    return cov;
}

double CovarianceContextImpl::covariance_max_max(const RandomVariable& max1,
                                                 const RandomVariable& max2) {
    const auto* max1_ptr = dynamic_cast<const OpMAX*>(max1.get());
    const auto* max2_ptr = dynamic_cast<const OpMAX*>(max2.get());

    if (max1_ptr == nullptr || max2_ptr == nullptr) {
        throw Nh::RuntimeException("covariance_max_max: both arguments must be OpMAX type");
    }

    const RandomVariable& A = max1->left();
    const RandomVariable& B = max1->right();
    const RandomVariable& C = max2->left();
    const RandomVariable& D = max2->right();

    double mu_A = A->mean();
    double mu_B = B->mean();
    double var_A = A->variance();
    double var_B = B->variance();
    double cov_AB = covariance(A, B);

    double theta1_sq = var_A + var_B - (2.0 * cov_AB);
    theta1_sq = std::max(theta1_sq, MINIMUM_VARIANCE);
    double theta1 = std::sqrt(theta1_sq);
    double alpha1 = (mu_A - mu_B) / theta1;
    double T1 = normal_cdf(alpha1);

    double mu_C = C->mean();
    double mu_D = D->mean();
    double var_C = C->variance();
    double var_D = D->variance();
    double cov_CD = covariance(C, D);

    double theta2_sq = var_C + var_D - (2.0 * cov_CD);
    theta2_sq = std::max(theta2_sq, MINIMUM_VARIANCE);
    double theta2 = std::sqrt(theta2_sq);
    double alpha2 = (mu_C - mu_D) / theta2;
    double T2 = normal_cdf(alpha2);

    double cov_AC = covariance(A, C);
    double cov_AD = covariance(A, D);
    double cov_BC = covariance(B, C);
    double cov_BD = covariance(B, D);

    double cov = (T1 * T2 * cov_AC) +
                 (T1 * (1.0 - T2) * cov_AD) +
                 ((1.0 - T1) * T2 * cov_BC) +
                 ((1.0 - T1) * (1.0 - T2) * cov_BD);

    if (std::isnan(cov)) {
        throw Nh::RuntimeException("covariance_max_max: result is NaN");
    }

    return cov;
}

double CovarianceContextImpl::covariance_max_w(const RandomVariable& max_ab,
                                               const RandomVariable& w) {
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

    double theta_sq = var_A + var_B - (2.0 * cov_AB);
    theta_sq = std::max(theta_sq, MINIMUM_VARIANCE);
    double theta = std::sqrt(theta_sq);

    double alpha = (mu_A - mu_B) / theta;
    double T = normal_cdf(alpha);

    double cov_AW = covariance(A, w);
    double cov_BW = covariance(B, w);
    double cov = (T * cov_AW) + ((1.0 - T) * cov_BW);

    if (std::isnan(cov)) {
        throw Nh::RuntimeException("covariance_max_w: result is NaN");
    }

    return cov;
}

void CovarianceContextImpl::check_covariance(double& cov, const RandomVariable& a,
                                             const RandomVariable& b) {
    double var_a = a->variance();
    double var_b = b->variance();
    if (var_a < 0.0 || var_b < 0.0) {
        throw Nh::RuntimeException("check_covariance: variance must be non-negative");
    }
    double max_cov = std::sqrt(var_a * var_b);
    cov = std::clamp(cov, -max_cov, max_cov);
}

// ============================================================================
// Expression-based helper implementations
// ============================================================================

Expression CovarianceContextImpl::cov_x_max0_expr(const RandomVariable& x,
                                                  const RandomVariable& y_max0) {
    const RandomVariable& z = y_max0->left();

    Expression cov_xz = cov_expr(x, z);
    Expression mu_z = z->mean_expr();
    Expression sigma_z = z->std_expr();

    return cov_xz * Phi_expr(mu_z / sigma_z);
}

Expression CovarianceContextImpl::cov_max0_max0_expr(const RandomVariable& a,
                                                     const RandomVariable& b) {
    const RandomVariable& d0 = a->left();
    const RandomVariable& d1 = b->left();

    Expression mu0 = d0->mean_expr();
    Expression sigma0 = d0->std_expr();
    Expression mu1 = d1->mean_expr();
    Expression sigma1 = d1->std_expr();

    Expression cov_d0d1 = cov_expr(d0, d1);
    double rho_val = cov_d0d1->value() / (sigma0->value() * sigma1->value());

    Expression E_D0_pos = max0_mean_expr(mu0, sigma0);
    Expression E_D1_pos = max0_mean_expr(mu1, sigma1);

    constexpr double RHO_THRESHOLD = 0.9999;

    Expression E_prod;
    if (rho_val > RHO_THRESHOLD) {
        E_prod = expected_prod_pos_rho1_expr(mu0, sigma0, mu1, sigma1);
    } else if (rho_val < -RHO_THRESHOLD) {
        E_prod = expected_prod_pos_rho_neg1_expr(mu0, sigma0, mu1, sigma1);
    } else {
        Expression rho = cov_d0d1 / (sigma0 * sigma1);
        E_prod = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    }

    Expression result = E_prod - E_D0_pos * E_D1_pos;
    return result;
}

Expression CovarianceContextImpl::cov_max_max_expr(const RandomVariable& max1,
                                                   const RandomVariable& max2) {
    const RandomVariable& A = max1->left();
    const RandomVariable& B = max1->right();
    const RandomVariable& C = max2->left();
    const RandomVariable& D = max2->right();

    Expression mu_A = A->mean_expr();
    Expression mu_B = B->mean_expr();
    Expression var_A = A->var_expr();
    Expression var_B = B->var_expr();
    Expression cov_AB = cov_expr(A, B);

    Expression theta1_sq = var_A + var_B - Const(2.0) * cov_AB + Const(MINIMUM_VARIANCE);
    Expression theta1 = sqrt(theta1_sq);
    Expression alpha1 = (mu_A - mu_B) / theta1;
    Expression T1 = Phi_expr(alpha1);

    Expression mu_C = C->mean_expr();
    Expression mu_D = D->mean_expr();
    Expression var_C = C->var_expr();
    Expression var_D = D->var_expr();
    Expression cov_CD = cov_expr(C, D);

    Expression theta2_sq = var_C + var_D - Const(2.0) * cov_CD + Const(MINIMUM_VARIANCE);
    Expression theta2 = sqrt(theta2_sq);
    Expression alpha2 = (mu_C - mu_D) / theta2;
    Expression T2 = Phi_expr(alpha2);

    Expression cov_AC = cov_expr(A, C);
    Expression cov_AD = cov_expr(A, D);
    Expression cov_BC = cov_expr(B, C);
    Expression cov_BD = cov_expr(B, D);

    return T1 * T2 * cov_AC +
           T1 * (Const(1.0) - T2) * cov_AD +
           (Const(1.0) - T1) * T2 * cov_BC +
           (Const(1.0) - T1) * (Const(1.0) - T2) * cov_BD;
}

Expression CovarianceContextImpl::cov_max_w_expr(const RandomVariable& max_ab,
                                                 const RandomVariable& w) {
    const RandomVariable& A = max_ab->left();
    const RandomVariable& B = max_ab->right();

    Expression mu_A = A->mean_expr();
    Expression mu_B = B->mean_expr();
    Expression var_A = A->var_expr();
    Expression var_B = B->var_expr();
    Expression cov_AB = cov_expr(A, B);

    Expression theta_sq = var_A + var_B - Const(2.0) * cov_AB + Const(MINIMUM_VARIANCE);
    Expression theta = sqrt(theta_sq);
    Expression alpha = (mu_A - mu_B) / theta;
    Expression T = Phi_expr(alpha);

    Expression cov_AW = cov_expr(A, w);
    Expression cov_BW = cov_expr(B, w);
    return T * cov_AW + (Const(1.0) - T) * cov_BW;
}

// ============================================================================
// CovarianceContext public implementation
// ============================================================================

CovarianceContext::CovarianceContext()
    : impl_(std::make_unique<CovarianceContextImpl>()) {}

CovarianceContext::~CovarianceContext() = default;

double CovarianceContext::covariance(const Normal& a, const Normal& b) {
    // Delegate to the RandomVariable overload for consistent caching behavior
    return covariance(static_cast<const RandomVariable&>(a),
                     static_cast<const RandomVariable&>(b));
}

double CovarianceContext::covariance(const RandomVariable& a, const RandomVariable& b) {
    double cov = 0.0;
    bool cached = impl_->matrix_->lookup(a, b, cov);

    if (!cached) {
        // Cache miss - compute covariance
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
            cov = CovarianceContextImpl::covariance_max_max(a, b);

        } else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr) {
            cov = CovarianceContextImpl::covariance_max_w(a, b);

        } else if (dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
            cov = CovarianceContextImpl::covariance_max_w(b, a);

        } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
                   dynamic_cast<const OpMAX0*>(a->left().get()) != nullptr) {
            cov = covariance(a->left(), b);

        } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr &&
                   dynamic_cast<const OpMAX0*>(b->left().get()) != nullptr) {
            cov = covariance(a, b->left());

        } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
                   dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
            if (a->left() == b->left()) {
                cov = a->variance();
            } else {
                cov = CovarianceContextImpl::covariance_max0_max0(a, b);
            }

        } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr) {
            cov = CovarianceContextImpl::covariance_x_max0_y(b, a);

        } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
            cov = CovarianceContextImpl::covariance_x_max0_y(a, b);

        } else if (dynamic_cast<const NormalImpl*>(a.get()) != nullptr &&
                   dynamic_cast<const NormalImpl*>(b.get()) != nullptr) {
            cov = 0.0;

        } else {
            throw Nh::RuntimeException(
                "covariance: unsupported RandomVariable type combination for covariance "
                "calculation");
        }

        CovarianceContextImpl::check_covariance(cov, a, b);
    }

    // Always store in cache to ensure consistent behavior
    // lookup() may compute values for Normal types without caching them
    impl_->matrix_->set(a, b, cov);

    return cov;
}

Expression CovarianceContext::cov_expr(const RandomVariable& a, const RandomVariable& b) {
    // Check cache first
    auto it = impl_->expr_cache_.find({a, b});
    if (it != impl_->expr_cache_.end()) {
        return it->second;
    }
    it = impl_->expr_cache_.find({b, a});
    if (it != impl_->expr_cache_.end()) {
        return it->second;
    }

    Expression result;

    // Same logic as in covariance.cpp cov_expr()
    if (a == b) {
        result = a->var_expr();
    } else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
        Expression cov_left = cov_expr(a->left(), b);
        Expression cov_right = cov_expr(a->right(), b);
        result = cov_left + cov_right;
    } else if (dynamic_cast<const OpADD*>(b.get()) != nullptr) {
        Expression cov_left = cov_expr(a, b->left());
        Expression cov_right = cov_expr(a, b->right());
        result = cov_left + cov_right;
    } else if (dynamic_cast<const OpSUB*>(a.get()) != nullptr) {
        result = cov_expr(a->left(), b) - cov_expr(a->right(), b);
    } else if (dynamic_cast<const OpSUB*>(b.get()) != nullptr) {
        result = cov_expr(a, b->left()) - cov_expr(a, b->right());
    } else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr &&
               dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
        result = CovarianceContextImpl::cov_max_max_expr(a, b);
    } else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr) {
        result = CovarianceContextImpl::cov_max_w_expr(a, b);
    } else if (dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
        result = CovarianceContextImpl::cov_max_w_expr(b, a);
    } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
               dynamic_cast<const OpMAX0*>(a->left().get()) != nullptr) {
        result = cov_expr(a->left(), b);
    } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr &&
               dynamic_cast<const OpMAX0*>(b->left().get()) != nullptr) {
        result = cov_expr(a, b->left());
    } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
               dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
        if (a->left() == b->left()) {
            result = a->var_expr();
        } else {
            result = CovarianceContextImpl::cov_max0_max0_expr(a, b);
        }
    } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr) {
        result = CovarianceContextImpl::cov_x_max0_expr(b, a);
    } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
        result = CovarianceContextImpl::cov_x_max0_expr(a, b);
    } else if (dynamic_cast<const NormalImpl*>(a.get()) != nullptr &&
               dynamic_cast<const NormalImpl*>(b.get()) != nullptr) {
        result = Const(0.0);
    } else {
        throw Nh::RuntimeException(
            "cov_expr: unsupported RandomVariable type combination");
    }

    // Cache the result
    impl_->expr_cache_[{a, b}] = result;

    return result;
}

void CovarianceContext::clear() {
    impl_->matrix_->clear();
}

size_t CovarianceContext::cache_size() const {
    return impl_->matrix_->size();
}

void CovarianceContext::clear_expr_cache() {
    impl_->expr_cache_.clear();
}

size_t CovarianceContext::expr_cache_size() const {
    return impl_->expr_cache_.size();
}

void CovarianceContext::clear_all_caches() {
    clear();
    clear_expr_cache();
}

CovarianceMatrix& CovarianceContext::matrix() {
    return impl_->matrix_;
}

}  // namespace RandomVariable
