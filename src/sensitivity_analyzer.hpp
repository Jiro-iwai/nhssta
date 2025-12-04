// -*- c++ -*-
// Authors: IWAI Jiro
// SensitivityAnalyzer: Sensitivity analysis

#ifndef NH_SENSITIVITY_ANALYZER_H
#define NH_SENSITIVITY_ANALYZER_H

#include <nhssta/ssta_results.hpp>
#include "circuit_graph.hpp"

namespace Nh {

class SensitivityAnalyzer {
public:
    explicit SensitivityAnalyzer(const CircuitGraph* graph);
    [[nodiscard]] SensitivityResults analyze(size_t top_n) const;
    
private:
    [[nodiscard]] std::vector<SensitivityPath> collect_endpoint_paths() const;
    void build_objective_function(const std::vector<SensitivityPath>& endpoint_paths, SensitivityResults& results) const;
    void collect_gate_sensitivities(SensitivityResults& results) const;
    
    const CircuitGraph* graph_;
    static constexpr double GRADIENT_THRESHOLD = 1e-10;
    static constexpr double MIN_VARIANCE = 1e-10;
};

}  // namespace Nh

#endif  // NH_SENSITIVITY_ANALYZER_H

