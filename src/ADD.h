// -*- c++ -*-
// Author: IWAI Jiro
// $Id: ADD.h,v 1.2 2008/11/16 17:27:18 jiro Exp $

#ifndef NH_ADD__H
#define NH_ADD__H

#include "RandomVariable.h"

namespace RandomVariable {

class OpADD : public _RandomVariable_ {
public:

    OpADD( const RandomVariable& left, const RandomVariable& righ );
    virtual ~OpADD();

private:

    virtual double calc_mean() const;
    virtual double calc_variance() const;
};

RandomVariable operator + (const RandomVariable& a, const RandomVariable& b);

}

#endif // NH_ADD__H
