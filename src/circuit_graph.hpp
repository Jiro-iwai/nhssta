// -*- c++ -*-
// Authors: IWAI Jiro
// CircuitGraph: Circuit graph construction and management

#ifndef NH_CIRCUIT_GRAPH_H
#define NH_CIRCUIT_GRAPH_H

#include <functional>
#include <list>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <nhssta/gate.hpp>
#include <nhssta/net_line.hpp>
#include "../src/normal.hpp"

namespace Nh {

class CircuitGraph {
public:
    using Gates = std::unordered_map<std::string, Gate>;
    using Net = std::list<NetLine>;
    using Pins = std::set<std::string>;
    using TrackPathCallback = std::function<void(const std::string& signal_name, 
                                                  const Instance& inst, 
                                                  const NetLineIns& ins, 
                                                  const std::string& gate_type)>;
    
    void build(const Gates& gates, const Net& net, 
               const Pins& inputs, const Pins& outputs,
               const Pins& dff_outputs, const Pins& dff_inputs,
               TrackPathCallback track_path_callback = nullptr);
    
    [[nodiscard]] const Signals& signals() const { return signals_; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& signal_to_instance() const {
        return signal_to_instance_;
    }
    [[nodiscard]] const std::unordered_map<std::string, std::vector<std::string>>& instance_to_inputs() const {
        return instance_to_inputs_;
    }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& instance_to_gate_type() const {
        return instance_to_gate_type_;
    }
    [[nodiscard]] const std::unordered_map<std::string, std::unordered_map<std::string, Normal>>& instance_to_delays() const {
        return instance_to_delays_;
    }
    
    void set_bench_file(const std::string& bench_file) {
        bench_file_ = bench_file;
    }
    
private:
    void connect_instances(const Gates& gates, Net& net, TrackPathCallback track_path_callback);
    void set_instance_input(const Instance& inst, const NetLineIns& ins);
    void set_dff_out(const std::string& out_signal_name, const Gates& gates, TrackPathCallback track_path_callback);
    void track_path(const std::string& signal_name, const Instance& inst, const NetLineIns& ins, const std::string& gate_type);
    [[nodiscard]] bool is_line_ready(const NetLine& line) const;
    void check_signal(const std::string& signal_name) const;
    void node_error(const std::string& head, const std::string& signal_name) const;
    void initialize_input_signals(const Pins& inputs);
    
    Signals signals_;
    std::string bench_file_;
    std::unordered_map<std::string, std::string> signal_to_instance_;
    std::unordered_map<std::string, std::vector<std::string>> instance_to_inputs_;
    std::unordered_map<std::string, std::string> instance_to_gate_type_;
    std::unordered_map<std::string, std::unordered_map<std::string, Normal>> instance_to_delays_;
    
    static const std::string DFF_GATE_NAME;
    static constexpr double DFF_CLOCK_ARRIVAL_TIME = 0.0;
};

}  // namespace Nh

#endif  // NH_CIRCUIT_GRAPH_H

