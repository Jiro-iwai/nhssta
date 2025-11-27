// -*- c++ -*-
// Author: IWAI Jiro

#ifndef RANDOM_VARIABLE__H
#define RANDOM_VARIABLE__H

#include <memory>
#include <nhssta/exception.hpp>
#include <string>
#include <type_traits>

#include "handle.hpp"

namespace RandomVariable {

// Minimum variance threshold to avoid division by zero in statistical calculations.
// Values smaller than this are clamped to this minimum.
constexpr double MINIMUM_VARIANCE = 1.0e-6;

// Threshold for coefficient of variation calculation.
// Mean values with absolute value smaller than this are treated as zero,
// resulting in infinite coefficient of variation.
constexpr double CV_ZERO_THRESHOLD = 1.0e-10;


class RandomVariableImpl;

// Handle pattern for RandomVariable: thin wrapper around std::shared_ptr
//
// Ownership semantics:
// - Copying a Handle is lightweight: it shares ownership via std::shared_ptr
// - Passing by value (RandomVariable) transfers/shares ownership
// - Passing by const reference (const RandomVariable&) is a non-owning reference
// - Storing as member variable creates ownership relationship
//
// Value category:
// - Copy is cheap (only copies shared_ptr, not the underlying object)
// - Move is cheap (transfers shared_ptr ownership)
// - Use const reference for read-only access when ownership is not needed
// RandomVariableHandle: Handle using HandleBase template
//
// Ownership semantics:
// - Copying a Handle is lightweight: it shares ownership via std::shared_ptr
// - Passing by value (RandomVariable) transfers/shares ownership
// - Passing by const reference (const RandomVariable&) is a non-owning reference
// - Storing as member variable creates ownership relationship
//
// Value category:
// - Copy is cheap (only copies shared_ptr, not the underlying object)
// - Move is cheap (transfers shared_ptr ownership)
// - Use const reference for read-only access when ownership is not needed
class RandomVariableHandle : public HandleBase<RandomVariableImpl> {
   public:
    using element_type = RandomVariableImpl;

    // Inherit all constructors from HandleBase
    using HandleBase<RandomVariableImpl>::HandleBase;
};

// Type alias: RandomVariable is a Handle (thin wrapper around std::shared_ptr)
// Usage:
// - Pass by value when ownership should be shared: void func(RandomVariable rv)
// - Pass by const reference for read-only access: void func(const RandomVariable& rv)
// - Store as member variable to create ownership: RandomVariable left_;
using RandomVariable = RandomVariableHandle;

class RandomVariableImpl {
   public:
    RandomVariableImpl();
    virtual ~RandomVariableImpl();

    [[nodiscard]] const std::string& name() const;
    void set_name(const std::string& name);

    [[nodiscard]] const RandomVariable& left() const;
    [[nodiscard]] const RandomVariable& right() const;

    [[nodiscard]] double mean() const;
    [[nodiscard]] double variance() const;

    // Numerical error metrics
    [[nodiscard]] double standard_deviation() const;
    [[nodiscard]] double coefficient_of_variation() const;
    [[nodiscard]] double relative_error() const;  // Alias for coefficient_of_variation

    [[nodiscard]] int level() const {
        return level_;
    }

   protected:
    RandomVariableImpl(double mean, double variance, const std::string& name = "");

    RandomVariableImpl(const RandomVariable& left, const RandomVariable& right,
                     const std::string& name = "");

   protected:
    [[nodiscard]] virtual double calc_mean() const;
    [[nodiscard]] virtual double calc_variance() const;

    static void check_variance(double& v);

    std::string name_;
    // Member variables own their children via shared_ptr
    // left_ and right_ share ownership with any external Handles that reference them
    RandomVariable left_;
    RandomVariable right_;
    
    // Cache variables are mutable to allow lazy evaluation in const methods
    // These represent cached computed values, not observable state
    mutable double mean_;
    mutable double variance_;
    mutable bool is_set_mean_;
    mutable bool is_set_variance_;
    
    int level_;
};

}  // namespace RandomVariable

#endif  // RANDOM_VARIABLE__H
