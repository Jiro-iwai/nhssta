// -*- c++ -*-
// Authors: IWAI Jiro
// CriticalPathAnalyzer: Critical path analysis

#ifndef NH_CRITICAL_PATH_ANALYZER_H
#define NH_CRITICAL_PATH_ANALYZER_H

#include <nhssta/ssta_results.hpp>
#include "circuit_graph.hpp"

namespace Nh {

class CriticalPathAnalyzer {
public:
    explicit CriticalPathAnalyzer(const CircuitGraph& graph);
    [[nodiscard]] CriticalPaths analyze(size_t top_n) const;
    
private:
    const CircuitGraph& graph_;
};

}  // namespace Nh

#endif  // NH_CRITICAL_PATH_ANALYZER_H

