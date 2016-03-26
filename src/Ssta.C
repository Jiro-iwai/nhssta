// -*- c++ -*-
// Authors: IWAI Jiro	     
// $Id: Ssta.C,v 1.5 2007/08/14 18:01:20 jiro Exp $

#include <iostream>
#include <cassert>
#include <cmath>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include "Util.h"
#include "Ssta.h"
#include "ADD.h"

namespace Nh {

    std::string date() {
        time_t t = time(0);
        char *s,*p;
        p = s = asctime(localtime(&t));
        while(*s != '\0') { if (*s == '\n') {*s = '\0'; break;} else {s++;}}
        return std::string(p);
    }

    Ssta::Ssta() : is_lat_(false), is_correlation_(false)
    {
        std::cerr << "nhssta 0.0.8 (" << date() << ")" << std::endl;
    }

    Ssta::~Ssta() {
        std::cerr << "OK" << std::endl;
    }

    void Ssta::check() {

        int error = 0;

        if( dlib_.empty() ) {
            std::cerr << "error: please specify `-d' properly" << std::endl;
            error++;
        }

        if( bench_.empty() ) {
            std::cerr << "error: please specify `-b' properly" << std::endl;
            error++;
        }

        if( error ) exit(1);
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
        throw exception(what);
    }

    // dlib //

    void Ssta::read_dlib() {

        try {

            Parser parser(dlib_, '#', "(),", " \t\r");
            parser.checkFile();

            while ( parser.getLine() ) {
                read_dlib_line(parser);
            }

        } catch( Parser::exception& e ){
            throw exception(e.what());
        }
    }

    void Ssta::read_dlib_line(Parser& parser) {

        Gate g;

        std::string gate_name;;
        parser.getToken(gate_name);
        Gates::const_iterator i = gates_.find(gate_name);
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
        if( !(type == "gauss" || type == "const") )
            parser.unexpectedToken();

        parser.checkSepalator('(');

        double mean;
        parser.getToken(mean);
        if( mean < 0.0 )
            parser.unexpectedToken();

        double variance;
        if( type == "gauss" ) { // gaussian
            parser.checkSepalator(',');

            double sigma;
            parser.getToken(sigma);
            if( sigma < 0.0 )
                parser.unexpectedToken();

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

        } catch ( SmartPtrException& e ) {
            throw exception(e.what());

        } catch ( ::RandomVariable::Exception& e ) {
            throw exception(e.what());

        } catch ( Gate::exception& e ) {
            throw exception(e.what());

        } catch ( Parser::exception& e ) {
            throw exception(e.what());

        }

        gates_.clear();
    }

    void Ssta::read_bench_input(Parser& parser) {

        parser.checkSepalator('(');

        std::string signal_name;
        parser.getToken(signal_name);
        Signals::const_iterator si = signals_.find(signal_name);
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
        Pins::const_iterator si = outputs_.find(signal_name);
        if( si != outputs_.end() ) {
            node_error("output",signal_name);
        }
        outputs_.insert(signal_name);

        parser.checkSepalator(')');
        parser.checkEnd();
    }

    static void tolower_string(std::string& token){
        unsigned int i;
        for( i = 0; i < token.size(); i++ )
            token[i] = tolower(token[i]);
    }

    bool Ssta::is_line_ready(const NetLine& line) const {
        Ins ins = line->ins();
        Ins::const_iterator j = ins.begin();
        for( ; j != ins.end(); j++ ){
            const std::string& signal_name = *j;
            Signals::const_iterator si = signals_.find(signal_name);
            if( si == signals_.end() ){
                return false;
            }
        }
        return true;
    }

    // don't use for dff
    void Ssta::set_instance_input(const Instance& inst, const Ins& ins) {
        int ith = 0;
        Ins::const_iterator j = ins.begin();
        for( ; j != ins.end(); j++ ){

            const std::string& signal_name = *j;
            const RandomVariable& signal = signals_[signal_name];
            std::string pin = boost::lexical_cast<std::string>(ith);
            inst->set_input(pin, signal);

            ith++;
        }
    }

    void Ssta::check_signal(const std::string& signal_name) const {
        Signals::const_iterator si = signals_.find(signal_name);
        if( si != signals_.end() ) {
            node_error("node",signal_name);
        }
    }

    void Ssta::connect_instances() {

        while( !net_.empty() ) {

            unsigned int size = net_.size();

            Net::iterator ni = net_.begin();
            while( ni != net_.end() ){

                const NetLine& line = *ni;
                const Ins& ins = line->ins();
                assert( line->gate() != "dff" );

                if( is_line_ready(line) ) {

                    const std::string& gate_name = line->gate();
                    Gates::const_iterator gi = gates_.find(gate_name);

                    Gate gate = gi->second;
                    Instance inst = gate->create_instance();

                    set_instance_input(inst, ins);

                    const RandomVariable& out = inst->output();
                    const std::string& out_signal_name = line->out();

                    check_signal(out_signal_name);
                    signals_[out_signal_name] = out;
                    out->set_name(out_signal_name);

                    net_.erase(ni++);
                } else
                    ni++;
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
                throw exception(what);
            }
        }
    }

    // treat ck of dff as input
    void Ssta::set_dff_out(const std::string& out_signal_name) {

        Normal in(0.0,::RandomVariable::minimum_variance); //////
        Gates::const_iterator gi = gates_.find("dff");
        Gate dff = gi->second;

        Normal delay = dff->delay("ck","q");
        Normal dff_delay = delay->clone();
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
        Gates::const_iterator gi = gates_.find(gate_name);
        if( gi == gates_.end() ){
            std::string what = "unknown gate \"";
            what += gate_name;
            what += "\"";
            what += " at line ";
            int line = parser.getNumLine();
            what += boost::lexical_cast<std::string>(line);
            what += " of file \"";
            what += parser.getFileName();
            what += "\"";
            throw exception(what);
        }

        l->set_gate(gate_name);

        parser.checkSepalator('(');

        Ins& ins = l->ins();
        while(1) {

            std::string in_signal_name;
            parser.getToken(in_signal_name);
            ins.push_back(in_signal_name);

            char sep;
            parser.getToken(sep);
            if( sep == ')' ) {
                break;
            } else if( sep == ',' ) {
                continue;
            } else {
                parser.unexpectedToken();
            }
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

        } catch ( SmartPtrException& e ) {
            throw exception(e.what());

        } catch ( ::RandomVariable::Exception& e ) {
            throw exception(e.what());
        }
    }

    void Ssta::report_lat() const {

        std::cout << "#" << std::endl;
        std::cout << "# LAT" << std::endl;
        std::cout << "#" << std::endl;
        std::cout << "#node		     mu	     std" << std::endl;
        std::cout << "#---------------------------------" << std::endl;

        Signals::const_iterator si = signals_.begin();
        for( ; si != signals_.end(); si++ ) {
            const RandomVariable& sigi = si->second;
            double sigma = sqrt(si->second->variance());
            std::cout << boost::format("%-15s") % sigi->name().c_str();
            std::cout << boost::format("%10.3f") % si->second->mean();
            std::cout << boost::format("%9.3f") % sigma << std::endl;
        }

        std::cout << "#---------------------------------" << std::endl;
    }

    void Ssta::print_line() const {
        bool isfirst = true;
        Signals::const_iterator si = signals_.begin();
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

        std::cout << "#" << std::endl;
        std::cout << "# correlation matrix" << std::endl;
        std::cout << "#" << std::endl;

        //print_line(); //

        std::cout << "#\t";
        Signals::const_iterator si = signals_.begin();
        for( ; si != signals_.end(); si++ ) {
            const RandomVariable& sigi = si->second;
            std::cout << boost::format("%s\t") % sigi->name().c_str();
        }
        std::cout << std::endl;

        print_line(); //

        si = signals_.begin();
        for( ; si != signals_.end(); si++ ) {
            const RandomVariable& sigi = si->second;
            double vi = sigi->variance();
            std::cout << boost::format("%s\t") % sigi->name().c_str();

            Signals::const_iterator sj = signals_.begin();
            for( ; sj != signals_.end(); sj++ ) {
                const RandomVariable& sigj = sj->second;
                double vj = sigj->variance();
                double cov = covariance(sigi,sigj);
                std::cout << boost::format("%4.3f\t") % (cov/sqrt(vi*vj));
                std::cout.flush();
            }

            std::cout << std::endl;
        }

        print_line(); //
    }

}
