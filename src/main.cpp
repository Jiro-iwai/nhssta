// -*- c++ -*-
// Author: IWAI Jiro

#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <nhssta/exception.hpp>
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>
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
    oss << "nhssta 0.1.1 (";
    oss << std::put_time(&timeinfo, "%a %b %d %H:%M:%S %Y");
    oss << ")";
    return oss.str();
}

// Helper function to format LAT results (matching report_lat() format)
std::string formatLatResults(const Nh::LatResults& results) {
    std::ostringstream oss;
    oss << "#" << std::endl;
    oss << "# LAT" << std::endl;
    oss << "#" << std::endl;
    oss << std::left << std::setw(15) << "#node" << std::right << std::setw(10) << "mu" << std::setw(9) << "std" << std::endl;
    oss << "#---------------------------------" << std::endl;

    for (const auto& result : results) {
        oss << std::left << std::setw(15) << result.node_name;
        oss << std::right << std::setw(10) << std::fixed << std::setprecision(3)
            << result.mean;
        oss << std::right << std::setw(9) << std::fixed << std::setprecision(3)
            << result.std_dev << std::endl;
    }

    oss << "#---------------------------------" << std::endl;
    return oss.str();
}

// Helper function to format correlation matrix (matching report_correlation() format)
std::string formatCorrelationMatrix(const Nh::CorrelationMatrix& matrix) {
    std::ostringstream oss;
    oss << "#" << std::endl;
    oss << "# correlation matrix" << std::endl;
    oss << "#" << std::endl;

    oss << "#\t";
    for (const auto& node_name : matrix.node_names) {
        oss << node_name << "\t";
    }
    oss << std::endl;

    // Print separator line
    oss << "#-------";
    for (size_t i = 1; i < matrix.node_names.size(); i++) {
        oss << "--------";
    }
    oss << "-----" << std::endl;

    for (const auto& node_name : matrix.node_names) {
        oss << node_name << "\t";

        for (const auto& other_name : matrix.node_names) {
            double corr = matrix.getCorrelation(node_name, other_name);
            oss << std::fixed << std::setprecision(3) << std::setw(4) << corr << "\t";
        }

        oss << std::endl;
    }

    // Print separator line
    oss << "#-------";
    for (size_t i = 1; i < matrix.node_names.size(); i++) {
        oss << "--------";
    }
    oss << "-----" << std::endl;

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

        // CLI layer: Output results using getLatResults() and getCorrelationMatrix()
        // This separates I/O from business logic
        if (ssta.is_lat() || ssta.is_correlation()) {
            std::cout << std::endl;
        }

        if (ssta.is_lat()) {
            Nh::LatResults lat_results = ssta.getLatResults();
            std::cout << formatLatResults(lat_results);
        }

        if (ssta.is_correlation()) {
            std::cout << std::endl;
            Nh::CorrelationMatrix corr_matrix = ssta.getCorrelationMatrix();
            std::cout << formatCorrelationMatrix(corr_matrix);
        }

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
