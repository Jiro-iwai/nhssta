// -*- c++ -*-
// Author: IWAI Jiro

#include "max.hpp"

#include <cassert>
#include <cmath>
#include <memory>
#include <nhssta/exception.hpp>

#include "covariance.hpp"
#include "statistical_functions.hpp"
#include "sub.hpp"
#include "util_numerical.hpp"

namespace RandomVariable {

/////

OpMAX::OpMAX(const RandomVariable& left, const RandomVariable& right)
    : RandomVariableImpl(left, right) {
    level_ = std::max(left->level(), right->level()) + 1;
}

OpMAX::~OpMAX() = default;

double OpMAX::calc_mean() const {
    // Clark formula for MAX(A, B): Direct calculation without MAX0 decomposition
    // μ_Z = μ_A * T + μ_B * (1-T) + θ * p
    // where:
    //   θ² = Var(A) + Var(B) - 2*Cov(A,B)
    //   α = (μ_A - μ_B) / θ
    //   T = Φ(α), p = φ(α)

    const RandomVariable& A = left();
    const RandomVariable& B = right();

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

    // T = Φ(α), p = φ(α)
    double T = normal_cdf(alpha);
    double p = normal_pdf(alpha);

    // μ_Z = μ_A * T + μ_B * (1-T) + θ * p
    double mu_Z = mu_A * T + mu_B * (1.0 - T) + theta * p;

    return mu_Z;
}

double OpMAX::calc_variance() const {
    // Clark formula for Var[MAX(A, B)]: Direct calculation without MAX0 decomposition
    // E[Z²] = (Var(A) + μ_A²) * T + (Var(B) + μ_B²) * (1-T) + (μ_A + μ_B) * θ * p
    // Var(Z) = E[Z²] - μ_Z²

    const RandomVariable& A = left();
    const RandomVariable& B = right();

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

    // T = Φ(α), p = φ(α)
    double T = normal_cdf(alpha);
    double p = normal_pdf(alpha);

    // E[Z²] = (Var(A) + μ_A²) * T + (Var(B) + μ_B²) * (1-T) + (μ_A + μ_B) * θ * p
    double E_Z_sq = (var_A + mu_A * mu_A) * T +
                     (var_B + mu_B * mu_B) * (1.0 - T) +
                     (mu_A + mu_B) * theta * p;

    // μ_Z (recompute to avoid dependency on calc_mean())
    double mu_Z = mu_A * T + mu_B * (1.0 - T) + theta * p;

    // Var(Z) = E[Z²] - μ_Z²
    double var_Z = E_Z_sq - mu_Z * mu_Z;

    RandomVariableImpl::check_variance(var_Z);
    return var_Z;
}

Expression OpMAX::calc_mean_expr() const {
    // Clark formula for MAX(A, B): Direct calculation without MAX0 decomposition
    // μ_Z = μ_A * T + μ_B * (1-T) + θ * p
    // where T = Φ(α), p = φ(α), α = (μ_A - μ_B) / θ

    const RandomVariable& A = left();
    const RandomVariable& B = right();

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

    // T = Φ(α), p = φ(α)
    Expression T = Phi_expr(alpha);
    Expression p = phi_expr(alpha);

    // μ_Z = μ_A * T + μ_B * (1-T) + θ * p
    return mu_A * T + mu_B * (Const(1.0) - T) + theta * p;
}

Expression OpMAX::calc_var_expr() const {
    // Clark formula for Var[MAX(A, B)]: Direct calculation without MAX0 decomposition
    // E[Z²] = (Var(A) + μ_A²) * T + (Var(B) + μ_B²) * (1-T) + (μ_A + μ_B) * θ * p
    // Var(Z) = E[Z²] - μ_Z²

    const RandomVariable& A = left();
    const RandomVariable& B = right();

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

    // T = Φ(α), p = φ(α)
    Expression T = Phi_expr(alpha);
    Expression p = phi_expr(alpha);

    // E[Z²] = (Var(A) + μ_A²) * T + (Var(B) + μ_B²) * (1-T) + (μ_A + μ_B) * θ * p
    Expression E_Z_sq = (var_A + mu_A * mu_A) * T +
                         (var_B + mu_B * mu_B) * (Const(1.0) - T) +
                         (mu_A + mu_B) * theta * p;

    // μ_Z (recompute to maintain expression tree consistency)
    Expression mu_Z = mu_A * T + mu_B * (Const(1.0) - T) + theta * p;

    // Var(Z) = E[Z²] - μ_Z²
    return E_Z_sq - mu_Z * mu_Z;
}

RandomVariable MAX(const RandomVariable& a, const RandomVariable& b) {
    // Fix for Issue #158: Normalize order so that the variable with larger mean
    // is always the left (base) argument. This ensures consistent decomposition
    // regardless of the order the caller provides.
    //
    // Clark approximation: MAX(A,B) = A + MAX0(B-A)
    // When A > B in mean, B-A has negative mean, so MAX0(B-A) ≈ 0
    // This gives better numerical stability than the reverse case.
    if (a->mean() >= b->mean()) {
        return RandomVariable(std::make_shared<OpMAX>(a, b));
    }
    return RandomVariable(std::make_shared<OpMAX>(b, a));
}

/////

OpMAX0::OpMAX0(const RandomVariable& left)
    : RandomVariableImpl(left, RandomVariable(nullptr)) {
    level_ = left->level() + 1;
}

OpMAX0::~OpMAX0() = default;

double OpMAX0::calc_mean() const {
    double mu = left()->mean();
    double va = left()->variance();
    if (va <= 0.0) {
        throw Nh::RuntimeException("OpMAX0::calc_mean: variance must be positive, got " +
                                   std::to_string(va));
    }
    double sg = sqrt(va);
    double ms = -mu / sg;
    double mm = MeanMax(ms);
    double r = mu + (sg * mm);
    return (r);
}

double OpMAX0::calc_variance() const {
    double mu = left()->mean();
    double va = left()->variance();
    if (va <= 0.0) {
        throw Nh::RuntimeException("OpMAX0::calc_variance: variance must be positive, got " +
                                   std::to_string(va));
    }
    double sg = sqrt(va);
    double ms = -mu / sg;
    double mm = MeanMax(ms);
    double mm2 = MeanMax2(ms);
    double r = va * (mm2 - mm * mm);
    RandomVariableImpl::check_variance(r);
    return (r);
}

const RandomVariable& OpMAX0::right() const {
    return right_;
}

Expression OpMAX0::calc_mean_expr() const {
    // E[max(0,D)] = μ + σ·MeanMax(-μ/σ)
    // Use max0_mean_expr from expression.hpp
    Expression mu = left()->mean_expr();
    Expression sigma = left()->std_expr();
    return max0_mean_expr(mu, sigma);
}

Expression OpMAX0::calc_var_expr() const {
    // Var[max(0,D)] = σ²·(MeanMax2(-μ/σ) - MeanMax(-μ/σ)²)
    // Use max0_var_expr from expression.hpp
    Expression mu = left()->mean_expr();
    Expression sigma = left()->std_expr();
    return max0_var_expr(mu, sigma);
}

RandomVariable MAX0(const RandomVariable& a) {
    return RandomVariable(std::make_shared<OpMAX0>(a));
}
}  // namespace RandomVariable
