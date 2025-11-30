// -*- c++ -*-
// Author: IWAI Jiro
//
// HandleBase: Generic base class template for Handle pattern
// Provides common functionality for std::shared_ptr wrappers

#ifndef NH_HANDLE__H
#define NH_HANDLE__H

#include <memory>
#include <nhssta/exception.hpp>
#include <type_traits>

// HandleBase: Base template for Handle pattern classes
// Note: Placed outside namespace to be usable by both Nh and RandomVariable namespaces
//
// Provides common functionality:
// - operator->, operator* for pointer-like access
// - get() for raw pointer access
// - shared() for shared_ptr access
// - operator bool for null checking
// - Comparison operators (==, !=)
// - dynamic_pointer_cast for safe downcasting
//
// Usage:
//   class MyHandle : public HandleBase<MyImpl> {
//   public:
//       using HandleBase<MyImpl>::HandleBase;  // Inherit constructors
//       // Add custom methods here
//   };
//
template <typename T>
class HandleBase {
   public:
    using element_type = T;

    // Default constructor: creates null handle
    HandleBase() = default;

    // Null pointer constructor
    HandleBase(std::nullptr_t)
        : body_(nullptr) {}

    // Takes ownership of raw pointer (creates new shared_ptr)
    explicit HandleBase(T* body)
        : body_(body != nullptr ? std::shared_ptr<T>(body) : nullptr) {}

    // Takes ownership via move (transfers shared_ptr ownership)
    explicit HandleBase(std::shared_ptr<T> body)
        : body_(std::move(body)) {}

    // Takes ownership via copy from derived type (shares shared_ptr ownership)
    template <class Derived, class = std::enable_if_t<std::is_base_of_v<T, Derived>>>
    explicit HandleBase(const std::shared_ptr<Derived>& body)
        : body_(std::static_pointer_cast<T>(body)) {}

    // Non-owning access: returns raw pointer (no ownership transfer)
    T* operator->() const {
        return body_.get();
    }

    T& operator*() const {
        return *body_;
    }

    [[nodiscard]] T* get() const {
        return body_.get();
    }

    // Ownership access: returns shared_ptr (creates new shared_ptr copy, shares ownership)
    [[nodiscard]] std::shared_ptr<T> shared() const {
        return body_;
    }

    // Null check
    explicit operator bool() const {
        return static_cast<bool>(body_);
    }

    // Comparison operators (compare by pointer identity)
    bool operator==(const HandleBase& rhs) const {
        return body_.get() == rhs.body_.get();
    }

    bool operator!=(const HandleBase& rhs) const {
        return !(*this == rhs);
    }

    // Safe downcast to derived type
    template <class U>
    [[nodiscard]] std::shared_ptr<U> dynamic_pointer_cast() const {
        auto ptr = std::dynamic_pointer_cast<U>(body_);
        if (!ptr) {
            throw Nh::RuntimeException("Handle: failed to dynamic cast");
        }
        return ptr;
    }

   protected:
    // Protected to allow derived classes direct access
    std::shared_ptr<T> body_ = nullptr;
};

#endif  // NH_HANDLE__H

