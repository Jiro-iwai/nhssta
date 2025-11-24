// -*- c++ -*-
// Author: IWAI Jiro

#ifndef RANDOM_VARIABLE__H
#define RANDOM_VARIABLE__H

#include <memory>
#include <nhssta/exception.hpp>
#include <string>
#include <type_traits>

namespace RandomVariable {

const double MINIMUM_VARIANCE = 1.0e-6;

// Backward compatibility: keep Exception as alias to Nh::RuntimeException
// This will be removed in a later phase
using Exception = Nh::RuntimeException;

class _RandomVariable_;

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
class RandomVariableHandle {
   public:
    using element_type = _RandomVariable_;

    RandomVariableHandle() = default;
    RandomVariableHandle(std::nullptr_t)
        : body_(nullptr) {}

    // Takes ownership of the raw pointer (creates new shared_ptr)
    explicit RandomVariableHandle(_RandomVariable_* body)
        : body_(body != nullptr ? std::shared_ptr<_RandomVariable_>(body) : nullptr) {}

    // Takes ownership via move (transfers shared_ptr ownership)
    explicit RandomVariableHandle(std::shared_ptr<_RandomVariable_> body)
        : body_(std::move(body)) {}

    // Takes ownership via copy (shares shared_ptr ownership)
    template <class Derived, class = std::enable_if_t<std::is_base_of_v<_RandomVariable_, Derived>>>
    explicit RandomVariableHandle(const std::shared_ptr<Derived>& body)
        : body_(std::static_pointer_cast<_RandomVariable_>(body)) {}

    // Non-owning access: returns raw pointer (no ownership transfer)
    _RandomVariable_* operator->() const {
        return body_.get();
    }
    _RandomVariable_& operator*() const {
        return *body_;
    }
    [[nodiscard]] _RandomVariable_* get() const {
        return body_.get();
    }

    // Ownership access: returns shared_ptr (creates new shared_ptr copy, shares ownership)
    [[nodiscard]] std::shared_ptr<_RandomVariable_> shared() const {
        return body_;
    }
    explicit operator bool() const {
        return static_cast<bool>(body_);
    }

    bool operator==(const RandomVariableHandle& rhs) const {
        return body_.get() == rhs.body_.get();
    }
    bool operator!=(const RandomVariableHandle& rhs) const {
        return !(*this == rhs);
    }
    bool operator<(const RandomVariableHandle& rhs) const {
        return body_.get() < rhs.body_.get();
    }
    bool operator>(const RandomVariableHandle& rhs) const {
        return body_.get() > rhs.body_.get();
    }

    template <class U>
    [[nodiscard]] std::shared_ptr<U> dynamic_pointer_cast() const {
        auto ptr = std::dynamic_pointer_cast<U>(body_);
        if (!ptr) {
            throw Nh::RuntimeException("RandomVariable: failed to dynamic cast");
        }
        return ptr;
    }

   private:
    // Owned shared_ptr: this Handle owns the underlying object
    // Copying the Handle shares this ownership (lightweight copy)
    std::shared_ptr<_RandomVariable_> body_;
};

// Type alias: RandomVariable is a Handle (thin wrapper around std::shared_ptr)
// Usage:
// - Pass by value when ownership should be shared: void func(RandomVariable rv)
// - Pass by const reference for read-only access: void func(const RandomVariable& rv)
// - Store as member variable to create ownership: RandomVariable left_;
using RandomVariable = RandomVariableHandle;

class _RandomVariable_ {
   public:
    _RandomVariable_();
    virtual ~_RandomVariable_();

    [[nodiscard]] const std::string& name() const;
    void set_name(const std::string& name);

    [[nodiscard]] const RandomVariable& left() const;
    [[nodiscard]] const RandomVariable& right() const;

    double mean();
    double variance();

    // Numerical error metrics
    [[nodiscard]] double standard_deviation();
    [[nodiscard]] double coefficient_of_variation();
    [[nodiscard]] double relative_error();  // Alias for coefficient_of_variation

    [[nodiscard]] int level() const {
        return level_;
    }

   protected:
    _RandomVariable_(double mean, double variance, const std::string& name = "");

    _RandomVariable_(const RandomVariable& left, const RandomVariable& right,
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
    double mean_;
    double variance_;

    bool is_set_mean_;
    bool is_set_variance_;
    int level_;
};

}  // namespace RandomVariable

#endif  // RANDOM_VARIABLE__H
