// -*- c++ -*-
// Authors: IWAI Jiro

#ifndef NH_SSTA__H
#define NH_SSTA__H

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
// smart_ptr.hpp no longer needed - NetLineBody uses std::shared_ptr directly
#include <nhssta/exception.hpp>
#include <nhssta/ssta_results.hpp>

#include <nhssta/gate.hpp>
#include <nhssta/net_line.hpp>

// Forward declaration for Parser (internal implementation detail)
// Parser is defined in global namespace in src/parser.hpp
class Parser;

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

   private:

    void read_dlib_line(Parser& parser);
    void read_bench_input(Parser& parser);
    void read_bench_output(Parser& parser);
    void read_bench_net(Parser& parser, const std::string& out_signal_name);
    void set_dff_out(const std::string& out_signal_name);
    void connect_instances();
    [[nodiscard]] bool is_line_ready(const NetLine& line) const;
    void set_instance_input(const Instance& inst, const NetLineIns& ins);
    void check_signal(const std::string& signal_name) const;

    void node_error(const std::string& head, const std::string& signal_name) const;

    ////

    // Use unordered_map for better performance (O(1) average vs O(log n) for map)
    typedef std::unordered_map<std::string, Gate> Gates;
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
    void set_lat() {
        is_lat_ = true;
    }
    void set_correlation() {
        is_correlation_ = true;
    }

    [[nodiscard]] bool is_lat() const {
        return is_lat_;
    }
    [[nodiscard]] bool is_correlation() const {
        return is_correlation_;
    }

    void set_dlib(const std::string& dlib) {
        dlib_ = dlib;
    }
    void set_bench(const std::string& bench) {
        bench_ = bench;
    }

    // Pure logic functions (Phase 2: I/O separation)
    [[nodiscard]] LatResults getLatResults() const;
    [[nodiscard]] CorrelationMatrix getCorrelationMatrix() const;
};
}  // namespace Nh

#endif  // NH_SSTA__H
