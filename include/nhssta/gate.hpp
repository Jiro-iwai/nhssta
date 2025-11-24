// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_GATE__H
#define NH_GATE__H

#include <map>
#include <memory>
#include <nhssta/exception.hpp>
#include <string>
#include <unordered_map>

// Note: statistics.hpp is an internal implementation detail
// It aggregates headers for RandomVariable types (Normal, RandomVariable, etc.)
// Gate class requires complete type definitions for Normal and RandomVariable
// as they are used as template arguments in std::unordered_map
#include "../src/statistics.hpp"

namespace Nh {

typedef ::RandomVariable::Normal Normal;
typedef ::RandomVariable::RandomVariable RandomVariable;

class _Instance_;
class Instance;
// Use unordered_map for better performance (O(1) average vs O(log n) for map)
typedef std::unordered_map<std::string, RandomVariable> Signals;

/////

class _Gate_ {
   public:
    friend class _Instance_;

    _Gate_()
        : num_instances_(0) {}
    explicit _Gate_(const std::string& type_name)
        : num_instances_(0)
        , type_name_(type_name) {}
    virtual ~_Gate_() = default;

    void set_type_name(const std::string& type_name) {
        type_name_ = type_name;
    }
    [[nodiscard]] const std::string& type_name() const {
        return type_name_;
    }

    void set_delay(const std::string& in, const std::string& out, const Normal& delay);

    [[nodiscard]] const Normal& delay(const std::string& in, const std::string& out = "y") const;

    typedef std::pair<std::string, std::string> IO;
    // Custom hash function for IO (std::pair<std::string, std::string>)
    struct IOHash {
        std::size_t operator()(const IO& io) const {
            auto h1 = std::hash<std::string>{}(io.first);
            auto h2 = std::hash<std::string>{}(io.second);
            return h1 ^ (h2 << 1);
        }
    };
    // Use unordered_map for better performance (O(1) average vs O(log n) for map)
    // Note: MAX operation is commutative, so iteration order doesn't affect the final result
    // Small floating-point rounding differences due to different calculation order are acceptable
    typedef std::unordered_map<IO, Normal, IOHash> Delays;
    const Delays& delays() {
        return delays_;
    }

    std::string allocate_instance_name() {
        std::string inst_name = type_name_;
        inst_name += ":";
        inst_name += std::to_string(num_instances_++);
        return inst_name;
    }

   private:
    int num_instances_;
    std::string type_name_;
    Delays delays_;
};

// Handle pattern for Gate: thin wrapper around std::shared_ptr
//
// Ownership semantics:
// - Copying a Gate is lightweight: it shares ownership via std::shared_ptr
// - Passing by value (Gate) transfers/shares ownership
// - Passing by const reference (const Gate&) is a non-owning reference
// - Storing as member variable creates ownership relationship
class Gate {
   public:
    // Backward compatibility: keep exception as alias to Nh::RuntimeException
    // This will be removed in a later phase
    using exception = Nh::RuntimeException;
    Gate();
    explicit Gate(std::shared_ptr<_Gate_> body);

    // Non-owning access: returns raw pointer (no ownership transfer)
    _Gate_* operator->() const {
        return body_.get();
    }
    _Gate_& operator*() const {
        return *body_;
    }

    // Ownership access: returns shared_ptr (creates new shared_ptr copy, shares ownership)
    [[nodiscard]] std::shared_ptr<_Gate_> get() const {
        return body_;
    }

    [[nodiscard]] Instance create_instance() const;

   private:
    // Owned shared_ptr: this Gate owns the underlying object
    // Copying the Gate shares this ownership (lightweight copy)
    std::shared_ptr<_Gate_> body_;
};

/////

class _Instance_ {
   public:
    _Instance_(const Gate& gate)
        : gate_(gate) {}
    virtual ~_Instance_() = default;

    void set_name(const std::string& name) {
        name_ = name;
    }
    [[nodiscard]] const std::string& name() const {
        return name_;
    }

    void set_input(const std::string& in_name, const RandomVariable& signal);
    RandomVariable output(const std::string& out_name = "y");

   private:
    Gate gate_;
    std::string name_;
    Signals inputs_;
    Signals outputs_;
};

// Handle pattern for Instance: thin wrapper around std::shared_ptr
//
// Ownership semantics:
// - Copying an Instance is lightweight: it shares ownership via std::shared_ptr
// - Passing by value (Instance) transfers/shares ownership
// - Passing by const reference (const Instance&) is a non-owning reference
class Instance {
   public:
    Instance() = default;
    explicit Instance(std::shared_ptr<_Instance_> body)
        : body_(std::move(body)) {}

    // Non-owning access: returns raw pointer (no ownership transfer)
    _Instance_* operator->() const {
        return body_.get();
    }
    _Instance_& operator*() const {
        return *body_;
    }

    // Ownership access: returns shared_ptr (creates new shared_ptr copy, shares ownership)
    [[nodiscard]] std::shared_ptr<_Instance_> get() const {
        return body_;
    }

    bool operator==(const Instance& rhs) const {
        return body_.get() == rhs.body_.get();
    }
    bool operator!=(const Instance& rhs) const {
        return !(*this == rhs);
    }

   private:
    // Owned shared_ptr: this Instance owns the underlying object
    // Copying the Instance shares this ownership (lightweight copy)
    std::shared_ptr<_Instance_> body_;
};
}  // namespace Nh

#endif  // NH_GATE__H
