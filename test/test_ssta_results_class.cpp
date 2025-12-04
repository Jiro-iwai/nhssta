// -*- c++ -*-
// Unit tests for SstaResults class
// Phase 4: Results data generation separation

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <cmath>

#include "../src/ssta_results.hpp"
#include "../src/circuit_graph.hpp"
#include "../src/dlib_parser.hpp"
#include "../src/bench_parser.hpp"
#include <nhssta/ssta_results.hpp>

using Nh::SstaResults;
using Nh::CircuitGraph;
using Nh::DlibParser;
using Nh::BenchParser;
using Nh::LatResults;
using Nh::CorrelationMatrix;

class SstaResultsTest : public ::testing::Test {
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
        dlib_path = createTestFile("test_results.dlib", dlib_content);
    }

    void TearDown() override {
        deleteTestFile("test_results.dlib");
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

// Test: SstaResults getLatResults with simple circuit
TEST_F(SstaResultsTest, GetLatResultsSimple) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_lat.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Get LAT results
    SstaResults results(&graph);
    LatResults lat_results = results.getLatResults();
    
    EXPECT_EQ(lat_results.size(), 2);  // A and Y
    // Results should be sorted by signal name
    EXPECT_EQ(lat_results[0].node_name, "A");
    EXPECT_EQ(lat_results[1].node_name, "Y");
    EXPECT_GT(lat_results[1].mean, 0.0);
    
    deleteTestFile("test_lat.bench");
}

// Test: SstaResults getLatResults with multiple signals
TEST_F(SstaResultsTest, GetLatResultsMultiple) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y)\n"
        "N1 = INV(A)\n"
        "Y = INV(N1)\n";
    std::string bench_path = createTestFile("test_lat_multiple.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Get LAT results
    SstaResults results(&graph);
    LatResults lat_results = results.getLatResults();
    
    EXPECT_EQ(lat_results.size(), 4);  // A, B, N1, Y
    // Results should be sorted alphabetically
    EXPECT_EQ(lat_results[0].node_name, "A");
    EXPECT_EQ(lat_results[1].node_name, "B");
    EXPECT_EQ(lat_results[2].node_name, "N1");
    EXPECT_EQ(lat_results[3].node_name, "Y");
    
    deleteTestFile("test_lat_multiple.bench");
}

// Test: SstaResults getCorrelationMatrix with simple circuit
TEST_F(SstaResultsTest, GetCorrelationMatrixSimple) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_corr.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Get correlation matrix
    SstaResults results(&graph);
    CorrelationMatrix matrix = results.getCorrelationMatrix();
    
    EXPECT_EQ(matrix.node_names.size(), 2);  // A and Y
    EXPECT_EQ(matrix.node_names[0], "A");
    EXPECT_EQ(matrix.node_names[1], "Y");
    
    // Check self-correlation (should be 1.0)
    EXPECT_DOUBLE_EQ(matrix.getCorrelation("A", "A"), 1.0);
    EXPECT_DOUBLE_EQ(matrix.getCorrelation("Y", "Y"), 1.0);
    
    deleteTestFile("test_corr.bench");
}

// Test: SstaResults getCorrelationMatrix with multiple signals
TEST_F(SstaResultsTest, GetCorrelationMatrixMultiple) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y)\n"
        "N1 = INV(A)\n"
        "Y = INV(N1)\n";
    std::string bench_path = createTestFile("test_corr_multiple.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Get correlation matrix
    SstaResults results(&graph);
    CorrelationMatrix matrix = results.getCorrelationMatrix();
    
    EXPECT_EQ(matrix.node_names.size(), 4);  // A, B, N1, Y
    
    // Check correlations are calculated
    double corr_ay = matrix.getCorrelation("A", "Y");
    EXPECT_GE(corr_ay, 0.0);
    EXPECT_LE(corr_ay, 1.0);
    
    deleteTestFile("test_corr_multiple.bench");
}

// Test: SstaResults getCorrelationMatrix symmetry
TEST_F(SstaResultsTest, GetCorrelationMatrixSymmetry) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_corr_symmetry.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Get correlation matrix
    SstaResults results(&graph);
    CorrelationMatrix matrix = results.getCorrelationMatrix();
    
    // Correlation should be symmetric
    double corr_ab = matrix.getCorrelation("A", "B");
    double corr_ba = matrix.getCorrelation("B", "A");
    EXPECT_DOUBLE_EQ(corr_ab, corr_ba);
    
    deleteTestFile("test_corr_symmetry.bench");
}

// Test: SstaResults getLatResults with empty circuit
TEST_F(SstaResultsTest, GetLatResultsEmpty) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with only inputs (no gates)
    std::string bench_content = 
        "INPUT(A)\n"
        "INPUT(B)\n";
    std::string bench_path = createTestFile("test_lat_empty.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse();
    
    // Build circuit graph
    CircuitGraph graph;
    graph.set_bench_file(bench_path);
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    // Get LAT results
    SstaResults results(&graph);
    LatResults lat_results = results.getLatResults();
    
    EXPECT_EQ(lat_results.size(), 2);  // A and B (inputs)
    
    deleteTestFile("test_lat_empty.bench");
}
