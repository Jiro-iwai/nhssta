// -*- c++ -*-
// Authors: IWAI Jiro

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>
#include <string>
#include <vector>

#include "add.hpp"
#include "parser.hpp"
#include "util_numerical.hpp"

namespace Nh {

// DFF gate name constant
static const std::string DFF_GATE_NAME = "dff";

// DFF clock input is treated as arriving at time 0 (start of clock cycle)
static constexpr double DFF_CLOCK_ARRIVAL_TIME = 0.0;

Ssta::Ssta() = default;

Ssta::~Ssta() = default;

void Ssta::check() {
    // Validate configuration and throw exception if invalid
    // Error messages should be handled by CLI layer

    std::string error_msg;

    if (dlib_.empty()) {
        if (!error_msg.empty()) {
            error_msg += "; ";
        }
        error_msg += "please specify `-d' properly";
    }

    if (bench_.empty()) {
        if (!error_msg.empty()) {
            error_msg += "; ";
        }
        error_msg += "please specify `-b' properly";
    }

    if (!error_msg.empty()) {
        throw Nh::ConfigurationException(error_msg);
    }
}

//// read ////

void Ssta::node_error(const std::string& head, const std::string& signal_name) const {
    std::string what(head);
    what += " \"";
    what += signal_name;
    what += "\" is multiply defined in file \"";
    what += bench_;
    what += "\"";
    throw Nh::RuntimeException(what);
}

// dlib //

void Ssta::read_dlib() {
    Parser parser(dlib_, '#', "(),", " \t\r");
    parser.checkFile();

    while (parser.getLine()) {
        read_dlib_line(parser);
    }
}

void Ssta::read_dlib_line(Parser& parser) {
    Gate g;

    std::string gate_name;
    parser.getToken(gate_name);
    auto i = gates_.find(gate_name);
    if (i != gates_.end()) {
        g = i->second;
    } else {
        g->set_type_name(gate_name);
        gates_[gate_name] = g;
    }

    std::string in;
    parser.getToken(in);

    std::string out;
    parser.getToken(out);

    std::string type;  // distribution type
    parser.getToken(type);
    if (!(type == "gauss" || type == "const")) {
        parser.unexpectedToken();
    }

    parser.checkSeparator('(');

    double mean = 0.0;
    parser.getToken(mean);
    if (mean < 0.0) {
        parser.unexpectedToken();
    }

    double variance = 0.0;
    if (type == "gauss") {  // gaussian
        parser.checkSeparator(',');

        double sigma = 0.0;
        parser.getToken(sigma);
        if (sigma < 0.0) {
            parser.unexpectedToken();
        }

        variance = sigma * sigma;

    } else {  // constant
        variance = 0.0;
    }
    Normal delay(mean, variance);
    g->set_delay(in, out, delay);

    parser.checkSeparator(')');
    parser.checkEnd();
}

// bench //

void Ssta::read_bench() {
    Parser parser(bench_, '#', "(),=", " \t\r");
    parser.checkFile();

    while (parser.getLine()) {
        std::string keyword;
        parser.getToken(keyword);

        if (keyword == "INPUT") {
            read_bench_input(parser);
        } else if (keyword == "OUTPUT") {
            read_bench_output(parser);
        } else {
            read_bench_net(parser, keyword);  // NET
        }
    }

    connect_instances();

    gates_.clear();
}

void Ssta::read_bench_input(Parser& parser) {
    parser.checkSeparator('(');

    std::string signal_name;
    parser.getToken(signal_name);
    auto si = signals_.find(signal_name);
    if (si != signals_.end()) {
        node_error("input", signal_name);
    }
    inputs_.insert(signal_name);

    Normal in(0.0, ::RandomVariable::MINIMUM_VARIANCE);  //////
    in->set_name(signal_name);
    signals_[signal_name] = in;

    parser.checkSeparator(')');
    parser.checkEnd();
}

void Ssta::read_bench_output(Parser& parser) {
    parser.checkSeparator('(');

    std::string signal_name;
    parser.getToken(signal_name);
    auto si = outputs_.find(signal_name);
    if (si != outputs_.end()) {
        node_error("output", signal_name);
    }
    outputs_.insert(signal_name);

    parser.checkSeparator(')');
    parser.checkEnd();
}

static void tolower_string(std::string& token) {
    for (char& c : token) {
        c = static_cast<char>(tolower(c));
    }
}

bool Ssta::is_line_ready(const NetLine& line) const {
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

// don't use for dff
void Ssta::set_instance_input(const Instance& inst, const NetLineIns& ins) {
    int ith = 0;
    auto j = ins.begin();
    for (; j != ins.end(); j++) {
        const std::string& signal_name = *j;
        const RandomVariable& signal = signals_[signal_name];
        std::string pin = std::to_string(ith);
        inst->set_input(pin, signal);

        ith++;
    }
}

void Ssta::check_signal(const std::string& signal_name) const {
    auto si = signals_.find(signal_name);
    if (si != signals_.end()) {
        node_error("node", signal_name);
    }
}

void Ssta::connect_instances() {
    while (!net_.empty()) {
        unsigned int size = net_.size();

        auto ni = net_.begin();
        while (ni != net_.end()) {
            const NetLine& line = *ni;
            const NetLineIns& ins = line->ins();
            // Note: dff gate check is now done in read_bench_net()

            if (is_line_ready(line)) {
                const std::string& gate_name = line->gate();
                auto gi = gates_.find(gate_name);
                if (gi == gates_.end()) {
                    std::string what = "gate \"";
                    what += gate_name;
                    what += "\" not found in gate library";
                    throw Nh::RuntimeException(what);
                }

                Gate gate = gi->second;
                Instance inst = gate.create_instance();

                set_instance_input(inst, ins);

                const RandomVariable& out = inst->output();
                const std::string& out_signal_name = line->out();

                check_signal(out_signal_name);
                signals_[out_signal_name] = out;
                out->set_name(out_signal_name);

                // Track path information if critical path analysis is enabled
                if (is_critical_path_) {
                    track_path(out_signal_name, inst, ins, gate_name);
                }

                net_.erase(ni++);
            } else {
                ni++;
            }
        }

        // floating circuit
        if (size == net_.size()) {
            std::string what = "following node is floating\n";
            ni = net_.begin();
            for (; ni != net_.end(); ni++) {
                const NetLine& line = *ni;
                what += line->out();
                what += "\n";
            }
            throw Nh::RuntimeException(what);
        }
    }
}

// Treat clock input of DFF as arriving at time 0 (start of clock cycle)
// This models the Q output timing after the clock edge
void Ssta::set_dff_out(const std::string& out_signal_name) {
    // Clock arrival time is 0 (reference point for the clock cycle)
    Normal clock_arrival(DFF_CLOCK_ARRIVAL_TIME, ::RandomVariable::MINIMUM_VARIANCE);
    
    auto gi = gates_.find(DFF_GATE_NAME);
    Gate dff = gi->second;

    // Calculate Q output timing: clock arrival + ck->q propagation delay
    Normal delay = dff->delay("ck", "q");
    Normal dff_delay = delay.clone();
    RandomVariable out = clock_arrival + dff_delay;

    check_signal(out_signal_name);
    signals_[out_signal_name] = out;
    out->set_name(out_signal_name);
    dff_outputs_.insert(out_signal_name);
}

void Ssta::read_bench_net(Parser& parser, const std::string& out_signal_name) {
    NetLine l;
    l->set_out(out_signal_name);

    parser.checkSeparator('=');

    std::string gate_name;
    parser.getToken(gate_name);
    tolower_string(gate_name);
    auto gi = gates_.find(gate_name);
    if (gi == gates_.end()) {
        std::string what = "unknown gate \"";
        what += gate_name;
        what += "\"";
        what += " at line ";
        int line = parser.getNumLine();
        what += std::to_string(line);
        what += " of file \"";
        what += parser.getFileName();
        what += "\"";
        throw Nh::ParseException(parser.getFileName(), parser.getNumLine(),
                                 "unknown gate \"" + gate_name + "\"");
    }

    l->set_gate(gate_name);

    parser.checkSeparator('(');

    NetLineIns& ins = l->ins();
    // Reserve space for input signals (typical gates have 2-4 inputs)
    // This reduces memory reallocation overhead during parsing
    ins.reserve(4);
    while (true) {
        std::string in_signal_name;
        parser.getToken(in_signal_name);
        ins.push_back(in_signal_name);

        char sep = '\0';
        parser.getToken(sep);
        if (sep == ')') {
            break;
        }
        if (sep == ',') {
            continue;
        }
        parser.unexpectedToken();
    }

    parser.checkEnd();

    if (gate_name == DFF_GATE_NAME) {
        set_dff_out(out_signal_name);
        // Record DFF data input (D terminal) for critical path analysis
        if (!ins.empty()) {
            dff_inputs_.insert(ins[0]);
        }
    } else {
        net_.push_back(l);
    }
}

// Pure logic functions (Phase 2: I/O separation)

LatResults Ssta::getLatResults() const {
    LatResults results;

    // Collect signal names and sort them for consistent output order
    // (unordered_map doesn't guarantee order, so we sort by key)
    std::vector<std::string> signal_names;
    signal_names.reserve(signals_.size());  // Pre-allocate to avoid reallocations
    for (const auto& pair : signals_) {
        signal_names.push_back(pair.first);
    }
    std::sort(signal_names.begin(), signal_names.end());

    for (const auto& signal_name : signal_names) {
        const RandomVariable& sigi = signals_.at(signal_name);
        double mean = sigi->mean();
        double variance = sigi->variance();
        double sigma = sqrt(variance);

        results.emplace_back(sigi->name(), mean, sigma);
    }

    return results;
}

CorrelationMatrix Ssta::getCorrelationMatrix() const {
    CorrelationMatrix matrix;

    // Collect signal names and sort them for consistent output order
    // (unordered_map doesn't guarantee order, so we sort by key)
    std::vector<std::string> signal_names;
    signal_names.reserve(signals_.size());  // Pre-allocate to avoid reallocations
    for (const auto& pair : signals_) {
        signal_names.push_back(pair.first);
    }
    std::sort(signal_names.begin(), signal_names.end());

    // Collect node names in sorted order
    for (const auto& signal_name : signal_names) {
        const RandomVariable& sigi = signals_.at(signal_name);
        matrix.node_names.push_back(sigi->name());
    }

    // Calculate correlations
    for (const auto& signal_name_i : signal_names) {
        const RandomVariable& sigi = signals_.at(signal_name_i);
        double vi = sigi->variance();

        for (const auto& signal_name_j : signal_names) {
            const RandomVariable& sigj = signals_.at(signal_name_j);
            double vj = sigj->variance();
            double cov = covariance(sigi, sigj);
            double corr = (vi > 0.0 && vj > 0.0) ? (cov / sqrt(vi * vj)) : 0.0;

            matrix.correlations[std::make_pair(sigi->name(), sigj->name())] = corr;
        }
    }

    return matrix;
}

void Ssta::track_path(const std::string& signal_name, const Instance& inst, const NetLineIns& ins, const std::string& gate_type) {
    const std::string& instance_name = inst->name();
    
    // Map signal to instance
    signal_to_instance_[signal_name] = instance_name;
    
    // Map instance to input signals
    instance_to_inputs_[instance_name] = ins;
    
    // Map instance to gate type (gate_type is already lowercased in read_bench_net)
    instance_to_gate_type_[instance_name] = gate_type;
    
    // Save gate delays for this instance (before gates_ is cleared)
    // gate_type is already lowercased in read_bench_net()
    auto gate_it = gates_.find(gate_type);
    if (gate_it != gates_.end()) {
        const Gate& gate = gate_it->second;
        const auto& gate_delays = gate->delays();
        std::unordered_map<std::string, Normal> delays_map;
        for (const auto& delay_pair : gate_delays) {
            delays_map[delay_pair.first.first] = delay_pair.second;  // pin_name -> delay
        }
        instance_to_delays_[instance_name] = delays_map;
    }
}

CriticalPaths Ssta::getCriticalPaths(size_t top_n) const {
    CriticalPaths paths;
    
    if (!is_critical_path_ || outputs_.empty() || top_n == 0) {
        return paths;
    }
    
    auto build_node_stats = [&](const std::vector<std::string>& ordered_nodes) {
        LatResults stats;
        stats.reserve(ordered_nodes.size());
        for (const auto& node_name : ordered_nodes) {
            double mean = 0.0;
            double sigma = 0.0;
            std::string display_name = node_name;
            auto sig_it = signals_.find(node_name);
            if (sig_it != signals_.end()) {
                const RandomVariable& sig = sig_it->second;
                mean = sig->mean();
                double variance = sig->variance();
                sigma = variance > 0.0 ? std::sqrt(variance) : 0.0;
                display_name = sig->name();
            }
            stats.emplace_back(display_name, mean, sigma);
        }
        return stats;
    };
    
    auto is_source_node = [&](const std::string& name) {
        return inputs_.find(name) != inputs_.end() || dff_outputs_.find(name) != dff_outputs_.end();
    };
    
    auto finalize_path = [&](std::vector<std::string>& node_path, std::vector<std::string>& instance_path, double output_lat) {
        std::vector<std::string> reversed_nodes(node_path.rbegin(), node_path.rend());
        std::vector<std::string> reversed_instances(instance_path.rbegin(), instance_path.rend());
        auto stats = build_node_stats(reversed_nodes);
        paths.emplace_back(reversed_nodes, reversed_instances, output_lat, stats);
        node_path.pop_back();
    };
    
    // Helper function to build path from output to input
    std::function<void(const std::string&, std::vector<std::string>&, std::vector<std::string>&, double)> build_path;
    build_path = [&](const std::string& signal_name, 
                     std::vector<std::string>& node_path,
                     std::vector<std::string>& instance_path,
                     double output_lat) {
        // Add current node to path
        node_path.push_back(signal_name);
        
        if (is_source_node(signal_name)) {
            finalize_path(node_path, instance_path, output_lat);
            return;
        }
        
        // Check if signal is generated by an instance
        auto inst_it = signal_to_instance_.find(signal_name);
        if (inst_it == signal_to_instance_.end()) {
            finalize_path(node_path, instance_path, output_lat);
            return;
        }
        
        const std::string& instance_name = inst_it->second;
        auto inputs_it = instance_to_inputs_.find(instance_name);
        if (inputs_it == instance_to_inputs_.end()) {
            node_path.pop_back();
            return;
        }
        
        const std::vector<std::string>& input_signals = inputs_it->second;
        
        // Get gate delays from saved data (gates_ may be cleared after read_bench())
        auto delays_it = instance_to_delays_.find(instance_name);
        std::unordered_map<std::string, Normal> delays_map;
        if (delays_it != instance_to_delays_.end()) {
            delays_map = delays_it->second;
        } else {
            // Delays not saved - try to get from gates_ if still available
            auto gate_type_it = instance_to_gate_type_.find(instance_name);
            if (gate_type_it != instance_to_gate_type_.end()) {
                const std::string& gate_type = gate_type_it->second;
                auto gate_it = gates_.find(gate_type);
                if (gate_it != gates_.end()) {
                    const Gate& gate = gate_it->second;
                    const auto& gate_delays = gate->delays();
                    for (const auto& delay_pair : gate_delays) {
                        delays_map[delay_pair.first.first] = delay_pair.second;
                    }
                }
            }
            // If still no delays found, continue with empty map (will use LAT only)
        }
        
        // Find the input signal that contributes to the critical path
        // This is the input whose LAT + gate_delay equals the output LAT
        std::string critical_input;
        double max_contribution = std::numeric_limits<double>::lowest();
        
        // Map input signals to pin indices (as set_instance_input does)
        int pin_index = 0;
        for (const auto& input_signal : input_signals) {
            auto input_sig_it = signals_.find(input_signal);
            if (input_sig_it == signals_.end()) {
                pin_index++;
                continue;
            }
            
            double input_lat = input_sig_it->second->mean();
            std::string pin_name = std::to_string(pin_index);
            
            // Try to find gate delay for this pin from saved delays
            auto delay_it = delays_map.find(pin_name);
            if (delay_it != delays_map.end()) {
                const Normal& gate_delay = delay_it->second;
                double contribution = input_lat + gate_delay->mean();
                
                // Check if this input contributes to the output LAT (within tolerance)
                // Use a more relaxed tolerance (5.0) to account for floating point errors
                // and MAX operation approximations (MAX operation can cause larger differences)
                if (contribution > max_contribution) {
                    // Always track the maximum contribution
                    max_contribution = contribution;
                    critical_input = input_signal;
                }
            } else {
                // Delay not found for this pin - use input LAT as fallback
                if (input_lat > max_contribution) {
                    max_contribution = input_lat;
                    critical_input = input_signal;
                }
            }
            
            pin_index++;
        }
        
        // If no critical input found with tolerance check, find input with maximum LAT + gate_delay
        // This handles cases where MAX operation causes slight differences
        if (critical_input.empty() && !input_signals.empty()) {
            pin_index = 0;
            for (const auto& input_signal : input_signals) {
                auto input_sig_it = signals_.find(input_signal);
                if (input_sig_it != signals_.end()) {
                    double input_lat = input_sig_it->second->mean();
                    std::string pin_name = std::to_string(pin_index);
                    
                    auto delay_it2 = delays_map.find(pin_name);
                    if (delay_it2 != delays_map.end()) {
                        const Normal& gate_delay = delay_it2->second;
                        double contribution = input_lat + gate_delay->mean();
                        if (contribution > max_contribution) {
                            max_contribution = contribution;
                            critical_input = input_signal;
                        }
                    } else {
                        // Delay not found, just use LAT
                        if (input_lat > max_contribution) {
                            max_contribution = input_lat;
                            critical_input = input_signal;
                        }
                    }
                }
                pin_index++;
            }
        }
        
        // If still no critical input found, use the input with maximum LAT
        // This is a fallback for edge cases
        if (critical_input.empty() && !input_signals.empty()) {
            double max_lat = std::numeric_limits<double>::lowest();
            for (const auto& input_signal : input_signals) {
                auto input_sig_it = signals_.find(input_signal);
                if (input_sig_it != signals_.end()) {
                    double input_lat = input_sig_it->second->mean();
                    if (input_lat > max_lat) {
                        max_lat = input_lat;
                        critical_input = input_signal;
                    }
                }
            }
        }
        
        // Add instance to path
        instance_path.push_back(instance_name);
        
        // Continue building path from critical input
        // Always try to build path - if critical_input is empty, use first available input
        if (!critical_input.empty()) {
            build_path(critical_input, node_path, instance_path, output_lat);
        } else if (!input_signals.empty()) {
            // No critical input found - use first available input
            build_path(input_signals[0], node_path, instance_path, output_lat);
        } else {
            // No inputs at all - create path up to current node (shouldn't happen)
            finalize_path(node_path, instance_path, output_lat);
            return;
        }
        
        // Backtrack
        instance_path.pop_back();
        node_path.pop_back();
    };
    
    // Build paths from each output and DFF input
    // Collect all path endpoints: outputs and DFF inputs (D terminals)
    std::set<std::string> path_endpoints;
    path_endpoints.insert(outputs_.begin(), outputs_.end());
    path_endpoints.insert(dff_inputs_.begin(), dff_inputs_.end());
    
    for (const auto& endpoint : path_endpoints) {
        // Check if endpoint signal exists in signals_
        auto sig_it = signals_.find(endpoint);
        if (sig_it == signals_.end()) {
            continue;
        }
        
        // Get endpoint LAT value
        double endpoint_lat = sig_it->second->mean();
        
        std::vector<std::string> node_path;
        std::vector<std::string> instance_path;
        build_path(endpoint, node_path, instance_path, endpoint_lat);
    }
    
    // Sort paths by delay (descending), then by endpoint name (ascending) for stability
    std::sort(paths.begin(), paths.end(), 
              [](const CriticalPath& a, const CriticalPath& b) {
                  if (a.delay_mean != b.delay_mean) {
                      return a.delay_mean > b.delay_mean;
                  }
                  // Same delay: sort by endpoint name for deterministic ordering
                  if (!a.node_names.empty() && !b.node_names.empty()) {
                      return a.node_names.back() < b.node_names.back();
                  }
                  return false;
              });
    
    if (paths.size() > top_n) {
        paths.resize(top_n);
    }
    
    return paths;
}

}  // namespace Nh
