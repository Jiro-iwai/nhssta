// -*- c++ -*-
// Author: IWAI Jiro (refactored)
//
// Data structures for SSTA results
// Phase 2: I/O and logic separation

#ifndef NH_SSTA_RESULTS__H
#define NH_SSTA_RESULTS__H

#include <string>
#include <vector>
#include <map>

namespace Nh {

    // LAT (Latest Arrival Time) result for a single signal
    struct LatResult {
        std::string node_name;
        double mean;
        double std_dev;  // standard deviation
        
        LatResult() : mean(0.0), std_dev(0.0) {}
        LatResult(const std::string& name, double m, double sd) 
            : node_name(name), mean(m), std_dev(sd) {}
    };

    // Collection of LAT results
    typedef std::vector<LatResult> LatResults;

    // Correlation matrix result
    struct CorrelationMatrix {
        std::vector<std::string> node_names;
        std::map<std::pair<std::string, std::string>, double> correlations;
        
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

}

#endif // NH_SSTA_RESULTS__H

