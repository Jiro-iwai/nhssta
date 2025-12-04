// -*- c++ -*-
// Author: IWAI Jiro (refactored)
//
// SstaResults class: Results data generation
// Phase 4: Separation of results data conversion from Ssta class

#ifndef NH_SSTA_RESULTS_CLASS__H
#define NH_SSTA_RESULTS_CLASS__H

#include <nhssta/ssta_results.hpp>
#include "circuit_graph.hpp"

namespace Nh {

class SstaResults {
public:
    explicit SstaResults(const CircuitGraph* graph);
    
    [[nodiscard]] LatResults getLatResults() const;
    [[nodiscard]] CorrelationMatrix getCorrelationMatrix() const;

private:
    const CircuitGraph* graph_;
};

} // namespace Nh

#endif // NH_SSTA_RESULTS_CLASS__H

