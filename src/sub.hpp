// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_SUB__H
#define NH_SUB__H

#include "random_variable.hpp"

namespace RandomVariable {

class OpSUB : public RandomVariableImpl {
   public:
    OpSUB(const RandomVariable& left, const RandomVariable& right);
    ~OpSUB() override;

    // Sensitivity analysis support
    [[nodiscard]] Expression calc_mean_expr() const override;
    [[nodiscard]] Expression calc_var_expr() const override;

   private:
    [[nodiscard]] double calc_mean() const override;
    [[nodiscard]] double calc_variance() const override;
};

RandomVariable operator-(const RandomVariable& a, const RandomVariable& b);
}  // namespace RandomVariable
#endif  // NH_SUB__H
