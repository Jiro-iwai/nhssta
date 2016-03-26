// -*- c++ -*-
// Author: IWAI Jiro

#include <iostream>
#include <boost/spirit.hpp>
#include "Ssta.h"

using namespace std;
using namespace boost::spirit;

void usage(const char* first, const char* last) {
    cerr << "usage: nhssta" << endl;
    cerr << " -d, --dlib         specifies .dlib file" << endl;
    cerr << " -b, --bench        specifies .bench file"	<< endl;
    cerr << " -l, --lat          prints all LAT data"  << endl;
    cerr << " -c, --correlation  prints correlation matrix of LAT"
		 << endl;
    cerr << " -h, --help         gives this help" << endl;
    exit(1);
}

struct SetBase {
    Nh::Ssta* ssta_;
    SetBase(Nh::Ssta* ssta) : ssta_(ssta) {}
    virtual void operator()(const char* first, const char* last) const = 0;
};

struct Set_lat : public SetBase {
    Set_lat(Nh::Ssta* ssta) : SetBase(ssta) {}
    void operator()(const char* first, const char* last) const {
		ssta_->set_lat();
    }
};

struct Set_correlation : public SetBase {
    Set_correlation(Nh::Ssta* ssta) : SetBase(ssta) {}
    void operator()(const char* first, const char* last) const {
		ssta_->set_correlation();
    }
};

struct Set_bench : public SetBase {
    Set_bench(Nh::Ssta* ssta) : SetBase(ssta) {}
    void operator()(const char* first, const char* last) const {
		ssta_->set_bench(string(first,last));
    }
};

struct Set_dlib : public SetBase {
    Set_dlib(Nh::Ssta* ssta) : SetBase(ssta) {}
    void operator()(const char* first, const char* last) const {
		ssta_->set_dlib(string(first,last));
    }
};

struct CmmdLineGrammar : public grammar < CmmdLineGrammar > {

    Nh::Ssta* ssta_;
    CmmdLineGrammar(Nh::Ssta* ssta) : ssta_(ssta) {}

    template <typename ScannerT> struct definition {

		rule<ScannerT> options;
		rule<ScannerT> dlib;
		rule<ScannerT> bench;
		rule<ScannerT> lat;
		rule<ScannerT> correlation;
		rule<ScannerT> help;
		rule<ScannerT> file;

		definition(CmmdLineGrammar const& self) {

			Set_lat set_lat(self.ssta_);
			Set_correlation set_correlation(self.ssta_);
			Set_bench set_bench(self.ssta_);
			Set_dlib set_dlib(self.ssta_);

			options 
				= *( lat | correlation | dlib | bench ) >> end_p
				| help >> end_p;

			lat   
				= ( str_p("-l") | str_p("--lat") )[set_lat];

			correlation 
				= ( str_p("-c") | str_p("--correlation") )[set_correlation];

			dlib  
				=  ( str_p("-d") | str_p("--dlib") ) >> file[set_dlib];

			bench 
				=  ( str_p("-b") | str_p("--bench") ) >> file[set_bench];

			file
				= lexeme_d[+graph_p];

			help  
				= ( str_p("-h") | str_p("--help") )[&usage];

		}

		rule<ScannerT> const& start() const {
			return options;                    
		}
    };
};

void set_option(int argc, char *argv[], Nh::Ssta* ssta) {

    string cmmdline;  
    for(int i = 1; i < argc; i++){
		cmmdline += argv[i];
		cmmdline += " ";
    }

    CmmdLineGrammar gr(ssta);
    if(!parse(cmmdline.c_str(), gr, space_p).full) { 
		usage(0,0);
    }
}

int main(int argc, char *argv[]) {

    try {
		Nh::Ssta ssta;
		set_option(argc, argv, &ssta);
		ssta.check();
		ssta.read_dlib();
		ssta.read_bench();
		ssta.report();

    } catch ( Nh::Ssta::exception& e ) {
		cerr << "error: " << e.what() << endl;
		exit(1);

    } catch ( exception& e ) {
		cerr << "error: " << e.what() << endl;
		exit(2);

    } catch ( ... ) {
		cerr << "unknown error" << endl;
		exit(3);
    }

    exit(0);
}
