// -*- c++ -*-
// Author: IWAI Jiro

#ifndef MAX__H
#define MAX__H

#include "random_variable.hpp"

namespace RandomVariable {

//////

class OpMAX0;

class OpMAX : public RandomVariableImpl {
   public:
    OpMAX(const RandomVariable& left, const RandomVariable& right);
    ~OpMAX() override;

    [[nodiscard]] const RandomVariable& max0() const {
        return max0_;
    }

   private:
    [[nodiscard]] double calc_mean() const override;
    [[nodiscard]] double calc_variance() const override;

    RandomVariable max0_;
};

RandomVariable MAX(const RandomVariable& a, const RandomVariable& b);

//////

class OpMAX0 : public RandomVariableImpl {
   public:
    OpMAX0(const RandomVariable& left);
    ~OpMAX0() override;

   private:
    [[nodiscard]] double calc_mean() const override;
    [[nodiscard]] double calc_variance() const override;
    [[nodiscard]] const RandomVariable& right() const;
};

RandomVariable MAX0(const RandomVariable& a);
}  // namespace RandomVariable

#endif  // MAX__H
