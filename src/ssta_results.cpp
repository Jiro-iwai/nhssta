// -*- c++ -*-
// Author: IWAI Jiro (refactored)
//
// SstaResults class implementation: Results data generation
// Phase 4: Separation of results data conversion from Ssta class

#include "ssta_results.hpp"
#include "covariance.hpp"
#include <algorithm>
#include <cmath>

namespace Nh {

SstaResults::SstaResults(const CircuitGraph* graph)
    : graph_(graph) {
}

LatResults SstaResults::getLatResults() const {
    LatResults results;
    
    const auto& signals = graph_->signals();
    
    // Collect signal names and sort them for consistent output order
    // (unordered_map doesn't guarantee order, so we sort by key)
    std::vector<std::string> signal_names;
    signal_names.reserve(signals.size());  // Pre-allocate to avoid reallocations
    for (const auto& pair : signals) {
        signal_names.push_back(pair.first);
    }
    std::sort(signal_names.begin(), signal_names.end());

    for (const auto& signal_name : signal_names) {
        const auto& sigi = signals.at(signal_name);
        double mean = sigi->mean();
        double variance = sigi->variance();
        double sigma = sqrt(variance);

        results.emplace_back(sigi->name(), mean, sigma);
    }

    return results;
}

CorrelationMatrix SstaResults::getCorrelationMatrix() const {
    CorrelationMatrix matrix;
    
    const auto& signals = graph_->signals();
    
    // Collect signal names and sort them for consistent output order
    // (unordered_map doesn't guarantee order, so we sort by key)
    std::vector<std::string> signal_names;
    signal_names.reserve(signals.size());  // Pre-allocate to avoid reallocations
    for (const auto& pair : signals) {
        signal_names.push_back(pair.first);
    }
    std::sort(signal_names.begin(), signal_names.end());

    // Collect node names in sorted order
    for (const auto& signal_name : signal_names) {
        const auto& sigi = signals.at(signal_name);
        matrix.node_names.push_back(sigi->name());
    }

    // Calculate correlations
    for (const auto& signal_name_i : signal_names) {
        const auto& sigi = signals.at(signal_name_i);
        double vi = sigi->variance();

        for (const auto& signal_name_j : signal_names) {
            const auto& sigj = signals.at(signal_name_j);
            double vj = sigj->variance();
            double cov = covariance(sigi, sigj);
            double corr = (vi > 0.0 && vj > 0.0) ? (cov / sqrt(vi * vj)) : 0.0;

            matrix.correlations[std::make_pair(sigi->name(), sigj->name())] = corr;
        }
    }

    return matrix;
}

} // namespace Nh

