// -*- c++ -*-
// Author: IWAI Jiro

#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <memory>
#include "gate.hpp"
#include "add.hpp"
#include <nhssta/exception.hpp>

namespace Nh {

    Gate::Gate()
        : body_(std::make_shared<_Gate_>()) {}

    Gate::Gate(std::shared_ptr<_Gate_> body)
        : body_(std::move(body)) {
        if (!body_) {
            throw Nh::RuntimeException("Gate: null body");
        }
    }

    Instance Gate::create_instance() const {
        auto inst_body = std::make_shared<_Instance_>(*this);
        Instance inst(inst_body);
        inst->set_name((*this)->allocate_instance_name());
        return inst;
    }

    ////

    void _Gate_::set_delay
    (
		const std::string& in,
		const std::string& out,
		const Normal& delay
		){
		IO io(in, out);
		assert(&(*delay));
		delays_[io] = delay;
    }

    const Normal& _Gate_::delay
    (
		const std::string& in,
		const std::string& out
		) const {

		IO io(in, out);

		auto i = delays_.find(io);
		if( i == delays_.end() ) {
			std::string what = "delay from pin \"";
			what += in;
			what += "\" to pin \"";
			what += out;
			what += "\" is not set on gate \"";
			what += type_name();
			what += "\"";
			throw Nh::RuntimeException(what);
		}

		assert(&(*(i->second)));
		return i->second;
    }

    /////

    void _Instance_::set_input
    (
		const std::string& in_name,
		const RandomVariable& signal
		) {
		(void)gate_->delay(in_name); // error check
		inputs_[in_name] = signal;
		assert(&(*inputs_[in_name]));
    }

    RandomVariable _Instance_::output(const std::string& out_name) {

		auto i = outputs_.find(out_name);

		if( i != outputs_.end() ) {
			assert(&(*(i->second)));
			return (i->second);
		}
		{

			if( gate_->delays().empty() ) {
				std::string what = "no delay is set on gate \"";
				what += gate_->type_name();
				what += "\"";
				throw Nh::RuntimeException(what);
			}

			auto i = gate_->delays().begin();
			for( ; i != gate_->delays().end(); i++ ) {

				const std::string& ith_out_name = i->first.second;
				if( ith_out_name != out_name ) {
					continue;
				}

				const std::string& ith_in_name = i->first.first;
				Normal gate_delay = gate_->delay(ith_in_name, out_name);
				RandomVariable delay = gate_delay.clone(); ////

				auto k = inputs_.find(ith_in_name);
				if( k != inputs_.end() ) {
					const RandomVariable& irv = k->second;

					auto j = outputs_.find(out_name);
					if( j != outputs_.end() ) {
						assert(&(*(i->second)));
						RandomVariable orv = j->second;
						RandomVariable d = irv + delay; /////
						RandomVariable m = MAX(orv, d);
						outputs_[out_name] = m;
						assert(&(*outputs_[out_name]));

					} else {
						RandomVariable d = irv + delay; /////
						outputs_[out_name] = d;
						assert(&(*outputs_[out_name]));

					}
				}
			}
		}
		return outputs_[out_name];
    }
}
