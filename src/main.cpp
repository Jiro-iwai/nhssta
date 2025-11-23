// -*- c++ -*-
// Author: IWAI Jiro

#include <iostream>
#include <string>
#include <cstring>
#include "ssta.hpp"
#include "exception.hpp"

using namespace std;

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

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
void set_option(int argc, char *argv[], Nh::Ssta* ssta) {
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        // Handle long options
        if (arg == "--help") {
            usage(0, 0);
        } else if (arg == "--lat") {
            ssta->set_lat();
        } else if (arg == "--correlation") {
            ssta->set_correlation();
        } else if (arg == "--dlib") {
            if (i + 1 < argc) {
                ssta->set_dlib(string(argv[++i]));
            } else {
                usage(0, 0);
            }
        } else if (arg == "--bench") {
            if (i + 1 < argc) {
                ssta->set_bench(string(argv[++i]));
            } else {
                usage(0, 0);
            }
        }
        // Handle short options
        else if (arg.length() == 2 && arg[0] == '-') {
            switch (arg[1]) {
                case 'h':
                    usage(0, 0);
                    break;
                case 'l':
                    ssta->set_lat();
                    break;
                case 'c':
                    ssta->set_correlation();
                    break;
                case 'd':
                    if (i + 1 < argc) {
                        ssta->set_dlib(string(argv[++i]));
                    } else {
                        usage(0, 0);
                    }
                    break;
                case 'b':
                    if (i + 1 < argc) {
                        ssta->set_bench(string(argv[++i]));
                    } else {
                        usage(0, 0);
                    }
                    break;
                default:
                    usage(0, 0);
                    break;
            }
        } else {
            // Invalid option
            usage(0, 0);
        }
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

    } catch ( Nh::Exception& e ) {
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
