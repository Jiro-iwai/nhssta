// -*- c++ -*-
// Author: IWAI Jiro

#include "add.hpp"

#include <memory>

#include "covariance.hpp"

namespace RandomVariable {

OpADD::OpADD(const RandomVariable& left, const RandomVariable& right)
    : RandomVariableImpl(left, right) {
    level_ = std::max(left->level(), right->level());
}

OpADD::~OpADD() = default;

double OpADD::calc_mean() const {
    double lm = left()->mean();
    double rm = right()->mean();
    return (lm + rm);
}

double OpADD::calc_variance() const {
    double lv = left()->variance();
    double rv = right()->variance();
    double cov = covariance(left(), right());
    double r = lv + (2.0 * cov) + rv;
    RandomVariableImpl::check_variance(r);
    return (r);
}

RandomVariable operator+(const RandomVariable& a, const RandomVariable& b) {
    return RandomVariable(std::make_shared<OpADD>(a, b));
}
}  // namespace RandomVariable
