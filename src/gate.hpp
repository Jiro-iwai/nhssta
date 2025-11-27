// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_GATE__H
#define NH_GATE__H

#include <map>
#include <memory>
#include <nhssta/exception.hpp>
#include <string>
#include <unordered_map>

#include "statistics.hpp"

namespace Nh {

using Normal = ::RandomVariable::Normal;
using RandomVariable = ::RandomVariable::RandomVariable;

class InstanceImpl;
class Instance;
// Use unordered_map for better performance (O(1) average vs O(log n) for map)
using Signals = std::unordered_map<std::string, RandomVariable>;

/////

class GateImpl {
   public:
    friend class InstanceImpl;

    GateImpl()
        : num_instances_(0) {}
    explicit GateImpl(const std::string& type_name)
        : num_instances_(0)
        , type_name_(type_name) {}
    virtual ~GateImpl() = default;

    void set_type_name(const std::string& type_name) {
        type_name_ = type_name;
    }
    [[nodiscard]] const std::string& type_name() const {
        return type_name_;
    }

    void set_delay(const std::string& in, const std::string& out, const Normal& delay);

    [[nodiscard]] const Normal& delay(const std::string& in, const std::string& out = "y") const;

    using IO = std::pair<std::string, std::string>;
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
    using Delays = std::unordered_map<IO, Normal, IOHash>;
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
    Gate();
    explicit Gate(std::shared_ptr<GateImpl> body);

    // Non-owning access: returns raw pointer (no ownership transfer)
    GateImpl* operator->() const {
        return body_.get();
    }
    GateImpl& operator*() const {
        return *body_;
    }

    // Ownership access: returns shared_ptr (creates new shared_ptr copy, shares ownership)
    [[nodiscard]] std::shared_ptr<GateImpl> get() const {
        return body_;
    }

    [[nodiscard]] Instance create_instance() const;

   private:
    // Owned shared_ptr: this Gate owns the underlying object
    // Copying the Gate shares this ownership (lightweight copy)
    std::shared_ptr<GateImpl> body_;
};

/////

class InstanceImpl {
   public:
    InstanceImpl(const Gate& gate)
        : gate_(gate) {}
    virtual ~InstanceImpl() = default;

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
    explicit Instance(std::shared_ptr<InstanceImpl> body)
        : body_(std::move(body)) {}

    // Non-owning access: returns raw pointer (no ownership transfer)
    InstanceImpl* operator->() const {
        return body_.get();
    }
    InstanceImpl& operator*() const {
        return *body_;
    }

    // Ownership access: returns shared_ptr (creates new shared_ptr copy, shares ownership)
    [[nodiscard]] std::shared_ptr<InstanceImpl> get() const {
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
    std::shared_ptr<InstanceImpl> body_;
};
}  // namespace Nh

#endif  // NH_GATE__H
