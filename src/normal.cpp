// -*- c++ -*-
// Author: IWAI Jiro

#include "normal.hpp"

#include <cassert>
#include <cmath>
#include <nhssta/exception.hpp>

namespace RandomVariable {

NormalImpl::NormalImpl() = default;

NormalImpl::NormalImpl(double mean, double variance)
    : RandomVariableImpl(mean, variance) {
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

NormalImpl::~NormalImpl() = default;

const RandomVariable& NormalImpl::left() const {
    return left_;
}

const RandomVariable& NormalImpl::right() const {
    return right_;
}

Normal NormalImpl::clone() const {
    Normal p(mean_, variance_);
    return p;
}

Normal Normal::clone() const {
    auto ptr = this->dynamic_pointer_cast<NormalImpl>();
    return ptr->clone();
}

// Sensitivity analysis support

void NormalImpl::init_expr() const {
    if (!mu_expr_) {
        // Create Variable expressions for μ and σ
        Variable mu_var;
        mu_var = mean_;  // Set value to stored mean
        mu_expr_ = mu_var;
        
        Variable sigma_var;
        sigma_var = std::sqrt(variance_);  // Set value to σ = √variance
        sigma_expr_ = sigma_var;
    }
}

Expression NormalImpl::mean_expr() const {
    init_expr();
    // E[D] = μ
    return mu_expr_;
}

Expression NormalImpl::var_expr() const {
    init_expr();
    // Var[D] = σ²
    return sigma_expr_ * sigma_expr_;
}

Expression NormalImpl::std_expr() const {
    init_expr();
    // Std[D] = σ
    return sigma_expr_;
}

}  // namespace RandomVariable
