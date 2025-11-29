// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NORMAL__H
#define NORMAL__H

#include <memory>

#include "random_variable.hpp"

namespace RandomVariable {

class Normal;

// Normal Random Variable
class NormalImpl : public RandomVariableImpl {
   public:
    NormalImpl();

    NormalImpl(double mean, double variance);

    ~NormalImpl() override;

    [[nodiscard]] Normal clone() const;

    // Sensitivity analysis: Expression-based accessors
    [[nodiscard]] Expression mean_expr() const override;
    [[nodiscard]] Expression var_expr() const override;
    [[nodiscard]] Expression std_expr() const override;
    [[nodiscard]] Expression mu_expr() const override;
    [[nodiscard]] Expression sigma_expr() const override;

   private:
    [[nodiscard]] const RandomVariable& left() const;
    [[nodiscard]] const RandomVariable& right() const;
    
    // Expression versions of parameters (for sensitivity analysis)
    // These are lazily initialized when first accessed
    mutable Expression mu_expr_;     // Expression for μ
    mutable Expression sigma_expr_;  // Expression for σ
    
    void init_expr() const;  // Lazy initialization of expressions
};

class Normal : public RandomVariable {
   public:
    Normal() = default;
    Normal(double mean, double variance)
        : RandomVariable(std::make_shared<NormalImpl>(mean, variance)) {}

    [[nodiscard]] Normal clone() const;
};
}  // namespace RandomVariable

#endif  // NORMAL__H
