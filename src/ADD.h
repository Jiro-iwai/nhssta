// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_ADD__H
#define NH_ADD__H

#include "RandomVariable.h"

namespace RandomVariable {

	class OpADD : public _RandomVariable_ {
	public:

		OpADD( const RandomVariable& left, const RandomVariable& right );
		~OpADD() override;

	private:

		[[nodiscard]] double calc_mean() const override;
		[[nodiscard]] double calc_variance() const override;
	};

	RandomVariable operator + (const RandomVariable& a, const RandomVariable& b);

}

#endif // NH_ADD__H
