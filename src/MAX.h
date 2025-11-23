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

		OpMAX( const RandomVariable& left, const RandomVariable& right );
		~OpMAX() override;

		const RandomVariable& max0() const { return max0_; }

    private:

		[[nodiscard]] double calc_mean() const override;
		[[nodiscard]] double calc_variance() const override;

		RandomVariable max0_;
    };

    RandomVariable MAX(const RandomVariable& a, const RandomVariable& b);

    //////

    class OpMAX0 : public _RandomVariable_ {
    public:

		OpMAX0( const RandomVariable& left );
		~OpMAX0() override;

    private:

		[[nodiscard]] double calc_mean() const override;
		[[nodiscard]] double calc_variance() const override;
		const RandomVariable& right() const;
    };

    RandomVariable MAX0(const RandomVariable& a);
}

#endif // MAX__H
