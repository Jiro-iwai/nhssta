// -*- c++ -*-
// Author: IWAI Jiro

#include <cassert>
#include <cmath>
#include "MAX.h"
#include "SUB.h"
#include "Covariance.h"
#include "Util.h"

namespace RandomVariable {

    /////

    OpMAX::OpMAX( const RandomVariable& left, const RandomVariable& right ) :
        _RandomVariable_(left,right),
        max0_(MAX0(right-left)) {
        level_ = std::max(left->level(),right->level())+1;
    }

    OpMAX::~OpMAX(){}

    double OpMAX::calc_mean() const {
        const RandomVariable& x = left();
        const RandomVariable& z = max0();
        double xm = x->mean();
        double zm = z->mean();
        return( xm + zm );
    }

    double OpMAX::calc_variance() const {
        const RandomVariable& x = left();
        const RandomVariable& z = max0();
        double xv = x->variance();
        double zv = z->variance();
        double cov = covariance(x,z);
        double r = xv + 2.0*cov + zv;
        check_variance(r);
        return(r);
    }

    RandomVariable MAX(const RandomVariable& a, const RandomVariable& b) {
        return SmartPtr<OpMAX>( new OpMAX( a, b) );
    }

    /////

    OpMAX0::OpMAX0( const RandomVariable& left ) :
        _RandomVariable_(left,RandomVariable(0)){
        level_ = left->level()+1;
    }

    OpMAX0::~OpMAX0(){}

    double OpMAX0::calc_mean() const {
        double mu = left()->mean();
        double va = left()->variance();
        assert( 0.0 < va );
        double sg = sqrt(va);
        double ms = -mu/sg;
        double mm = MeanMax(ms);
        double r = mu + sg*mm;
        return (r);
    }

    double OpMAX0::calc_variance() const {
        double mu = left()->mean();
        double va = left()->variance();
        assert( 0.0 < va );
        double sg = sqrt(va);
        double ms = -mu/sg;
        double mm = MeanMax(ms);
        double mm2 = MeanMax2(ms);
        double r = va * ( mm2 - mm*mm );
        check_variance(r);
        return (r);
    }

    const RandomVariable& OpMAX0::right() const {
        return right_;
    }

    RandomVariable MAX0(const RandomVariable& a) {
        return SmartPtr<OpMAX0>( new OpMAX0(a) );
    }
}
