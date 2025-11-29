// -*- c++ -*-
// Author: IWAI Jiro

#include "random_variable.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace RandomVariable {

RandomVariableImpl::RandomVariableImpl()
    : left_(nullptr)
    , right_(nullptr)
    , mean_(0)
    , variance_(0)
    , is_set_mean_(false)
    , is_set_variance_(false)
    , level_(0) {
#ifdef DEBUG
    std::cerr << "RandomVariableImpl(" << this << ":";
    std::cerr << name_ << ") is creating" << std::endl;
#endif  // DEBUG
}

RandomVariableImpl::RandomVariableImpl(double mean, double variance, const std::string& name)
    : name_(name)
    , left_(nullptr)
    , right_(nullptr)
    , mean_(mean)
    , variance_(variance)
    , is_set_mean_(false)
    , is_set_variance_(false)
    , level_(0) {
#ifdef DEBUG
    std::cerr << "RandomVariableImpl(" << this << ":";
    std::cerr << name_ << ") is creating" << std::endl;
#endif  // DEBUG
}

RandomVariableImpl::RandomVariableImpl(const RandomVariable& left, const RandomVariable& right,
                                   const std::string& name)
    : name_(name)
    , left_(left)
    , right_(right)
    , mean_(0)
    , variance_(0)
    , is_set_mean_(false)
    , is_set_variance_(false)
    , level_(0) {
#ifdef DEBUG
    std::cerr << "RandomVariableImpl(" << this << ":";
    std::cerr << name_ << ") is creating" << std::endl;
#endif  // DEBUG
}

RandomVariableImpl::~RandomVariableImpl() {
#ifdef DEBUG
    std::cerr << "RandomVariableImpl(" << this << ":";
    std::cerr << name_ << ") is deleting" << std::endl;
#endif  // DEBUG
}

const std::string& RandomVariableImpl::name() const {
    return name_;
}

void RandomVariableImpl::set_name(const std::string& name) {
    name_ = name;
}

const RandomVariable& RandomVariableImpl::left() const {
    return left_;
}

const RandomVariable& RandomVariableImpl::right() const {
    return right_;
}

double RandomVariableImpl::mean() const {
    if (!is_set_mean_) {
        mean_ = calc_mean();
        is_set_mean_ = true;
    }

    return mean_;
}

double RandomVariableImpl::variance() const {
    if (!is_set_variance_) {
        variance_ = calc_variance();
        if (std::isnan(variance_)) {
            throw Nh::RuntimeException("RandomVariable: variance calculation resulted in NaN");
        }
        is_set_variance_ = true;
    }

    return variance_;
}

double RandomVariableImpl::calc_mean() const {
    return mean_;
}

double RandomVariableImpl::calc_variance() const {
    return variance_;
}

double RandomVariableImpl::standard_deviation() const {
    double v = variance();
    return std::sqrt(v);
}

double RandomVariableImpl::coefficient_of_variation() const {
    double m = mean();
    if (std::abs(m) < CV_ZERO_THRESHOLD) {
        // For very small means, return a large value to indicate high relative error
        return std::numeric_limits<double>::infinity();
    }
    double sd = standard_deviation();
    return sd / std::abs(m);
}

double RandomVariableImpl::relative_error() const {
    return coefficient_of_variation();
}

void RandomVariableImpl::check_variance(double& v) {
    if (fabs(v) < MINIMUM_VARIANCE) {
        v = MINIMUM_VARIANCE;
    }
    if (v < 0.0) {
        throw Nh::RuntimeException("RandomVariable: negative variance");
    }
}

// Default implementations for Expression-based methods
// Derived classes should override these for sensitivity analysis support

Expression RandomVariableImpl::mean_expr() const {
    throw Nh::RuntimeException("mean_expr() not implemented for this RandomVariable type");
}

Expression RandomVariableImpl::var_expr() const {
    throw Nh::RuntimeException("var_expr() not implemented for this RandomVariable type");
}

Expression RandomVariableImpl::std_expr() const {
    // Default: sqrt(var_expr())
    return sqrt(var_expr());
}

}  // namespace RandomVariable
