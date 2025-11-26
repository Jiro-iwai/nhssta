// -*- c++ -*-
// Author: IWAI Jiro

#include "normal.hpp"

#include <cassert>
#include <cmath>
#include <nhssta/exception.hpp>

namespace RandomVariable {

_Normal_::_Normal_() = default;

_Normal_::_Normal_(double mean, double variance)
    : _RandomVariable_(mean, variance) {
    // Input validation: catch invalid numeric values at the boundary
    // to prevent silent propagation through calculations (Issue #136)
    if (std::isnan(mean)) {
        throw Nh::RuntimeException("Normal: mean is NaN");
    }
    if (std::isnan(variance)) {
        throw Nh::RuntimeException("Normal: variance is NaN");
    }
    if (std::isinf(mean)) {
        throw Nh::RuntimeException("Normal: mean is infinite");
    }
    if (std::isinf(variance)) {
        throw Nh::RuntimeException("Normal: variance is infinite");
    }
    if (variance < 0.0) {
        throw Nh::RuntimeException("Normal: negative variance");
    }
}

_Normal_::~_Normal_() = default;

const RandomVariable& _Normal_::left() const {
    return left_;
}

const RandomVariable& _Normal_::right() const {
    return right_;
}

Normal _Normal_::clone() const {
    Normal p(mean_, variance_);
    return p;
}

Normal Normal::clone() const {
    auto ptr = this->dynamic_pointer_cast<_Normal_>();
    return ptr->clone();
}
}  // namespace RandomVariable
