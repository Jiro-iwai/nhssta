// -*- c++ -*-
// Author: IWAI Jiro

#include "max.hpp"

#include <cassert>
#include <cmath>
#include <memory>
#include <nhssta/exception.hpp>

#include "covariance.hpp"
#include "sub.hpp"
#include "util_numerical.hpp"

namespace RandomVariable {

/////

OpMAX::OpMAX(const RandomVariable& left, const RandomVariable& right)
    : RandomVariableImpl(left, right)
    , max0_(MAX0(right - left)) {
    level_ = std::max(left->level(), right->level()) + 1;
}

OpMAX::~OpMAX() = default;

double OpMAX::calc_mean() const {
    const RandomVariable& x = left();
    const RandomVariable& z = max0();
    double xm = x->mean();
    double zm = z->mean();
    return (xm + zm);
}

double OpMAX::calc_variance() const {
    const RandomVariable& x = left();
    const RandomVariable& z = max0();
    double xv = x->variance();
    double zv = z->variance();
    double cov = covariance(x, z);
    double r = xv + (2.0 * cov) + zv;
    RandomVariableImpl::check_variance(r);
    return (r);
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

RandomVariable MAX0(const RandomVariable& a) {
    return RandomVariable(std::make_shared<OpMAX0>(a));
}
}  // namespace RandomVariable
