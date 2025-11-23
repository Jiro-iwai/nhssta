// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_GATE__H
#define NH_GATE__H

#include <string>
#include <map>
#include <memory>
#include "smart_ptr.hpp"
#include "statistics.hpp"
#include "exception.hpp"

namespace Nh {

    typedef ::RandomVariable::Normal Normal;
    typedef ::RandomVariable::RandomVariable RandomVariable;

    class _Instance_;
    class Instance;
    typedef std::map<std::string,RandomVariable> Signals;

    /////

    class _Gate_ : public RCObject {
    public:

		friend class _Instance_;

		_Gate_() : num_instances_(0) {}
		explicit _Gate_(const std::string& type_name) :
			num_instances_(0), type_name_(type_name) {
		}
		~_Gate_() override = default;

		void set_type_name(const std::string& type_name) {
			type_name_ = type_name;
		}
		[[nodiscard]] const std::string& type_name() const { return type_name_; }

		void set_delay
		(
			const std::string& in,
			const std::string& out,
			const Normal& delay
			);

		[[nodiscard]] const Normal& delay
		(
			const std::string& in,
			const std::string& out = "y"
			) const;

		typedef std::pair<std::string,std::string> IO;
		typedef std::map<IO,Normal> Delays;
		const Delays& delays() { return delays_; }

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

    class Gate {
    public:
		// Backward compatibility: keep exception as alias to Nh::RuntimeException
		// This will be removed in a later phase
		using exception = Nh::RuntimeException;
		Gate();
		explicit Gate(std::shared_ptr<_Gate_> body);

		_Gate_* operator->() const { return body_.get(); }
		_Gate_& operator*() const { return *body_; }
		[[nodiscard]] std::shared_ptr<_Gate_> get() const { return body_; }

		[[nodiscard]] Instance create_instance() const;

    private:
		std::shared_ptr<_Gate_> body_;
    };


    /////

    class _Instance_ : public RCObject {
    public:

		_Instance_(const Gate& gate) : gate_(gate) {}
		~_Instance_() override = default;

		void set_name(const std::string& name) { name_ = name; }
		[[nodiscard]] const std::string& name() const { return name_;	}

		void set_input(const std::string& in_name,
					   const RandomVariable& signal);
		RandomVariable output(const std::string& out_name = "y");

    private:

		Gate gate_;
		std::string name_;
		Signals inputs_;
		Signals outputs_;
    };

    class Instance {
    public:
		Instance() = default;
		explicit Instance(std::shared_ptr<_Instance_> body)
			: body_(std::move(body)) {}

		_Instance_* operator->() const { return body_.get(); }
		_Instance_& operator*() const { return *body_; }
		[[nodiscard]] std::shared_ptr<_Instance_> get() const { return body_; }

		bool operator==(const Instance& rhs) const { return body_.get() == rhs.body_.get(); }
		bool operator!=(const Instance& rhs) const { return !(*this == rhs); }

    private:
		std::shared_ptr<_Instance_> body_;
    };
}

#endif // NH_GATE__H
