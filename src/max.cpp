// -*- c++ -*-
// Author: IWAI Jiro

#include <cassert>
#include <cmath>
#include <memory>
#include "max.hpp"
#include "sub.hpp"
#include "covariance.hpp"
#include "util_numerical.hpp"
#include "exception.hpp"

namespace RandomVariable {

    /////

OpMAX::OpMAX(const RandomVariable& left, const RandomVariable& right)
    : _RandomVariable_(left, right), max0_(MAX0(right - left)) {
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
    _RandomVariable_::check_variance(r);
    return (r);
}

RandomVariable MAX(const RandomVariable& a, const RandomVariable& b) {
    return RandomVariable(std::make_shared<OpMAX>(a, b));
}

    /////

OpMAX0::OpMAX0(const RandomVariable& left)
    : _RandomVariable_(left, RandomVariable(nullptr)) {
    level_ = left->level() + 1;
}

OpMAX0::~OpMAX0() = default;

double OpMAX0::calc_mean() const {
    double mu = left()->mean();
    double va = left()->variance();
    if (va <= 0.0) {
        throw Nh::RuntimeException("OpMAX0::calc_mean: variance must be positive, got " + std::to_string(va));
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
        throw Nh::RuntimeException("OpMAX0::calc_variance: variance must be positive, got " + std::to_string(va));
    }
    double sg = sqrt(va);
    double ms = -mu / sg;
    double mm = MeanMax(ms);
    double mm2 = MeanMax2(ms);
    double r = va * (mm2 - mm * mm);
    _RandomVariable_::check_variance(r);
    return (r);
}

const RandomVariable& OpMAX0::right() const {
    return right_;
}

RandomVariable MAX0(const RandomVariable& a) {
    return RandomVariable(std::make_shared<OpMAX0>(a));
}
}
