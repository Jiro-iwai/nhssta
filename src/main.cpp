// -*- c++ -*-
// Author: IWAI Jiro

#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <nhssta/exception.hpp>
#include <nhssta/ssta.hpp>
#include <sstream>
#include <string>

void usage(const char* first, const char* last) {
    std::cerr << "usage: nhssta" << std::endl;
    std::cerr << " -d, --dlib         specifies .dlib file" << std::endl;
    std::cerr << " -b, --bench        specifies .bench file" << std::endl;
    std::cerr << " -l, --lat          prints all LAT data" << std::endl;
    std::cerr << " -c, --correlation  prints correlation matrix of LAT" << std::endl;
    std::cerr << " -h, --help         gives this help" << std::endl;
    throw Nh::RuntimeException("Invalid command-line arguments");
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
void set_option(int argc, char* argv[], Nh::Ssta* ssta) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // Handle long options
        if (arg == "--help") {
            usage(0, 0);
        } else if (arg == "--lat") {
            ssta->set_lat();
        } else if (arg == "--correlation") {
            ssta->set_correlation();
        } else if (arg == "--dlib") {
            if (i + 1 < argc) {
                ssta->set_dlib(std::string(argv[++i]));
            } else {
                usage(0, 0);
            }
        } else if (arg == "--bench") {
            if (i + 1 < argc) {
                ssta->set_bench(std::string(argv[++i]));
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
                        ssta->set_dlib(std::string(argv[++i]));
                    } else {
                        usage(0, 0);
                    }
                    break;
                case 'b':
                    if (i + 1 < argc) {
                        ssta->set_bench(std::string(argv[++i]));
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

// Helper function to get version string (moved from Ssta constructor)
std::string get_version_string() {
    auto now = std::time(nullptr);
    std::tm timeinfo{};  // Initialize to zero
#ifdef _WIN32
    localtime_s(&timeinfo, &now);  // Windows thread-safe version
#else
    localtime_r(&now, &timeinfo);  // POSIX thread-safe version
#endif
    
    std::ostringstream oss;
    oss << "nhssta 0.1.0 (";
    oss << std::put_time(&timeinfo, "%a %b %d %H:%M:%S %Y");
    oss << ")";
    return oss.str();
}

int main(int argc, char* argv[]) {
    try {
        // CLI layer: Output version information
        std::cerr << get_version_string() << std::endl;

        Nh::Ssta ssta;
        set_option(argc, argv, &ssta);
        ssta.check();
        ssta.read_dlib();
        ssta.read_bench();
        ssta.report();

        // CLI layer: Output success message
        std::cerr << "OK" << std::endl;

    } catch (Nh::Exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;

    } catch (std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 2;

    } catch (...) {
        std::cerr << "error: unknown error" << std::endl;
        return 3;
    }

    return 0;
}
