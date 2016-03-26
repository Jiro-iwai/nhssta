// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_GATE__H
#define NH_GATE__H

#include <string>
#include <map>
#include "SmartPtr.h"
#include "Statistics.h"

namespace Nh {

    typedef ::RandomVariable::Normal Normal;
    typedef ::RandomVariable::RandomVariable RandomVariable;

    class _Instance_;
    typedef SmartPtr<_Instance_> Instance;
    typedef std::map<std::string,RandomVariable> Signals;

    /////

    class _Gate_ : public RCObject {
    public:

		friend class _Instance_;

		_Gate_() : num_instances_(0) {}
		_Gate_(const std::string type_name) :
			type_name_(type_name) {
		}
		virtual ~_Gate_(){};

		void set_type_name(const std::string type_name) {
			type_name_ = type_name;
		}
		const std::string& type_name() const { return type_name_; }

		void set_delay
		(
			const std::string& in,
			const std::string& out,
			const Normal& delay
			);

		const Normal& delay
		(
			const std::string& in,
			const std::string& out = "y"
			) const;

		Instance create_instance();

		typedef std::pair<std::string,std::string> IO;
		typedef std::map<IO,Normal> Delays;
		const Delays& delays() { return delays_; }

    private:

		int num_instances_;
		std::string type_name_;
		Delays delays_;
    };

    class Gate : public SmartPtr<_Gate_> {
    public:
		class exception {
		public:
			exception(const std::string& what): what_(what) {}
			const std::string& what() { return what_; }
		private:
			std::string what_ ;
		};
		Gate() : SmartPtr<_Gate_>( new _Gate_() ) {}
		Gate(_Gate_* body) : SmartPtr<_Gate_>(body) {}
    };


    /////

    class _Instance_ : public RCObject {
    public:

		_Instance_(const Gate& gate) : gate_(gate) {}
		~_Instance_(){}

		void set_name(const std::string name) { name_ = name; }
		const std::string& name() const { return name_;	}

		void set_input(const std::string& in_name,
					   const RandomVariable& signal);
		RandomVariable output(const std::string& out_name = "y");

    private:

		Gate gate_;
		std::string name_;
		Signals inputs_;
		Signals outputs_;
    };
}

#endif // NH_GATE__H
