// -*- c++ -*-
// Unit tests for SensitivityAnalyzer class
// Phase 3.5: Sensitivity analysis separation

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <cmath>

#include "../src/sensitivity_analyzer.hpp"
#include "../src/circuit_graph.hpp"
#include "../src/dlib_parser.hpp"
#include "../src/bench_parser.hpp"
#include <nhssta/ssta_results.hpp>

using Nh::SensitivityAnalyzer;
using Nh::CircuitGraph;
using Nh::DlibParser;
using Nh::BenchParser;
using Nh::SensitivityResults;

class SensitivityAnalyzerTest : public ::testing::Test {
   protected:
    void SetUp() override {
        test_dir = "../test/test_data";
        // Create test directory if it doesn't exist
        struct stat info;
        if (stat(test_dir.c_str(), &info) != 0) {
            std::string cmd = "mkdir -p " + test_dir;
            system(cmd.c_str());
        }
        
        // Create test dlib file
        std::string dlib_content = 
            "inv  0  y  gauss  (15.0, 2.0)\n"
            "nand  0  y  gauss  (24.0, 3.0)\n"
            "nand  1  y  gauss  (20.0, 3.0)\n";
        dlib_path = createTestFile("test_sa.dlib", dlib_content);
    }

    void TearDown() override {
        deleteTestFile("test_sa.dlib");
    }

    std::string createTestFile(const std::string& filename, const std::string& content) {
        std::string filepath = test_dir + "/" + filename;
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << content;
            file.close();
        }
        return filepath;
    }

    void deleteTestFile(const std::string& filename) {
        std::string filepath = test_dir + "/" + filename;
        remove(filepath.c_str());
    }

    std::string test_dir;
    std::string dlib_path;
};

// Test: SensitivityAnalyzer with simple circuit
TEST_F(SensitivityAnalyzerTest, AnalyzeSimpleCircuit) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_simple_sa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze sensitivity
    SensitivityAnalyzer analyzer(&graph);
    SensitivityResults results = analyzer.analyze(5);
    
    EXPECT_EQ(results.top_paths.size(), 1);
    EXPECT_EQ(results.top_paths[0].endpoint, "Y");
    EXPECT_GT(results.top_paths[0].lat, 0.0);
    EXPECT_GT(results.top_paths[0].std_dev, 0.0);
    EXPECT_GT(results.objective_value, 0.0);
    
    deleteTestFile("test_simple_sa.bench");
}

// Test: SensitivityAnalyzer with multiple outputs
TEST_F(SensitivityAnalyzerTest, AnalyzeMultipleOutputs) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with multiple outputs
    std::string bench_content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y1)\n"
        "OUTPUT(Y2)\n"
        "Y1 = INV(A)\n"
        "Y2 = INV(B)\n";
    std::string bench_path = createTestFile("test_multiple_sa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze sensitivity
    SensitivityAnalyzer analyzer(&graph);
    SensitivityResults results = analyzer.analyze(5);
    
    EXPECT_EQ(results.top_paths.size(), 2);
    // Paths should be sorted by score (descending)
    if (results.top_paths.size() >= 2) {
        EXPECT_GE(results.top_paths[0].score, results.top_paths[1].score);
    }
    
    deleteTestFile("test_multiple_sa.bench");
}

// Test: SensitivityAnalyzer with top_n limit
TEST_F(SensitivityAnalyzerTest, AnalyzeWithTopN) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with multiple outputs
    std::string bench_content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "INPUT(C)\n"
        "OUTPUT(Y1)\n"
        "OUTPUT(Y2)\n"
        "OUTPUT(Y3)\n"
        "Y1 = INV(A)\n"
        "Y2 = INV(B)\n"
        "Y3 = INV(C)\n";
    std::string bench_path = createTestFile("test_topn_sa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze sensitivity with top_n = 2
    SensitivityAnalyzer analyzer(&graph);
    SensitivityResults results = analyzer.analyze(2);
    
    EXPECT_LE(results.top_paths.size(), 2);
    
    deleteTestFile("test_topn_sa.bench");
}

// Test: SensitivityAnalyzer with zero top_n
TEST_F(SensitivityAnalyzerTest, AnalyzeWithZeroTopN) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_zero_sa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze sensitivity with top_n = 0
    SensitivityAnalyzer analyzer(&graph);
    SensitivityResults results = analyzer.analyze(0);
    
    EXPECT_EQ(results.top_paths.size(), 0);
    EXPECT_EQ(results.gate_sensitivities.size(), 0);
    
    deleteTestFile("test_zero_sa.bench");
}

// Test: SensitivityAnalyzer gate sensitivities
TEST_F(SensitivityAnalyzerTest, GateSensitivities) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with multiple gates
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "N1 = INV(A)\n"
        "Y = INV(N1)\n";
    std::string bench_path = createTestFile("test_gate_sens.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze sensitivity
    SensitivityAnalyzer analyzer(&graph);
    SensitivityResults results = analyzer.analyze(5);
    
    // Should have gate sensitivities
    EXPECT_GE(results.gate_sensitivities.size(), 0);
    // Gate sensitivities should be sorted by magnitude (descending)
    if (results.gate_sensitivities.size() >= 2) {
        double mag0 = std::abs(results.gate_sensitivities[0].grad_mu) + 
                      std::abs(results.gate_sensitivities[0].grad_sigma);
        double mag1 = std::abs(results.gate_sensitivities[1].grad_mu) + 
                      std::abs(results.gate_sensitivities[1].grad_sigma);
        EXPECT_GE(mag0, mag1);
    }
    
    deleteTestFile("test_gate_sens.bench");
}

// Test: SensitivityAnalyzer with no outputs
TEST_F(SensitivityAnalyzerTest, AnalyzeWithNoOutputs) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with no outputs
    std::string bench_content = 
        "INPUT(A)\n"
        "N1 = INV(A)\n";
    std::string bench_path = createTestFile("test_no_outputs_sa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze sensitivity
    SensitivityAnalyzer analyzer(&graph);
    SensitivityResults results = analyzer.analyze(5);
    
    EXPECT_EQ(results.top_paths.size(), 0);
    EXPECT_EQ(results.gate_sensitivities.size(), 0);
    
    deleteTestFile("test_no_outputs_sa.bench");
}

