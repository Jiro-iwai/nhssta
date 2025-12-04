// -*- c++ -*-
// Authors: IWAI Jiro
// CircuitGraph: Circuit graph construction and management

#include "circuit_graph.hpp"
#include "normal.hpp"
#include <nhssta/gate.hpp>
#include <nhssta/net_line.hpp>
#include <nhssta/exception.hpp>
#include <sstream>
#include <cmath>

namespace Nh {

const std::string CircuitGraph::DFF_GATE_NAME = "dff";

void CircuitGraph::build(const Gates& gates, const Net& net, 
                         const Pins& inputs, const Pins& outputs,
                         const Pins& dff_outputs, const Pins& dff_inputs,
                         const TrackPathCallback& track_path_callback) {
    // Initialize input signals
    initialize_input_signals(inputs);
    
    // Handle DFF outputs
    Net mutable_net = net;  // Copy net to make it mutable
    for (const auto& dff_output : dff_outputs) {
        set_dff_out(dff_output, gates, track_path_callback);
    }
    
    // Connect instances
    connect_instances(gates, mutable_net, track_path_callback);
}

void CircuitGraph::initialize_input_signals(const Pins& inputs) {
    for (const auto& signal_name : inputs) {
        Normal in(0.0, ::RandomVariable::MINIMUM_VARIANCE);
        in->set_name(signal_name);
        signals_[signal_name] = in;
    }
}

void CircuitGraph::connect_instances(const Gates& gates, Net& net, const TrackPathCallback& track_path_callback) {
    while (!net.empty()) {
        unsigned int size = net.size();

        auto ni = net.begin();
        while (ni != net.end()) {
            const NetLine& line = *ni;
            const NetLineIns& ins = line->ins();

            if (is_line_ready(line)) {
                const std::string& gate_name = line->gate();
                auto gi = gates.find(gate_name);
                if (gi == gates.end()) {
                    std::ostringstream what;
                    what << "Gate \"" << gate_name << "\" not found in gate library";
                    throw Nh::RuntimeException(what.str());
                }

                Gate gate = gi->second;
                Instance inst = gate.create_instance();

                set_instance_input(inst, ins);

                const RandomVariable& out = inst->output();
                const std::string& out_signal_name = line->out();

                check_signal(out_signal_name);
                signals_[out_signal_name] = out;
                out->set_name(out_signal_name);

                // Track path information
                track_path(out_signal_name, inst, ins, gate_name);
                
                // Call callback if provided
                if (track_path_callback) {
                    track_path_callback(out_signal_name, inst, ins, gate_name);
                }

                net.erase(ni++);
            } else {
                ni++;
            }
        }

        // floating circuit
        if (size == net.size()) {
            std::ostringstream what;
            what << "following node is floating\n";
            ni = net.begin();
            for (; ni != net.end(); ni++) {
                const NetLine& line = *ni;
                what << line->out() << "\n";
            }
            throw Nh::RuntimeException(what.str());
        }
    }
}

void CircuitGraph::set_instance_input(const Instance& inst, const NetLineIns& ins) {
    int ith = 0;
    auto j = ins.begin();
    for (; j != ins.end(); j++) {
        const std::string& signal_name = *j;
        const RandomVariable& signal = signals_.at(signal_name);
        std::string pin = std::to_string(ith);
        inst->set_input(pin, signal);

        ith++;
    }
}

void CircuitGraph::set_dff_out(const std::string& out_signal_name, const Gates& gates, const TrackPathCallback& track_path_callback) {
    // Clock arrival time is 0 (reference point for the clock cycle)
    Normal clock_arrival(DFF_CLOCK_ARRIVAL_TIME, ::RandomVariable::MINIMUM_VARIANCE);
    
    auto gi = gates.find(DFF_GATE_NAME);
    if (gi == gates.end()) {
        std::ostringstream what;
        what << "Gate \"" << DFF_GATE_NAME << "\" not found in gate library";
        throw Nh::RuntimeException(what.str());
    }
    Gate dff = gi->second;

    // Calculate Q output timing: clock arrival + ck->q propagation delay
    Normal delay = dff->delay("ck", "q");
    Normal dff_delay = delay.clone();
    RandomVariable out = clock_arrival + dff_delay;

    check_signal(out_signal_name);
    signals_[out_signal_name] = out;
    out->set_name(out_signal_name);
}

[[nodiscard]] bool CircuitGraph::is_line_ready(const NetLine& line) const {
    NetLineIns ins = line->ins();
    auto j = ins.begin();
    for (; j != ins.end(); j++) {
        const std::string& signal_name = *j;
        auto si = signals_.find(signal_name);
        if (si == signals_.end()) {
            return false;
        }
    }
    return true;
}

void CircuitGraph::check_signal(const std::string& signal_name) const {
    auto si = signals_.find(signal_name);
    if (si != signals_.end()) {
        node_error("node", signal_name);
    }
}

void CircuitGraph::track_path(const std::string& signal_name, const Instance& inst, const NetLineIns& ins, const std::string& gate_type) {
    const std::string& instance_name = inst->name();
    
    // Map signal to instance
    signal_to_instance_[signal_name] = instance_name;
    
    // Map instance to input signals
    instance_to_inputs_[instance_name] = ins;
    
    // Map instance to gate type (gate_type is already lowercased in read_bench_net)
    instance_to_gate_type_[instance_name] = gate_type;
    
    // Save cloned delays for this instance (for sensitivity analysis)
    // Use inst->used_delays() which contains the actual cloned delays
    // that are used in the Expression tree
    const auto& used_delays = inst->used_delays();
    std::unordered_map<std::string, Normal> delays_map;
    for (const auto& delay_pair : used_delays) {
        // Key is (input_pin, output_pin), store by input_pin for now
        delays_map[delay_pair.first.first] = delay_pair.second;
    }
    instance_to_delays_[instance_name] = delays_map;
}

void CircuitGraph::node_error(const std::string& head, const std::string& signal_name) const {
    std::ostringstream what;
    what << head << " \"" << signal_name << "\" is multiply defined";
    if (!bench_file_.empty()) {
        what << " in file \"" << bench_file_ << "\"";
    }
    throw Nh::RuntimeException(what.str());
}

}  // namespace Nh

