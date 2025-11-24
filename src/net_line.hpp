// -*- c++ -*-
// Authors: IWAI Jiro
//
// NetLine and NetLineBody classes for representing netlist lines
// Extracted from Ssta class to improve separation of concerns

#ifndef NET_LINE__H
#define NET_LINE__H

#include <memory>
#include <nhssta/exception.hpp>
#include <string>
#include <vector>

namespace Nh {

// Type alias for input signal names
typedef std::vector<std::string> NetLineIns;

class NetLineBody {
   public:
    NetLineBody() = default;
    virtual ~NetLineBody() = default;

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

class NetLine {
   public:
    NetLine()
        : body_(std::make_shared<NetLineBody>()) {}
    explicit NetLine(std::shared_ptr<NetLineBody> body)
        : body_(std::move(body)) {
        if (!body_) {
            throw RuntimeException("NetLine: null body");
        }
    }

    NetLine(const NetLine&) = default;
    NetLine(NetLine&&) noexcept = default;
    NetLine& operator=(const NetLine&) = default;
    NetLine& operator=(NetLine&&) noexcept = default;
    ~NetLine() = default;

    NetLineBody* operator->() const {
        return body_.get();
    }
    NetLineBody& operator*() const {
        return *body_;
    }

    bool operator==(const NetLine& rhs) const {
        return body_.get() == rhs.body_.get();
    }
    bool operator!=(const NetLine& rhs) const {
        return !(*this == rhs);
    }
    bool operator<(const NetLine& rhs) const {
        return body_.get() < rhs.body_.get();
    }
    bool operator>(const NetLine& rhs) const {
        return body_.get() > rhs.body_.get();
    }

    [[nodiscard]] std::shared_ptr<NetLineBody> get() const {
        return body_;
    }

   private:
    std::shared_ptr<NetLineBody> body_;
};

}  // namespace Nh

#endif  // NET_LINE__H

