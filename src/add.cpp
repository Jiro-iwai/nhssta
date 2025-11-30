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

Expression OpADD::calc_mean_expr() const {
    // E[A + B] = E[A] + E[B]
    return left()->mean_expr() + right()->mean_expr();
}

Expression OpADD::calc_var_expr() const {
    // Var[A + B] = Var[A] + 2*Cov(A,B) + Var[B]
    Expression var_left = left()->var_expr();
    Expression var_right = right()->var_expr();
    Expression cov = cov_expr(left(), right());
    return var_left + Const(2.0) * cov + var_right;
}

RandomVariable operator+(const RandomVariable& a, const RandomVariable& b) {
    return RandomVariable(std::make_shared<OpADD>(a, b));
}
}  // namespace RandomVariable
