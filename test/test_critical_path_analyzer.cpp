// -*- c++ -*-
// Unit tests for CriticalPathAnalyzer class
// Phase 3: Critical path analysis separation

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>

#include "../src/critical_path_analyzer.hpp"
#include "../src/circuit_graph.hpp"
#include "../src/dlib_parser.hpp"
#include "../src/bench_parser.hpp"
#include <nhssta/ssta_results.hpp>

using Nh::CriticalPathAnalyzer;
using Nh::CircuitGraph;
using Nh::DlibParser;
using Nh::BenchParser;
using Nh::CriticalPaths;

class CriticalPathAnalyzerTest : public ::testing::Test {
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
        dlib_path = createTestFile("test_cpa.dlib", dlib_content);
    }

    void TearDown() override {
        deleteTestFile("test_cpa.dlib");
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

// Test: CriticalPathAnalyzer with simple circuit
TEST_F(CriticalPathAnalyzerTest, AnalyzeSimpleCircuit) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_simple_cpa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze critical paths
    CriticalPathAnalyzer analyzer(graph);
    CriticalPaths paths = analyzer.analyze(5);
    
    EXPECT_EQ(paths.size(), 1);
    EXPECT_EQ(paths[0].node_names.size(), 2);  // A -> Y
    EXPECT_EQ(paths[0].node_names[0], "A");
    EXPECT_EQ(paths[0].node_names[1], "Y");
    EXPECT_GT(paths[0].delay_mean, 0.0);
    
    deleteTestFile("test_simple_cpa.bench");
}

// Test: CriticalPathAnalyzer with multiple paths
TEST_F(CriticalPathAnalyzerTest, AnalyzeMultiplePaths) {
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
        "N1 = INV(A)\n"
        "Y1 = INV(N1)\n"
        "Y2 = INV(B)\n";
    std::string bench_path = createTestFile("test_multiple_cpa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze critical paths
    CriticalPathAnalyzer analyzer(graph);
    CriticalPaths paths = analyzer.analyze(5);
    
    EXPECT_GE(paths.size(), 2);
    // Paths should be sorted by delay (descending)
    if (paths.size() >= 2) {
        EXPECT_GE(paths[0].delay_mean, paths[1].delay_mean);
    }
    
    deleteTestFile("test_multiple_cpa.bench");
}

// Test: CriticalPathAnalyzer with top_n limit
TEST_F(CriticalPathAnalyzerTest, AnalyzeWithTopN) {
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
    std::string bench_path = createTestFile("test_topn_cpa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze critical paths with top_n = 2
    CriticalPathAnalyzer analyzer(graph);
    CriticalPaths paths = analyzer.analyze(2);
    
    EXPECT_LE(paths.size(), 2);
    
    deleteTestFile("test_topn_cpa.bench");
}

// Test: CriticalPathAnalyzer with zero top_n
TEST_F(CriticalPathAnalyzerTest, AnalyzeWithZeroTopN) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_zero_cpa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze critical paths with top_n = 0
    CriticalPathAnalyzer analyzer(graph);
    CriticalPaths paths = analyzer.analyze(0);
    
    EXPECT_EQ(paths.size(), 0);
    
    deleteTestFile("test_zero_cpa.bench");
}

// Test: CriticalPathAnalyzer with complex path
TEST_F(CriticalPathAnalyzerTest, AnalyzeComplexPath) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with longer path
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "N1 = INV(A)\n"
        "N2 = INV(N1)\n"
        "Y = INV(N2)\n";
    std::string bench_path = createTestFile("test_complex_cpa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze critical paths
    CriticalPathAnalyzer analyzer(graph);
    CriticalPaths paths = analyzer.analyze(5);
    
    EXPECT_EQ(paths.size(), 1);
    EXPECT_GE(paths[0].node_names.size(), 3);  // A -> N1 -> N2 -> Y
    EXPECT_EQ(paths[0].node_names[0], "A");
    EXPECT_EQ(paths[0].node_names.back(), "Y");
    
    deleteTestFile("test_complex_cpa.bench");
}

// Test: CriticalPathAnalyzer path ordering
TEST_F(CriticalPathAnalyzerTest, PathOrdering) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with paths of different delays
    std::string bench_content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y1)\n"
        "OUTPUT(Y2)\n"
        "Y1 = INV(A)\n"  // Shorter path (1 gate)
        "N1 = INV(B)\n"
        "Y2 = INV(N1)\n";  // Longer path (2 gates, should have higher delay)
    std::string bench_path = createTestFile("test_ordering_cpa.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Analyze critical paths
    CriticalPathAnalyzer analyzer(graph);
    CriticalPaths paths = analyzer.analyze(5);
    
    EXPECT_GE(paths.size(), 2);
    // Paths should be sorted by delay (descending)
    // Longer path (Y2) should have higher delay and come first
    if (paths.size() >= 2) {
        EXPECT_GE(paths[0].delay_mean, paths[1].delay_mean);
    }
    
    deleteTestFile("test_ordering_cpa.bench");
}

