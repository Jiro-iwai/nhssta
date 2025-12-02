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
    std::cerr << " -p, --path [N]     prints top N critical paths (default: 5)" << std::endl;
    std::cerr << " -s, --sensitivity  prints sensitivity analysis" << std::endl;
    std::cerr << " -n, --top [N]      specifies top N paths for sensitivity (default: 5)" << std::endl;
    std::cerr << " -h, --help         gives this help" << std::endl;
    throw Nh::RuntimeException("Invalid command-line arguments");
}

// Helper function to parse optional numeric argument for -p/--path and -n/--top options
// Returns the parsed count, or default if no valid number is available
// Updates index i if a number argument was consumed
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
size_t parse_count(int argc, char* argv[], int& i, size_t default_value) {
    if (i + 1 < argc) {
        std::string next_arg = argv[i + 1];
        // Check if it's a number (not another option starting with '-')
        if (!next_arg.empty() && next_arg[0] != '-') {
            try {
                auto count = static_cast<size_t>(std::stoul(next_arg));
                i++;  // Consume the number argument
                return count;
            } catch (...) {
                // Conversion failed, use default
            }
        }
    }
    return default_value;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
size_t parse_path_count(int argc, char* argv[], int& i) {
    return parse_count(argc, argv, i, Nh::DEFAULT_CRITICAL_PATH_COUNT);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
void set_option(int argc, char* argv[], Nh::Ssta* ssta) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // Handle long options
        if (arg == "--help") {
            usage(nullptr, nullptr);
        } else if (arg == "--lat") {
            ssta->set_lat();
        } else if (arg == "--correlation") {
            ssta->set_correlation();
        } else if (arg == "--path") {
            ssta->set_critical_path(parse_path_count(argc, argv, i));
        } else if (arg == "--sensitivity") {
            ssta->set_sensitivity();
        } else if (arg == "--top") {
            ssta->set_sensitivity_top_n(parse_count(argc, argv, i, Nh::DEFAULT_CRITICAL_PATH_COUNT));
        } else if (arg == "--dlib") {
            if (i + 1 < argc) {
                ssta->set_dlib(std::string(argv[++i]));
            } else {
                usage(nullptr, nullptr);
            }
        } else if (arg == "--bench") {
            if (i + 1 < argc) {
                ssta->set_bench(std::string(argv[++i]));
            } else {
                usage(nullptr, nullptr);
            }
        }
        // Handle short options
        else if (arg.length() == 2 && arg[0] == '-') {
            switch (arg[1]) {
                case 'h':
                    usage(nullptr, nullptr);
                    break;
                case 'l':
                    ssta->set_lat();
                    break;
                case 'c':
                    ssta->set_correlation();
                    break;
                case 'p':
                    ssta->set_critical_path(parse_path_count(argc, argv, i));
                    break;
                case 's':
                    ssta->set_sensitivity();
                    break;
                case 'n':
                    ssta->set_sensitivity_top_n(parse_count(argc, argv, i, Nh::DEFAULT_CRITICAL_PATH_COUNT));
                    break;
                case 'd':
                    if (i + 1 < argc) {
                        ssta->set_dlib(std::string(argv[++i]));
                    } else {
                        usage(nullptr, nullptr);
                    }
                    break;
                case 'b':
                    if (i + 1 < argc) {
                        ssta->set_bench(std::string(argv[++i]));
                    } else {
                        usage(nullptr, nullptr);
                    }
                    break;
                default:
                    usage(nullptr, nullptr);
                    break;
            }
        } else {
            // Invalid option
            usage(nullptr, nullptr);
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
    oss << "nhssta 0.3.1 (";
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

// Helper function to format sensitivity analysis results
std::string formatSensitivityResults(const Nh::SensitivityResults& results) {
    std::ostringstream oss;
    oss << "#" << std::endl;
    oss << "# Sensitivity Analysis" << std::endl;
    oss << "#" << std::endl;
    oss << "# Objective: log(Σ exp(LAT + σ)) = " << std::fixed << std::setprecision(3) 
        << results.objective_value << std::endl;
    oss << "#" << std::endl;
    oss << "# Top " << results.top_paths.size() << " Endpoints (by LAT + σ):" << std::endl;
    oss << "#" << std::endl;
    
    oss << std::left << std::setw(18) << "#node" 
        << std::right << std::setw(10) << "LAT" 
        << std::setw(9) << "sigma" 
        << std::setw(10) << "score" << std::endl;
    oss << "#-----------------------------------------" << std::endl;
    
    for (const auto& path : results.top_paths) {
        oss << std::left << std::setw(18) << path.endpoint;
        oss << std::right << std::setw(10) << std::fixed << std::setprecision(3) << path.lat;
        oss << std::setw(9) << std::fixed << std::setprecision(3) << path.std_dev;
        oss << std::setw(10) << std::fixed << std::setprecision(3) << path.score << std::endl;
    }
    
    oss << "#-----------------------------------------" << std::endl;
    oss << "#" << std::endl;
    oss << "# Gate Sensitivities (∂F/∂μ, ∂F/∂σ):" << std::endl;
    oss << "#" << std::endl;
    
    oss << std::left << std::setw(12) << "#instance" 
        << std::setw(10) << "output"
        << std::setw(8) << "input"
        << std::setw(8) << "type"
        << std::right << std::setw(12) << "dF/dmu" 
        << std::setw(12) << "dF/dsigma" << std::endl;
    oss << "#-------------------------------------------------------------" << std::endl;
    
    for (const auto& sens : results.gate_sensitivities) {
        oss << std::left << std::setw(12) << sens.instance;
        oss << std::setw(10) << sens.output_node;
        oss << std::setw(8) << sens.input_signal;
        oss << std::setw(8) << sens.gate_type;
        oss << std::right << std::setw(12) << std::fixed << std::setprecision(6) << sens.grad_mu;
        oss << std::setw(12) << std::fixed << std::setprecision(6) << sens.grad_sigma << std::endl;
    }
    
    oss << "#-------------------------------------------------------------" << std::endl;
    
    return oss.str();
}

// Helper function to format critical paths
std::string formatCriticalPaths(const Nh::CriticalPaths& paths) {
    std::ostringstream oss;
    oss << "#" << std::endl;
    oss << "# critical paths" << std::endl;
    oss << "#" << std::endl;
    
    for (size_t i = 0; i < paths.size(); i++) {
        const auto& path = paths[i];
        oss << "# Path " << (i + 1) << " (delay: " << std::fixed << std::setprecision(3) << path.delay_mean << ")" << std::endl;
        oss << std::left << std::setw(15) << "#node" << std::right << std::setw(10) << "mu" << std::setw(9) << "std" << std::endl;
        oss << "#---------------------------------" << std::endl;

        if (!path.node_stats.empty()) {
            for (const auto& entry : path.node_stats) {
                oss << std::left << std::setw(15) << entry.node_name;
                oss << std::right << std::setw(10) << std::fixed << std::setprecision(3) << entry.mean;
                oss << std::right << std::setw(9) << std::fixed << std::setprecision(3) << entry.std_dev << std::endl;
            }
        } else {
            for (const auto& node_name : path.node_names) {
                oss << std::left << std::setw(15) << node_name;
                oss << std::right << std::setw(10) << std::fixed << std::setprecision(3) << 0.0;
                oss << std::right << std::setw(9) << std::fixed << std::setprecision(3) << 0.0 << std::endl;
            }
        }

        oss << "#---------------------------------" << std::endl;
        if (i + 1 < paths.size()) {
            oss << std::endl;
        }
    }
    
    oss << "#" << std::endl;
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
        if (ssta.is_lat() || ssta.is_correlation() || ssta.is_critical_path()) {
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

        if (ssta.is_critical_path()) {
            std::cout << std::endl;
            Nh::CriticalPaths paths = ssta.getCriticalPaths();
            std::cout << formatCriticalPaths(paths);
        }

        if (ssta.is_sensitivity()) {
            std::cout << std::endl;
            Nh::SensitivityResults sens_results = ssta.getSensitivityResults();
            std::cout << formatSensitivityResults(sens_results);
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
