// -*- c++ -*-
// Authors: IWAI Jiro
//
// NetLine and NetLineImpl classes for representing netlist lines
// Extracted from Ssta class to improve separation of concerns

#ifndef NET_LINE__H
#define NET_LINE__H

#include <memory>
#include <nhssta/exception.hpp>
#include <string>
#include <vector>

namespace Nh {

// Type alias for input signal names (modern C++ using syntax)
using NetLineIns = std::vector<std::string>;

// Implementation class for NetLine (Impl suffix naming convention)
class NetLineImpl {
   public:
    NetLineImpl() = default;
    virtual ~NetLineImpl() = default;

    void set_out(const std::string& out) {
        out_ = out;
    }
    [[nodiscard]] const std::string& out() const {
        return out_;
    }

    void set_gate(const std::string& gate) {
        gate_ = gate;
    }
    [[nodiscard]] const std::string& gate() const {
        return gate_;
    }

    [[nodiscard]] const NetLineIns& ins() const {
        return ins_;
    }
    NetLineIns& ins() {
        return ins_;
    }

   private:
    std::string out_;
    std::string gate_;
    NetLineIns ins_;
};

// Handle pattern for NetLine: thin wrapper around std::shared_ptr
//
// Ownership semantics:
// - Copying a NetLine is lightweight: it shares ownership via std::shared_ptr
// - Passing by value (NetLine) transfers/shares ownership
// - Passing by const reference (const NetLine&) is a non-owning reference
class NetLineHandle {
   public:
    NetLineHandle()
        : body_(std::make_shared<NetLineImpl>()) {}
    explicit NetLineHandle(std::shared_ptr<NetLineImpl> body)
        : body_(std::move(body)) {
        if (!body_) {
            throw RuntimeException("NetLine: null body");
        }
    }

    NetLineHandle(const NetLineHandle&) = default;
    NetLineHandle(NetLineHandle&&) noexcept = default;
    NetLineHandle& operator=(const NetLineHandle&) = default;
    NetLineHandle& operator=(NetLineHandle&&) noexcept = default;
    ~NetLineHandle() = default;

    NetLineImpl* operator->() const {
        return body_.get();
    }
    NetLineImpl& operator*() const {
        return *body_;
    }

    bool operator==(const NetLineHandle& rhs) const {
        return body_.get() == rhs.body_.get();
    }
    bool operator!=(const NetLineHandle& rhs) const {
        return !(*this == rhs);
    }
    bool operator<(const NetLineHandle& rhs) const {
        return body_.get() < rhs.body_.get();
    }
    bool operator>(const NetLineHandle& rhs) const {
        return body_.get() > rhs.body_.get();
    }

    [[nodiscard]] std::shared_ptr<NetLineImpl> get() const {
        return body_;
    }

   private:
    std::shared_ptr<NetLineImpl> body_;
};

// Type alias: NetLine is a Handle (thin wrapper around std::shared_ptr)
using NetLine = NetLineHandle;

}  // namespace Nh

#endif  // NET_LINE__H

