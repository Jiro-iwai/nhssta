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
#include "../src/handle.hpp"
#include "../src/statistics.hpp"

namespace Nh {

using Normal = ::RandomVariable::Normal;
using RandomVariable = ::RandomVariable::RandomVariable;

class InstanceImpl;
class InstanceHandle;
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

// Handle pattern for Gate using HandleBase template
//
// Ownership semantics:
// - Copying a Gate is lightweight: it shares ownership via std::shared_ptr
// - Passing by value (Gate) transfers/shares ownership
// - Passing by const reference (const Gate&) is a non-owning reference
// - Storing as member variable creates ownership relationship
class GateHandle : public HandleBase<GateImpl> {
   public:
    GateHandle();
    explicit GateHandle(std::shared_ptr<GateImpl> body);

    // Custom method: create an Instance from this Gate
    [[nodiscard]] InstanceHandle create_instance() const;
};

// Type alias: Gate is a Handle (thin wrapper around std::shared_ptr)
using Gate = GateHandle;

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

    // Access cloned delays used in Expression tree (for sensitivity analysis)
    // Key: (input_pin, output_pin), Value: cloned Normal delay
    using UsedDelays = std::unordered_map<GateImpl::IO, Normal, GateImpl::IOHash>;
    [[nodiscard]] const UsedDelays& used_delays() const {
        return used_delays_;
    }

   private:
    Gate gate_;
    std::string name_;
    Signals inputs_;
    Signals outputs_;
    UsedDelays used_delays_;  // Track cloned delays for sensitivity analysis
};

// Handle pattern for Instance using HandleBase template
//
// Ownership semantics:
// - Copying an Instance is lightweight: it shares ownership via std::shared_ptr
// - Passing by value (Instance) transfers/shares ownership
// - Passing by const reference (const Instance&) is a non-owning reference
class InstanceHandle : public HandleBase<InstanceImpl> {
   public:
    // Inherit all constructors from HandleBase
    using HandleBase<InstanceImpl>::HandleBase;
};

// Type alias: Instance is a Handle (thin wrapper around std::shared_ptr)
using Instance = InstanceHandle;
}  // namespace Nh

#endif  // NH_GATE__H
