// -*- c++ -*-
// Authors: IWAI Jiro
// DlibParser: .dlib file parser for gate library

#include "dlib_parser.hpp"
#include "parser.hpp"
#include "normal.hpp"
#include <nhssta/gate.hpp>
#include <nhssta/exception.hpp>

namespace Nh {

DlibParser::DlibParser(const std::string& dlib_file)
    : dlib_file_(dlib_file) {
}

void DlibParser::parse() {
    Parser parser(dlib_file_, '#', "(),", " \t\r");
    parser.checkFile();

    while (parser.getLine()) {
        parse_line(parser);
    }
}

void DlibParser::parse_line(Parser& parser) {
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

}  // namespace Nh

