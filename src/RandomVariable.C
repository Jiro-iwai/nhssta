// -*- c++ -*-
// Author: IWAI Jiro

#include <cassert>
#include <cmath>
#include <iostream>
#include "RandomVariable.h"

namespace RandomVariable {

    _RandomVariable_::_RandomVariable_():
        name_(""),
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
        is_set_mean_(false),
        is_set_variance_(false)
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
            if( std::isnan(variance_) )
                assert(0);
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

    void _RandomVariable_::check_variance(double& v) const {
        if( fabs(v) < minimum_variance )
            v = minimum_variance;
        if( v < 0.0 )
            throw Nh::RuntimeException("RandomVariable: negative variance");
    }

}
