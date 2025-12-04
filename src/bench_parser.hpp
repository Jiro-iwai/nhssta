// -*- c++ -*-
// Authors: IWAI Jiro
// BenchParser: .bench file parser for circuit description

#ifndef NH_BENCH_PARSER_H
#define NH_BENCH_PARSER_H

#include <list>
#include <set>
#include <string>
#include <unordered_map>
#include <nhssta/gate.hpp>
#include <nhssta/net_line.hpp>

namespace Nh {

// Forward declaration
class Parser;
using Gates = std::unordered_map<std::string, Gate>;

class BenchParser {
public:
    using Pins = std::set<std::string>;
    using Net = std::list<NetLine>;
    
    explicit BenchParser(const std::string& bench_file);
    void parse(const Gates& gates);
    void parse() { parse(Gates()); }  // Backward compatibility: parse without gates (for tests)
    
    [[nodiscard]] const Pins& inputs() const { return inputs_; }
    [[nodiscard]] const Pins& outputs() const { return outputs_; }
    [[nodiscard]] const Pins& dff_outputs() const { return dff_outputs_; }
    [[nodiscard]] const Pins& dff_inputs() const { return dff_inputs_; }
    [[nodiscard]] const Net& net() const { return net_; }
    
private:
    void parse_input(Parser& parser);
    void parse_output(Parser& parser);
    void parse_net(Parser& parser, const std::string& out_signal_name, const Gates& gates);
    void node_error(const std::string& head, const std::string& signal_name) const;
    
    std::string bench_file_;
    Pins inputs_;
    Pins outputs_;
    Pins dff_outputs_;
    Pins dff_inputs_;
    Net net_;
    
    static const std::string DFF_GATE_NAME;
};

}  // namespace Nh

#endif  // NH_BENCH_PARSER_H

