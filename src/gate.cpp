// -*- c++ -*-
// Author: IWAI Jiro

#include <nhssta/gate.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <nhssta/exception.hpp>
#include <string>

#include "add.hpp"
#include "normal.hpp"
#include "random_variable.hpp"

namespace Nh {

GateHandle::GateHandle()
    : HandleBase(std::make_shared<GateImpl>()) {}

GateHandle::GateHandle(std::shared_ptr<GateImpl> body)
    : HandleBase(std::move(body)) {
    if (!body_) {
        throw Nh::RuntimeException("Gate: null body");
    }
}

InstanceHandle GateHandle::create_instance() const {
    auto inst_body = std::make_shared<InstanceImpl>(*this);
    InstanceHandle inst(inst_body);
    inst->set_name((*this)->allocate_instance_name());
    return inst;
}

////

void GateImpl::set_delay(const std::string& in, const std::string& out, const Normal& delay) {
    IO io(in, out);
    delays_[io] = delay;
}

const Normal& GateImpl::delay(const std::string& in, const std::string& out) const {
    IO io(in, out);

    auto i = delays_.find(io);
    if (i == delays_.end()) {
        std::string what = "delay from pin \"";
        what += in;
        what += "\" to pin \"";
        what += out;
        what += "\" is not set on gate \"";
        what += type_name();
        what += "\"";
        throw Nh::RuntimeException(what);
    }

    return i->second;
}

/////

void InstanceImpl::set_input(const std::string& in_name, const RandomVariable& signal) {
    (void)gate_->delay(in_name);  // error check
    inputs_[in_name] = signal;
}

RandomVariable InstanceImpl::output(const std::string& out_name) {
    auto i = outputs_.find(out_name);

    if (i != outputs_.end()) {
        return (i->second);
    }
    {
        if (gate_->delays().empty()) {
            std::string what = "no delay is set on gate \"";
            what += gate_->type_name();
            what += "\"";
            throw Nh::RuntimeException(what);
        }

        auto i = gate_->delays().begin();
        for (; i != gate_->delays().end(); i++) {
            const std::string& ith_out_name = i->first.second;
            if (ith_out_name != out_name) {
                continue;
            }

            const std::string& ith_in_name = i->first.first;
            Normal gate_delay = gate_->delay(ith_in_name, out_name);
            RandomVariable delay = gate_delay.clone();  ////

            auto k = inputs_.find(ith_in_name);
            if (k != inputs_.end()) {
                const RandomVariable& irv = k->second;

                auto j = outputs_.find(out_name);
                if (j != outputs_.end()) {
                    RandomVariable orv = j->second;
                    RandomVariable d = irv + delay;  /////
                    RandomVariable m = MAX(orv, d);
                    outputs_[out_name] = m;

                } else {
                    RandomVariable d = irv + delay;  /////
                    outputs_[out_name] = d;
                }
            }
        }
    }
    return outputs_[out_name];
}
}  // namespace Nh
