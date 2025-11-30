// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_ADD__H
#define NH_ADD__H

#include "random_variable.hpp"

namespace RandomVariable {

class OpADD : public RandomVariableImpl {
   public:
    OpADD(const RandomVariable& left, const RandomVariable& right);
    ~OpADD() override;

    // Sensitivity analysis support
    [[nodiscard]] Expression calc_mean_expr() const override;
    [[nodiscard]] Expression calc_var_expr() const override;

   private:
    [[nodiscard]] double calc_mean() const override;
    [[nodiscard]] double calc_variance() const override;
};

RandomVariable operator+(const RandomVariable& a, const RandomVariable& b);

}  // namespace RandomVariable

#endif  // NH_ADD__H
