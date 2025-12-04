// -*- c++ -*-
// Authors: IWAI Jiro
// SensitivityAnalyzer: Sensitivity analysis

#include "sensitivity_analyzer.hpp"
#include "circuit_graph.hpp"
#include "expression.hpp"
#include <nhssta/ssta_results.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace Nh {

SensitivityAnalyzer::SensitivityAnalyzer(const CircuitGraph* graph)
    : graph_(graph) {
}

SensitivityResults SensitivityAnalyzer::analyze(size_t top_n) const {
    using namespace RandomVariable;
    
    SensitivityResults results;
    
    // Step 1: Collect output signals for objective function
    std::vector<SensitivityPath> endpoint_paths = collect_endpoint_paths();
    
    // Step 2: Sort by score (LAT + σ) descending
    std::sort(endpoint_paths.begin(), endpoint_paths.end(),
              [](const SensitivityPath& a, const SensitivityPath& b) {
                  return a.score > b.score;
              });
    
    // Step 3: Select top N endpoints for objective function
    if (endpoint_paths.size() > top_n) {
        endpoint_paths.resize(top_n);
    }
    results.top_paths = endpoint_paths;
    
    if (endpoint_paths.empty()) {
        return results;
    }
    
    // Step 4: Build objective function F = log(Σ exp(LAT + σ))
    build_objective_function(endpoint_paths, results);
    
    // Step 5: Collect gate sensitivities
    collect_gate_sensitivities(results);
    
    return results;
}

std::vector<SensitivityPath> SensitivityAnalyzer::collect_endpoint_paths() const {
    std::vector<SensitivityPath> endpoint_paths;
    const auto& outputs = graph_->outputs();
    const auto& signals = graph_->signals();
    
    // Note: DFF D terminals are not included due to Expression tree complexity
    for (const auto& endpoint : outputs) {
        auto sig_it = signals.find(endpoint);
        if (sig_it == signals.end()) {
            continue;
        }
        const RandomVariable& lat = sig_it->second;
        double mean = lat->mean();
        double stddev = std::sqrt(lat->variance());
        endpoint_paths.emplace_back(endpoint, mean, stddev);
    }
    
    return endpoint_paths;
}

void SensitivityAnalyzer::build_objective_function(const std::vector<SensitivityPath>& endpoint_paths, SensitivityResults& results) const {
    using namespace RandomVariable;
    
    const auto& signals = graph_->signals();
    
    // First, clear all gradients
    zero_all_grad();
    
    // Build Expression for each endpoint's score
    Expression sum_exp = Const(0.0);
    for (const auto& path : endpoint_paths) {
        auto sig_it = signals.find(path.endpoint);
        if (sig_it == signals.end()) {
            continue;
        }
        const RandomVariable& lat = sig_it->second;
        
        Expression mean_expr = lat->mean_expr();
        Expression std_expr = lat->std_expr();
        Expression score_expr = mean_expr + std_expr;
        sum_exp = sum_exp + exp(score_expr);
    }
    
    Expression objective = log(sum_exp);
    results.objective_value = objective->value();
    
    // Compute gradients via backward pass
    objective->backward();
}

void SensitivityAnalyzer::collect_gate_sensitivities(SensitivityResults& results) const {
    const auto& signal_to_instance = graph_->signal_to_instance();
    const auto& instance_to_inputs = graph_->instance_to_inputs();
    const auto& instance_to_gate_type = graph_->instance_to_gate_type();
    const auto& instance_to_delays = graph_->instance_to_delays();
    
    // Build reverse mapping: instance name -> output signal name
    std::unordered_map<std::string, std::string> instance_to_output;
    for (const auto& sig_inst : signal_to_instance) {
        instance_to_output[sig_inst.second] = sig_inst.first;
    }
    
    for (const auto& inst_pair : instance_to_delays) {
        const std::string& instance_name = inst_pair.first;
        const auto& delays = inst_pair.second;
        
        // Get output node name for this instance
        std::string output_node;
        auto out_it = instance_to_output.find(instance_name);
        if (out_it != instance_to_output.end()) {
            output_node = out_it->second;
        }
        
        // Get gate type for this instance
        std::string gate_type;
        auto type_it = instance_to_gate_type.find(instance_name);
        if (type_it != instance_to_gate_type.end()) {
            gate_type = type_it->second;
        }
        
        // Get input signal names for this instance
        std::vector<std::string> input_signals;
        auto inputs_it = instance_to_inputs.find(instance_name);
        if (inputs_it != instance_to_inputs.end()) {
            input_signals = inputs_it->second;
        }
        
        for (const auto& delay_pair : delays) {
            const std::string& pin_name = delay_pair.first;
            const Normal& delay = delay_pair.second;
            
            // Skip const delays (σ ≈ 0) to avoid numerical issues
            // See Issue #184 for future improvement
            if (delay->variance() < MIN_VARIANCE) {
                continue;
            }
            
            // Map pin number to input signal name
            std::string input_signal = pin_name;  // fallback to pin name
            try {
                size_t pin_idx = std::stoul(pin_name);
                if (pin_idx < input_signals.size()) {
                    input_signal = input_signals[pin_idx];
                }
            } catch (const std::invalid_argument& e) {
                // Keep pin_name if not a number (this is acceptable for non-numeric pin names)
            } catch (const std::out_of_range& e) {
                // Pin index out of range - this is an error condition
                throw Nh::RuntimeException("Pin index out of range: " + pin_name);
            }
            
            // Get gradients from the cloned Expression
            double grad_mu = delay->mean_expr()->gradient();
            double grad_sigma = delay->std_expr()->gradient();
            
            // Only include gates with non-zero gradients
            if (std::abs(grad_mu) > GRADIENT_THRESHOLD || std::abs(grad_sigma) > GRADIENT_THRESHOLD) {
                results.gate_sensitivities.emplace_back(
                    instance_name, output_node, input_signal, gate_type,
                    grad_mu, grad_sigma);
            }
        }
    }
    
    // Sort by absolute gradient magnitude (descending)
    std::sort(results.gate_sensitivities.begin(), results.gate_sensitivities.end(),
              [](const GateSensitivity& a, const GateSensitivity& b) {
                  double mag_a = std::abs(a.grad_mu) + std::abs(a.grad_sigma);
                  double mag_b = std::abs(b.grad_mu) + std::abs(b.grad_sigma);
                  return mag_a > mag_b;
              });
}

}  // namespace Nh

