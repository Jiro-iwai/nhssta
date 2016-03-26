// -*- c++ -*-
// Author: IWAI Jiro

#ifndef MAX__H
#define MAX__H

#include "RandomVariable.h"

namespace RandomVariable {

    //////

    class OpMAX0;

    class OpMAX : public _RandomVariable_ {

    public:

		OpMAX( const RandomVariable& left, const RandomVariable& righ );
		virtual ~OpMAX();

		const RandomVariable& max0() const { return max0_; }

    private:

		virtual double calc_mean() const ;
		virtual double calc_variance() const;

		RandomVariable max0_;
    };

    RandomVariable MAX(const RandomVariable& a, const RandomVariable& b);

    //////

    class OpMAX0 : public _RandomVariable_ {
    public:

		OpMAX0( const RandomVariable& left );
		virtual ~OpMAX0();

    private:

		virtual double calc_mean() const ;
		virtual double calc_variance() const;
		virtual const RandomVariable& right() const;
    };

    RandomVariable MAX0(const RandomVariable& a);
}

#endif // MAX__H
