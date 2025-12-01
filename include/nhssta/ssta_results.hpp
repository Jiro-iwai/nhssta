// -*- c++ -*-
// Author: IWAI Jiro (refactored)
//
// Data structures for SSTA results
// Phase 2: I/O and logic separation

#ifndef NH_SSTA_RESULTS__H
#define NH_SSTA_RESULTS__H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Nh {

// Custom hash function for std::pair<std::string, std::string>
struct StringPairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        // Combine hashes
        return h1 ^ (h2 << 1);
    }
};

// LAT (Latest Arrival Time) result for a single signal
struct LatResult {
    std::string node_name;
    double mean;
    double std_dev;  // standard deviation

    LatResult()
        : mean(0.0)
        , std_dev(0.0) {}
    LatResult(const std::string& name, double m, double sd)
        : node_name(name)
        , mean(m)
        , std_dev(sd) {}
};

// Collection of LAT results
using LatResults = std::vector<LatResult>;

// Correlation matrix result
struct CorrelationMatrix {
    std::vector<std::string> node_names;
    std::unordered_map<std::pair<std::string, std::string>, double, StringPairHash> correlations;

    // Get correlation between two nodes
    [[nodiscard]] double getCorrelation(const std::string& node1, const std::string& node2) const {
        auto it = correlations.find(std::make_pair(node1, node2));
        if (it != correlations.end()) {
            return it->second;
        }
        // Try reverse order
        it = correlations.find(std::make_pair(node2, node1));
        if (it != correlations.end()) {
            return it->second;
        }
        // Same node has correlation 1.0
        if (node1 == node2) {
            return 1.0;
        }
        return 0.0;
    }
};

// Critical path (complete path from input to output)
struct CriticalPath {
    std::vector<std::string> node_names;      // Signal node names (e.g., A, N1, Y)
    std::vector<std::string> instance_names;  // Gate instance names (e.g., gate1:0, gate2:1)
    double delay_mean;                        // Total path delay (mean)
    LatResults node_stats;                    // LAT-style stats for each node along the path

    CriticalPath()
        : delay_mean(0.0) {}
    CriticalPath(const std::vector<std::string>& nodes, const std::vector<std::string>& instances, double delay)
        : node_names(nodes)
        , instance_names(instances)
        , delay_mean(delay) {}
    CriticalPath(const std::vector<std::string>& nodes, const std::vector<std::string>& instances, double delay,
                 const LatResults& stats)
        : node_names(nodes)
        , instance_names(instances)
        , delay_mean(delay)
        , node_stats(stats) {}
};

// Collection of critical paths (top N paths)
using CriticalPaths = std::vector<CriticalPath>;

// Sensitivity analysis result for a single gate
struct GateSensitivity {
    std::string gate_name;    // Legacy: "instance:pin" format
    std::string instance;     // Instance name (e.g., "inv:0")
    std::string output_node;  // Output signal name (e.g., "n1", "Y")
    std::string input_signal; // Input signal name (e.g., "A", "N2")
    std::string gate_type;    // Gate type (e.g., "inv", "and")
    double grad_mu;    // ∂F/∂μ
    double grad_sigma; // ∂F/∂σ

    GateSensitivity()
        : grad_mu(0.0)
        , grad_sigma(0.0) {}
    GateSensitivity(const std::string& name, double gm, double gs)
        : gate_name(name)
        , grad_mu(gm)
        , grad_sigma(gs) {}
    GateSensitivity(const std::string& inst, const std::string& out_node,
                    const std::string& in_sig, const std::string& type,
                    double gm, double gs)
        : gate_name(inst + ":" + in_sig)
        , instance(inst)
        , output_node(out_node)
        , input_signal(in_sig)
        , gate_type(type)
        , grad_mu(gm)
        , grad_sigma(gs) {}
};

// Path info for sensitivity analysis (with score = LAT + σ)
struct SensitivityPath {
    std::string endpoint;
    double lat;
    double std_dev;
    double score;  // LAT + σ

    SensitivityPath()
        : lat(0.0)
        , std_dev(0.0)
        , score(0.0) {}
    SensitivityPath(const std::string& ep, double l, double s)
        : endpoint(ep)
        , lat(l)
        , std_dev(s)
        , score(l + s) {}
};

// Sensitivity analysis results
struct SensitivityResults {
    std::vector<SensitivityPath> top_paths;
    std::vector<GateSensitivity> gate_sensitivities;
    double objective_value{0.0};  // log(Σ exp(LAT + σ))

    SensitivityResults() = default;
};

}  // namespace Nh

#endif  // NH_SSTA_RESULTS__H
