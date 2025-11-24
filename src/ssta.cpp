// -*- c++ -*-
// Authors: IWAI Jiro	     

#include <iostream>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include "util_numerical.hpp"
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>
#include "add.hpp"

namespace Nh {

    Ssta::Ssta() = default;

    Ssta::~Ssta() = default;

    void Ssta::check() {
        // Validate configuration and throw exception if invalid
        // Error messages should be handled by CLI layer
        
        std::string error_msg;
        
        if( dlib_.empty() ) {
            if (!error_msg.empty()) {
                error_msg += "; ";
            }
            error_msg += "please specify `-d' properly";
        }

        if( bench_.empty() ) {
            if (!error_msg.empty()) {
                error_msg += "; ";
            }
            error_msg += "please specify `-b' properly";
        }

        if( !error_msg.empty() ) {
            throw Nh::Exception("error: " + error_msg);
        }
    }

    //// read ////

    void Ssta::node_error
    (
        const std::string& head,
        const std::string& signal_name
        ) const
    {
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

        try {

            Parser parser(dlib_, '#', "(),", " \t\r");
            parser.checkFile();

            while ( parser.getLine() ) {
                read_dlib_line(parser);
            }

        } catch( Nh::FileException& e ){
            throw Nh::Exception(e.what());
        } catch( Nh::ParseException& e ){
            throw Nh::Exception(e.what());
        }
    }

    void Ssta::read_dlib_line(Parser& parser) {

        Gate g;

        std::string gate_name;;
        parser.getToken(gate_name);
        auto i = gates_.find(gate_name);
        if( i != gates_.end() ){
            g = i->second;
        } else {
            g->set_type_name(gate_name);
            gates_[gate_name] = g;
        }

        std::string in;
        parser.getToken(in);

        std::string out;
        parser.getToken(out);

        std::string type; // distribution type
        parser.getToken(type);
        if( !(type == "gauss" || type == "const") ) {
            parser.unexpectedToken();
        }

        parser.checkSepalator('(');

        double mean = 0.0;
        parser.getToken(mean);
        if( mean < 0.0 ) {
            parser.unexpectedToken();
        }

        double variance = 0.0;
        if( type == "gauss" ) { // gaussian
            parser.checkSepalator(',');

            double sigma = 0.0;
            parser.getToken(sigma);
            if( sigma < 0.0 ) {
                parser.unexpectedToken();
            }

            variance = sigma*sigma;

        } else { // constant
            variance = 0.0;
        }
        Normal delay(mean, variance);
        g->set_delay(in, out, delay);

        parser.checkSepalator(')');
        parser.checkEnd();
    }

    // bench //

    void Ssta::read_bench() {

        Parser parser(bench_, '#', "(),=", " \t\r");
        parser.checkFile();

        try {

            while ( parser.getLine() ) {

                std::string keyword;
                parser.getToken(keyword);

                if( keyword == "INPUT" ) {
                    read_bench_input(parser);
                } else if( keyword == "OUTPUT" ) {
                    read_bench_output(parser);
                } else {
                    read_bench_net(parser,keyword); // NET
                }
            }

            connect_instances();

        } catch ( Nh::RuntimeException& e ) {
            throw Nh::Exception(e.what());

        } catch ( Nh::FileException& e ) {
            throw Nh::Exception(e.what());

        } catch ( Nh::ParseException& e ) {
            throw Nh::Exception(e.what());

        }

        gates_.clear();
    }

    void Ssta::read_bench_input(Parser& parser) {

        parser.checkSepalator('(');

        std::string signal_name;
        parser.getToken(signal_name);
        auto si = signals_.find(signal_name);
        if( si != signals_.end() ) {
            node_error("input",signal_name);
        }
        inputs_.insert(signal_name);

        Normal in(0.0,::RandomVariable::minimum_variance); //////
        in->set_name(signal_name);
        signals_[signal_name] = in;

        parser.checkSepalator(')');
        parser.checkEnd();
    }

    void Ssta::read_bench_output(Parser& parser) {

        parser.checkSepalator('(');

        std::string signal_name;
        parser.getToken(signal_name);
        auto si = outputs_.find(signal_name);
        if( si != outputs_.end() ) {
            node_error("output",signal_name);
        }
        outputs_.insert(signal_name);

        parser.checkSepalator(')');
        parser.checkEnd();
    }

    static void tolower_string(std::string& token){
        for( char& c : token ) {
            c = static_cast<char>(tolower(c));
        }
    }

    bool Ssta::is_line_ready(const NetLine& line) const {
        Ins ins = line->ins();
        auto j = ins.begin();
        for( ; j != ins.end(); j++ ){
            const std::string& signal_name = *j;
            auto si = signals_.find(signal_name);
            if( si == signals_.end() ){
                return false;
            }
        }
        return true;
    }

    // don't use for dff
    void Ssta::set_instance_input(const Instance& inst, const Ins& ins) {
        int ith = 0;
        auto j = ins.begin();
        for( ; j != ins.end(); j++ ){

            const std::string& signal_name = *j;
            const RandomVariable& signal = signals_[signal_name];
            std::string pin = std::to_string(ith);
            inst->set_input(pin, signal);

            ith++;
        }
    }

    void Ssta::check_signal(const std::string& signal_name) const {
        auto si = signals_.find(signal_name);
        if( si != signals_.end() ) {
            node_error("node",signal_name);
        }
    }

    void Ssta::connect_instances() {

        while( !net_.empty() ) {

            unsigned int size = net_.size();

            auto ni = net_.begin();
            while( ni != net_.end() ){

                const NetLine& line = *ni;
                const Ins& ins = line->ins();
                // Note: dff gate check is now done in read_bench_net()

                if( is_line_ready(line) ) {

                    const std::string& gate_name = line->gate();
                    auto gi = gates_.find(gate_name);

                    Gate gate = gi->second;
                    Instance inst = gate.create_instance();

                    set_instance_input(inst, ins);

                    const RandomVariable& out = inst->output();
                    const std::string& out_signal_name = line->out();

                    check_signal(out_signal_name);
                    signals_[out_signal_name] = out;
                    out->set_name(out_signal_name);

                    net_.erase(ni++);
                } else {
                    ni++;
                }
            }

            // floating circuit
            if( size == net_.size() ) {
                std::string what = "following node is floating\n";
                ni = net_.begin();
                for( ; ni != net_.end(); ni++ ){
                    const NetLine& line = *ni;
                    what += line->out();
                    what += "\n";
                }
                throw Nh::RuntimeException(what);
            }
        }
    }

    // treat ck of dff as input
    void Ssta::set_dff_out(const std::string& out_signal_name) {

        Normal in(0.0,::RandomVariable::minimum_variance); //////
        auto gi = gates_.find("dff");
        Gate dff = gi->second;

        Normal delay = dff->delay("ck","q");
        Normal dff_delay = delay.clone();
        RandomVariable out = in + dff_delay; /////

        check_signal(out_signal_name);
        signals_[out_signal_name] = out;
        out->set_name(out_signal_name);
    }

    void Ssta::read_bench_net(Parser& parser,
                              const std::string& out_signal_name) {

        NetLine l;
        l->set_out(out_signal_name);

        parser.checkSepalator('=');

        std::string gate_name;
        parser.getToken(gate_name);
        tolower_string(gate_name);
        auto gi = gates_.find(gate_name);
        if( gi == gates_.end() ){
            std::string what = "unknown gate \"";
            what += gate_name;
            what += "\"";
            what += " at line ";
            int line = parser.getNumLine();
            what += std::to_string(line);
            what += " of file \"";
            what += parser.getFileName();
            what += "\"";
            throw Nh::ParseException(parser.getFileName(), parser.getNumLine(), "unknown gate \"" + gate_name + "\"");
        }

        l->set_gate(gate_name);

        parser.checkSepalator('(');

        Ins& ins = l->ins();
        // Reserve space for input signals (typical gates have 2-4 inputs)
        // This reduces memory reallocation overhead during parsing
        ins.reserve(4);
        while(true) {

            std::string in_signal_name;
            parser.getToken(in_signal_name);
            ins.push_back(in_signal_name);

            char sep = '\0';
            parser.getToken(sep);
            if( sep == ')' ) {
                break;
            }
            if( sep == ',' ) {
                continue;
            }
            parser.unexpectedToken();
        }

        parser.checkEnd();

        if( gate_name == "dff" ) {
            set_dff_out(out_signal_name);
        } else {
            net_.push_back(l);
        }
    }


    //// report ////

    void Ssta::report() {

        try {

            if( is_lat_ ){
                std::cout << std::endl;
                report_lat();
            }

            if( is_correlation_ ){
                std::cout << std::endl;
                report_correlation();
            }

        } catch ( Nh::RuntimeException& e ) {
            throw Nh::Exception(e.what());
        }
    }

    void Ssta::report_lat() const {
        LatResults results = getLatResults();

        std::cout << "#" << std::endl;
        std::cout << "# LAT" << std::endl;
        std::cout << "#" << std::endl;
        std::cout << "#node		     mu	     std" << std::endl;
        std::cout << "#---------------------------------" << std::endl;

        for (const auto& result : results) {
            std::cout << std::left << std::setw(15) << result.node_name;
            std::cout << std::right << std::setw(10) << std::fixed << std::setprecision(3) << result.mean;
            std::cout << std::right << std::setw(9) << std::fixed << std::setprecision(3) << result.std_dev << std::endl;
        }

        std::cout << "#---------------------------------" << std::endl;
    }

    void Ssta::print_line() const {
        bool isfirst = true;
        auto si = signals_.begin();
        for( ; si != signals_.end(); si++ ) {
            if( isfirst ){
                std::cout << "#-------";
                isfirst = false;
            } else {
                std::cout << "--------";
            }
        }
        std::cout << "-----" << std::endl;
    }

    void Ssta::report_correlation() const {
        CorrelationMatrix matrix = getCorrelationMatrix();

        std::cout << "#" << std::endl;
        std::cout << "# correlation matrix" << std::endl;
        std::cout << "#" << std::endl;

        std::cout << "#\t";
        for (const auto& node_name : matrix.node_names) {
            std::cout << node_name << "\t";
        }
        std::cout << std::endl;

        print_line(); //

        for (const auto& node_name : matrix.node_names) {
            std::cout << node_name << "\t";

            for (const auto& other_name : matrix.node_names) {
                double corr = matrix.getCorrelation(node_name, other_name);
                std::cout << std::fixed << std::setprecision(3) << std::setw(4) << corr << "\t";
                std::cout.flush();
            }

            std::cout << std::endl;
        }

        print_line(); //
    }

    // Pure logic functions (Phase 2: I/O separation)

    LatResults Ssta::getLatResults() const {
        LatResults results;
        
        // Collect signal names and sort them for consistent output order
        // (unordered_map doesn't guarantee order, so we sort by key)
        std::vector<std::string> signal_names;
        signal_names.reserve(signals_.size()); // Pre-allocate to avoid reallocations
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
        signal_names.reserve(signals_.size()); // Pre-allocate to avoid reallocations
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

}
