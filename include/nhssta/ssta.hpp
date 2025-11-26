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

// Default number of critical paths to report when -p/--path option
// is specified without an explicit count.
constexpr size_t DEFAULT_CRITICAL_PATH_COUNT = 5;

class Ssta {
   public:

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
    void track_path(const std::string& signal_name, const Instance& inst, const NetLineIns& ins, const std::string& gate_type);

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
    bool is_critical_path_ = false;
    size_t critical_path_count_ = DEFAULT_CRITICAL_PATH_COUNT;
    Gates gates_;
    Signals signals_;
    Net net_;
    Pins inputs_;
    Pins outputs_;
    Pins dff_outputs_;
    Pins dff_inputs_;  // DFF入力（D端子）信号名
    // Path tracking data structures
    std::unordered_map<std::string, std::string> signal_to_instance_;  // signal name -> instance name
    std::unordered_map<std::string, std::vector<std::string>> instance_to_inputs_;  // instance name -> input signal names
    std::unordered_map<std::string, std::string> instance_to_gate_type_;  // instance name -> gate type name
    std::unordered_map<std::string, std::unordered_map<std::string, Normal>> instance_to_delays_;  // instance name -> (pin_name -> delay)

   public:
    void set_lat() {
        is_lat_ = true;
    }
    void set_correlation() {
        is_correlation_ = true;
    }
    void set_critical_path(size_t top_n = DEFAULT_CRITICAL_PATH_COUNT) {
        is_critical_path_ = true;
        critical_path_count_ = top_n;
    }

    [[nodiscard]] bool is_lat() const {
        return is_lat_;
    }
    [[nodiscard]] bool is_correlation() const {
        return is_correlation_;
    }
    [[nodiscard]] bool is_critical_path() const {
        return is_critical_path_;
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
    [[nodiscard]] CriticalPaths getCriticalPaths(size_t top_n) const;
    [[nodiscard]] CriticalPaths getCriticalPaths() const {
        return getCriticalPaths(critical_path_count_);
    }
};
}  // namespace Nh

#endif  // NH_SSTA__H
