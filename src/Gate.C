// -*- c++ -*-
// Author: IWAI Jiro

#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <boost/lexical_cast.hpp>
#include "Gate.h"
#include "ADD.h"

namespace Nh {

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

		Delays::const_iterator i = delays_.find(io);
		if( i == delays_.end() ) {
			std::string what = "delay from pin \"";
			what += in;
			what += "\" to pin \"";
			what += out;
			what += "\" is not set on gate \"";
			what += type_name();
			what += "\"";
			throw Gate::exception(what);
		}

		assert(&(*(i->second)));
		return i->second;
    }

    Instance _Gate_::create_instance() {
		Instance inst( new _Instance_( Gate(this) ) );
		std::string inst_name = type_name_ + ":";
		inst_name += boost::lexical_cast<std::string>(num_instances_++);
		inst->set_name(inst_name);
		return inst;
    }

    /////

    void _Instance_::set_input
    (
		const std::string& in_name,
		const RandomVariable& signal
		) {
		gate_->delay(in_name); // error check
		inputs_[in_name] = signal;
		assert(&(*inputs_[in_name]));
    }

    RandomVariable _Instance_::output(const std::string& out_name) {

		Signals::const_iterator i = outputs_.find(out_name);

		if( i != outputs_.end() ) {
			assert(&(*(i->second)));
			return (i->second);

		} else {

			if( gate_->delays().empty() ) {
				std::string what = "no delay is set on gate \"";
				what += gate_->type_name();
				what += "\"";
				throw Gate::exception(what);
			}

			_Gate_::Delays::const_iterator i = gate_->delays().begin();
			for( ; i != gate_->delays().end(); i++ ) {

				const std::string& ith_out_name = i->first.second;
				if( ith_out_name != out_name ) continue;

				const std::string& ith_in_name = i->first.first;
				Normal gate_delay = gate_->delay(ith_in_name, out_name);
				RandomVariable delay = gate_delay->clone(); ////

				Signals::const_iterator k = inputs_.find(ith_in_name);
				if( k != inputs_.end() ) {
					const RandomVariable& irv = k->second;

					Signals::const_iterator j = outputs_.find(out_name);
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
