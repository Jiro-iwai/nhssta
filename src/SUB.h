// -*- c++ -*-
// Author: IWAI Jiro
// $Id: SUB.h,v 1.1 2007/08/14 18:01:20 jiro Exp $

#ifndef NH_SUB__H
#define NH_SUB__H

#include "RandomVariable.h"

namespace RandomVariable {

    class OpSUB : public _RandomVariable_ {
    public:

	OpSUB( const RandomVariable& left, const RandomVariable& righ );
	virtual ~OpSUB();

    private:

	virtual double calc_mean() const;
	virtual double calc_variance() const;
    };

    RandomVariable operator- (const RandomVariable& a, const RandomVariable& b);
}
#endif // NH_SUB__H
