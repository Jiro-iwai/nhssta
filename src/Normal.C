// -*- c++ -*-
// Author: IWAI Jiro

#include <cassert>
#include "Normal.h"

namespace RandomVariable {

    _Normal_::_Normal_():
		_RandomVariable_(){
    }

    _Normal_::_Normal_
    (
		double mean,
		double variance
		) : _RandomVariable_(mean,variance)
    {
		if( variance < 0.0 )
			throw Exception("Normal: negative variance");
    }

    _Normal_::~_Normal_() = default;

    const RandomVariable& _Normal_::left() const {
		return left_;
    }

    const RandomVariable& _Normal_::right() const {
		return right_;
    }

    Normal _Normal_::clone() const {
		Normal p( mean_, variance_ );
		return p;
    }
}
