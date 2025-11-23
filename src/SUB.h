// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_SUB__H
#define NH_SUB__H

#include "RandomVariable.h"

namespace RandomVariable {

    class OpSUB : public _RandomVariable_ {
    public:

		OpSUB( const RandomVariable& left, const RandomVariable& right );
		~OpSUB() override;

    private:

		[[nodiscard]] double calc_mean() const override;
		[[nodiscard]] double calc_variance() const override;
    };

    RandomVariable operator- (const RandomVariable& a, const RandomVariable& b);
}
#endif // NH_SUB__H
