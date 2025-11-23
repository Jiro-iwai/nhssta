// -*- c++ -*-
// Authors: IWAI Jiro

#ifndef NH_SSTA__H
#define NH_SSTA__H

#include <set>
#include <map>
#include <list>
#include <vector>
#include <string>
#include "SmartPtr.h"
#include "Gate.h"
#include "Parser.h"
#include "SstaResults.h"
#include "Exception.h"

namespace Nh {

    class Ssta {
    public:

		// Backward compatibility: keep exception as alias to Nh::Exception
		// This will be removed in a later phase
		using exception = Nh::Exception;

		Ssta();
		~Ssta();
		void check();
		void read_dlib();
		void read_bench();
		void report() ;

    private:

		typedef std::vector<std::string> Ins;

		class NetLineBody : public RCObject {
		public:
			NetLineBody(){}
			~NetLineBody(){}

			void set_out(const std::string& out) { out_ = out; }
			const std::string& out() const { return out_; }

			void set_gate(const std::string& gate) { gate_ = gate; }
			const std::string& gate() const { return gate_; }

			const Ins& ins() const{ return ins_; }
			Ins& ins() { return ins_; }

		private:
			std::string out_;
			std::string gate_;
			Ins ins_;
		};

		class NetLine : public SmartPtr<NetLineBody> {
		public:
			NetLine() : SmartPtr<NetLineBody>( new NetLineBody() ) {}
		};

		void read_dlib_line(Parser& parser);
		void read_bench_input(Parser& parser);
		void read_bench_output(Parser& parser);
		void read_bench_net(Parser& parser,const std::string& out_signal_name);
		void set_dff_out(const std::string& out_signal_name);
		void connect_instances();
		bool is_line_ready(const NetLine& line) const;
		void set_instance_input(const Instance& inst, const Ins& ins);
		void check_signal(const std::string& signal_name) const;

		void node_error
		(
			const std::string& head,
			const std::string& signal_name
			) const;

		void report_lat() const;
		void report_correlation() const;
		void print_line() const;

		////

		typedef std::map<std::string,Gate> Gates;
		typedef std::list<NetLine> Net;
		typedef std::set<std::string> Pins;

		std::string dlib_;
		std::string bench_;
		bool is_lat_;
		bool is_correlation_;
		Gates gates_;
		Signals signals_;
		Net net_;
		Pins inputs_;
		Pins outputs_;

    public:

		void set_lat() { is_lat_ = true; }
		void set_correlation() { is_correlation_ = true; }

		void set_dlib(std::string dlib) { dlib_ = dlib; }
		void set_bench(std::string bench) { bench_ = bench; }

		// Pure logic functions (Phase 2: I/O separation)
		LatResults getLatResults() const;
		CorrelationMatrix getCorrelationMatrix() const;

    };
}

#endif	// NH_SSTA__H
