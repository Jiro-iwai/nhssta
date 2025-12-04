// -*- c++ -*-
// Authors: IWAI Jiro

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "add.hpp"
#include "covariance.hpp"
#include "expression.hpp"
#include "parser.hpp"
#include "util_numerical.hpp"
#include "dlib_parser.hpp"
#include "bench_parser.hpp"
#include "circuit_graph.hpp"
#include "critical_path_analyzer.hpp"
#include "sensitivity_analyzer.hpp"
#include "ssta_results.hpp"

namespace Nh {

// DFF gate name constant
static const std::string DFF_GATE_NAME = "dff";

// DFF clock input is treated as arriving at time 0 (start of clock cycle)
static constexpr double DFF_CLOCK_ARRIVAL_TIME = 0.0;

// Threshold for gradient values to consider non-zero
// Used to filter out negligible gradients in sensitivity analysis
static constexpr double GRADIENT_THRESHOLD = 1e-10;

Ssta::Ssta() = default;

Ssta::~Ssta() {
    // Clear static caches to prevent crash during static destruction
    // The cov_expr_cache contains Expression objects (shared_ptr to ExpressionImpl)
    // If destroyed after eTbl_ (static in expression.cpp), the ExpressionImpl
    // destructors will crash when trying to erase from an already-destroyed set
    ::RandomVariable::clear_cov_expr_cache();
    // Unique pointers will automatically clean up refactored components
}

void Ssta::check() {
    // Validate configuration and throw exception if invalid
    // Error messages should be handled by CLI layer

    std::ostringstream error_msg;

    if (dlib_.empty()) {
        if (error_msg.tellp() > 0) {
            error_msg << "; ";
        }
        error_msg << "please specify `-d' properly";
    }

    if (bench_.empty()) {
        if (error_msg.tellp() > 0) {
            error_msg << "; ";
        }
        error_msg << "please specify `-b' properly";
    }

    std::string error_str = error_msg.str();
    if (!error_str.empty()) {
        throw Nh::ConfigurationException(error_str);
    }
}

// dlib //

void Ssta::read_dlib() {
    // Use DlibParser (Phase 5: refactoring)
    dlib_parser_ = std::make_unique<DlibParser>(dlib_);
    dlib_parser_->parse();
    gates_ = dlib_parser_->gates();
}

// bench //

void Ssta::read_bench() {
    // Use BenchParser and CircuitGraph (Phase 5: refactoring)
    bench_parser_ = std::make_unique<BenchParser>(bench_);
    bench_parser_->parse();
    
    // Build circuit graph with track_path callback if needed
    circuit_graph_ = std::make_unique<CircuitGraph>();
    circuit_graph_->set_bench_file(bench_);
    
    // Create track_path callback if critical path or sensitivity analysis is enabled
    CircuitGraph::TrackPathCallback track_callback = nullptr;
    if (is_critical_path_ || is_sensitivity_) {
        // This callback is handled by CircuitGraph internally
        // No additional action needed here
    }
    
    circuit_graph_->build(gates_, bench_parser_->net(),
                          bench_parser_->inputs(), bench_parser_->outputs(),
                          bench_parser_->dff_outputs(), bench_parser_->dff_inputs(),
                          track_callback);
    
    // Create analyzers and results generator
    if (is_critical_path_) {
        path_analyzer_ = std::make_unique<CriticalPathAnalyzer>(circuit_graph_.get());
    }
    if (is_sensitivity_) {
        sensitivity_analyzer_ = std::make_unique<SensitivityAnalyzer>(circuit_graph_.get());
    }
    results_ = std::make_unique<SstaResults>(circuit_graph_.get());
    
    // Clear gates_ to free memory, unless sensitivity analysis is enabled
    // (sensitivity analysis needs the gate delays for gradient computation)
    if (!is_sensitivity_) {
        gates_.clear();
    }
}

// bench //

// Pure logic functions (Phase 2: I/O separation)

LatResults Ssta::getLatResults() const {
    // Use SstaResults (Phase 5: refactoring)
    if (!results_) {
        return LatResults();  // Return empty results if read_bench() hasn't been called
    }
    return results_->getLatResults();
}

CorrelationMatrix Ssta::getCorrelationMatrix() const {
    // Use SstaResults (Phase 5: refactoring)
    if (!results_) {
        return CorrelationMatrix();  // Return empty matrix if read_bench() hasn't been called
    }
    return results_->getCorrelationMatrix();
}

// Old methods removed - functionality moved to CircuitGraph (Phase 5)

CriticalPaths Ssta::getCriticalPaths(size_t top_n) const {
    // Use CriticalPathAnalyzer (Phase 5: refactoring)
    if (!is_critical_path_) {
        return CriticalPaths();
    }
    if (!path_analyzer_) {
        throw Nh::RuntimeException("read_bench() must be called before getCriticalPaths()");
    }
    return path_analyzer_->analyze(top_n);
}

SensitivityResults Ssta::getSensitivityResults(size_t top_n) const {
    // Use SensitivityAnalyzer (Phase 5: refactoring)
    if (!is_sensitivity_) {
        return SensitivityResults();
    }
    if (!sensitivity_analyzer_) {
        throw Nh::RuntimeException("read_bench() must be called before getSensitivityResults()");
    }
    return sensitivity_analyzer_->analyze(top_n);
}

}  // namespace Nh
