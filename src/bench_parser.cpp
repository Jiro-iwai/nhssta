// -*- c++ -*-
// Authors: IWAI Jiro
// BenchParser: .bench file parser for circuit description

#include "bench_parser.hpp"
#include "parser.hpp"
#include <nhssta/net_line.hpp>
#include <nhssta/exception.hpp>
#include <cctype>
#include <sstream>

namespace Nh {

const std::string BenchParser::DFF_GATE_NAME = "dff";

BenchParser::BenchParser(const std::string& bench_file)
    : bench_file_(bench_file) {
}

void BenchParser::parse(const Gates& gates) {
    Parser parser(bench_file_, '#', "(),=", " \t\r");
    parser.checkFile();

    while (parser.getLine()) {
        std::string keyword;
        parser.getToken(keyword);

        if (keyword == "INPUT") {
            parse_input(parser);
        } else if (keyword == "OUTPUT") {
            parse_output(parser);
        } else {
            parse_net(parser, keyword, gates);  // NET
        }
    }
}

void BenchParser::parse_input(Parser& parser) {
    parser.checkSeparator('(');

    std::string signal_name;
    parser.getToken(signal_name);
    auto si = inputs_.find(signal_name);
    if (si != inputs_.end()) {
        node_error("input", signal_name);
    }
    inputs_.insert(signal_name);

    parser.checkSeparator(')');
    parser.checkEnd();
}

void BenchParser::parse_output(Parser& parser) {
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

void BenchParser::parse_net(Parser& parser, const std::string& out_signal_name, const Gates& gates) {
    NetLine l;
    l->set_out(out_signal_name);

    parser.checkSeparator('=');

    std::string gate_name;
    parser.getToken(gate_name);
    tolower_string(gate_name);
    
    // Check if gate exists in gate library (for ParseException compatibility)
    auto gi = gates.find(gate_name);
    if (gi == gates.end()) {
        throw Nh::ParseException(parser.getFileName(), parser.getNumLine(),
                                 "unknown gate \"" + gate_name + "\"");
    }

    l->set_gate(gate_name);

    parser.checkSeparator('(');

    NetLineIns& ins = l->ins();
    // Reserve space for input signals (typical gates have 2-4 inputs)
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
        // Record DFF output and input
        dff_outputs_.insert(out_signal_name);
        // Record DFF data input (D terminal) for critical path analysis
        if (!ins.empty()) {
            dff_inputs_.insert(ins[0]);
        }
        // DFF is not added to net (handled separately in CircuitGraph)
    } else {
        net_.push_back(l);
    }
}

void BenchParser::node_error(const std::string& head, const std::string& signal_name) const {
    std::ostringstream what;
    what << head << " \"" << signal_name << "\" is multiply defined in file \"" << bench_file_ << "\"";
    throw Nh::RuntimeException(what.str());
}

}  // namespace Nh

