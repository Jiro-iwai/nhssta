// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NORMAL__H
#define NORMAL__H

#include "RandomVariable.h"

namespace RandomVariable {

    class Normal;

    // Normal Random Variable
    class _Normal_ : public _RandomVariable_ {
    public:

		_Normal_();

		_Normal_
		(
			double mean,
			double variance
			);

		~_Normal_() override;

		[[nodiscard]] Normal clone() const;

    private:

		const RandomVariable& left() const;
		const RandomVariable& right() const;

    };

    class Normal : public SmartPtr<_Normal_> {
    public:
		Normal() : SmartPtr<_Normal_>(0) {}
		Normal( double mean, double variance ) :
			SmartPtr<_Normal_>( new _Normal_(mean,variance) ) {}
    };
}

#endif // NORMAL__H
