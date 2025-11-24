// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NORMAL__H
#define NORMAL__H

#include <memory>

#include "random_variable.hpp"

namespace RandomVariable {

class Normal;

// Normal Random Variable
class _Normal_ : public _RandomVariable_ {
   public:
    _Normal_();

    _Normal_(double mean, double variance);

    ~_Normal_() override;

    [[nodiscard]] Normal clone() const;

   private:
    [[nodiscard]] const RandomVariable& left() const;
    [[nodiscard]] const RandomVariable& right() const;
};

class Normal : public RandomVariable {
   public:
    Normal() = default;
    Normal(double mean, double variance)
        : RandomVariable(std::make_shared<_Normal_>(mean, variance)) {}

    [[nodiscard]] Normal clone() const;
};
}  // namespace RandomVariable

#endif  // NORMAL__H
