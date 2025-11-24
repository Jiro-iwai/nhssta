// -*- c++ -*-
// Author: IWAI Jiro

#include <cassert>
#include <cmath>
#include <iostream>
#include "random_variable.hpp"

namespace RandomVariable {

    _RandomVariable_::_RandomVariable_():
        left_(nullptr),
        right_(nullptr),
        mean_(0),
        variance_(0),
        is_set_mean_(false),
        is_set_variance_(false),
        level_(0)
    {
#ifdef DEBUG
        std::cerr << "_RandomVariable_(" << this << ":";
        std::cerr << name_ << ") is creating" << std::endl;
#endif // DEBUG
    }

    _RandomVariable_::_RandomVariable_
    (
        double mean,
        double variance,
        const std::string& name
        ):
        name_(name),
        left_(nullptr),
        right_(nullptr),
        mean_(mean),
        variance_(variance),
        is_set_mean_(false),
        is_set_variance_(false),
        level_(0)
    {
#ifdef DEBUG
        std::cerr << "_RandomVariable_(" << this << ":";;
        std::cerr << name_ << ") is creating" << std::endl;
#endif // DEBUG
    }

    _RandomVariable_::_RandomVariable_
    (
        const RandomVariable& left,
        const RandomVariable& right,
        const std::string& name
        ):
        name_(name),
        left_(left),
        right_(right),
        mean_(0),
        variance_(0),
        is_set_mean_(false),
        is_set_variance_(false),
        level_(0)
    {
#ifdef DEBUG
        std::cerr << "_RandomVariable_(" << this << ":";
        std::cerr << name_ << ") is creating" << std::endl;
#endif // DEBUG
    }

    _RandomVariable_::~_RandomVariable_(){
#ifdef DEBUG
        std::cerr << "_RandomVariable_(" << this << ":";
        std::cerr << name_ << ") is deleting" << std::endl;
#endif // DEBUG
    }

    const std::string& _RandomVariable_::name() const {
        return name_;
    }

    void _RandomVariable_::set_name(const std::string& name) {
        name_ = name;
    }

    const RandomVariable& _RandomVariable_::left() const {
        return left_;
    }

    const RandomVariable& _RandomVariable_::right() const {
        return right_;
    }

    double _RandomVariable_::mean() {
        if( !is_set_mean_ ){
            mean_ = calc_mean();
            is_set_mean_ = true;
        }

        return mean_;
    }

    double _RandomVariable_::variance() {
        if( !is_set_variance_ ){
            variance_ = calc_variance();
            if( std::isnan(variance_) ) {
                assert(0);
            }
            is_set_variance_ = true;
        }

        return variance_;
    }

    double _RandomVariable_::calc_mean() const {
        return mean_;
    }

    double _RandomVariable_::calc_variance() const {
        return variance_;
    }

    double _RandomVariable_::standard_deviation() {
        double v = variance();
        return std::sqrt(v);
    }

    double _RandomVariable_::coefficient_of_variation() {
        double m = mean();
        if (std::abs(m) < 1.0e-10) {
            // For very small means, return a large value to indicate high relative error
            return std::numeric_limits<double>::infinity();
        }
        double sd = standard_deviation();
        return sd / std::abs(m);
    }

    double _RandomVariable_::relative_error() {
        return coefficient_of_variation();
    }

    void _RandomVariable_::check_variance(double& v) {
        if( fabs(v) < minimum_variance ) {
            v = minimum_variance;
        }
        if( v < 0.0 ) {
            throw Nh::RuntimeException("RandomVariable: negative variance");
        }
    }

}
