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
typedef std::vector<LatResult> LatResults;

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
typedef std::vector<CriticalPath> CriticalPaths;

}  // namespace Nh

#endif  // NH_SSTA_RESULTS__H
