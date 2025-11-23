// -*- c++ -*-
// Author: IWAI Jiro

#include <memory>
#include "SUB.h"
#include "Covariance.h"

namespace RandomVariable {

    OpSUB::OpSUB( const RandomVariable& left,
                  const RandomVariable& right ) :
        _RandomVariable_(left,right){
        level_ = std::max(left->level(),right->level());
    }

    OpSUB::~OpSUB() = default;

    double OpSUB::calc_mean() const {
        double lm = left()->mean();
        double rm = right()->mean();
        return ( lm - rm );
    }

    double OpSUB::calc_variance() const {
        double lv = left()->variance();
        double rv = right()->variance();
        double cov = covariance(left(),right());
        double r = lv - (2.0 * cov) + rv;
        check_variance(r);
        return (r);
    }

    RandomVariable operator- (const RandomVariable& a, const RandomVariable& b){
        return RandomVariable(std::make_shared<OpSUB>(a, b));
    }
}

