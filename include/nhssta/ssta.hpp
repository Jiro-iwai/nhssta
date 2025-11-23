// -*- c++ -*-
// Authors: IWAI Jiro

#ifndef NH_SSTA__H
#define NH_SSTA__H

#include <memory>
#include <set>
#include <map>
#include <list>
#include <vector>
#include <string>
// smart_ptr.hpp no longer needed - NetLineBody uses std::shared_ptr directly
#include "../src/gate.hpp"
#include "../src/parser.hpp"
#include <nhssta/ssta_results.hpp>
#include <nhssta/exception.hpp>

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

		class NetLineBody {
		public:
			NetLineBody() = default;
			virtual ~NetLineBody() = default;

			void set_out(const std::string& out) { out_ = out; }
			[[nodiscard]] const std::string& out() const { return out_; }

			void set_gate(const std::string& gate) { gate_ = gate; }
			[[nodiscard]] const std::string& gate() const { return gate_; }

			[[nodiscard]] const Ins& ins() const{ return ins_; }
			Ins& ins() { return ins_; }

		private:
			std::string out_;
			std::string gate_;
			Ins ins_;
		};

		class NetLine {
		public:
			NetLine() : body_(std::make_shared<NetLineBody>()) {}
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

			NetLineBody* operator->() const { return body_.get(); }
			NetLineBody& operator*() const { return *body_; }

			bool operator==(const NetLine& rhs) const { return body_.get() == rhs.body_.get(); }
			bool operator!=(const NetLine& rhs) const { return !(*this == rhs); }
			bool operator<(const NetLine& rhs) const { return body_.get() < rhs.body_.get(); }
			bool operator>(const NetLine& rhs) const { return body_.get() > rhs.body_.get(); }

			[[nodiscard]] std::shared_ptr<NetLineBody> get() const { return body_; }

		private:
			std::shared_ptr<NetLineBody> body_;
		};

		void read_dlib_line(Parser& parser);
		void read_bench_input(Parser& parser);
		void read_bench_output(Parser& parser);
		void read_bench_net(Parser& parser,const std::string& out_signal_name);
		void set_dff_out(const std::string& out_signal_name);
		void connect_instances();
		[[nodiscard]] bool is_line_ready(const NetLine& line) const;
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
		bool is_lat_ = false;
		bool is_correlation_ = false;
		Gates gates_;
		Signals signals_;
		Net net_;
		Pins inputs_;
		Pins outputs_;

    public:

		void set_lat() { is_lat_ = true; }
		void set_correlation() { is_correlation_ = true; }

		void set_dlib(const std::string& dlib) { dlib_ = dlib; }
		void set_bench(const std::string& bench) { bench_ = bench; }

		// Pure logic functions (Phase 2: I/O separation)
		[[nodiscard]] LatResults getLatResults() const;
		[[nodiscard]] CorrelationMatrix getCorrelationMatrix() const;

    };
}

#endif	// NH_SSTA__H
