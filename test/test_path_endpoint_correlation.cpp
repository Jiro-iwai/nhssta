// -*- c++ -*-
// Unit tests for critical path endpoint correlation matrix
// Feature: Display correlation matrix for start/end nodes of critical paths

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cmath>
#include <fstream>
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>
#include <set>
#include <sstream>

#include "test_path_helper.h"

using namespace Nh;

class PathEndpointCorrelationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        ssta_ = new Ssta();
        example_dir = find_example_dir();
        if (example_dir.empty()) {
            example_dir = "../example";
        }

        test_dir = "../test/test_data";
        struct stat info;
        if (stat(test_dir.c_str(), &info) != 0) {
            std::string cmd = "mkdir -p " + test_dir;
            system(cmd.c_str());
        }
    }

    void TearDown() override {
        delete ssta_;
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

    Ssta* ssta_;
    std::string example_dir;
    std::string test_dir;
};

// Test: getPathEndpointCorrelationMatrix method exists
TEST_F(PathEndpointCorrelationTest, MethodExists) {
    // Set up a simple circuit
    std::string dlib_content = "gate1 0 y gauss(10.0, 1.0)\n";
    std::string bench_content = "INPUT(A)\nOUTPUT(Y)\nY = gate1(A)\n";

    std::string dlib_file = createTestFile("ep_corr_simple.dlib", dlib_content);
    std::string bench_file = createTestFile("ep_corr_simple.bench", bench_content);

    ssta_->set_dlib(dlib_file);
    ssta_->set_bench(bench_file);
    ssta_->set_critical_path(1);
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(1);

    // This should compile and return a CorrelationMatrix
    CorrelationMatrix matrix = ssta_->getPathEndpointCorrelationMatrix(paths);

    // Basic verification
    EXPECT_FALSE(matrix.node_names.empty());

    deleteTestFile("ep_corr_simple.dlib");
    deleteTestFile("ep_corr_simple.bench");
}

// Test: Endpoint matrix contains only start and end nodes
TEST_F(PathEndpointCorrelationTest, ContainsOnlyEndpoints) {
    // Circuit: A -> gate1 -> N1 -> gate2 -> Y
    // Start node: A, End node: Y, Intermediate: N1
    std::string dlib_content =
        "gate1 0 y gauss(10.0, 1.0)\n"
        "gate2 0 y gauss(5.0, 0.5)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "N1 = gate1(A)\n"
        "Y = gate2(N1)\n";

    std::string dlib_file = createTestFile("ep_corr_chain.dlib", dlib_content);
    std::string bench_file = createTestFile("ep_corr_chain.bench", bench_content);

    ssta_->set_dlib(dlib_file);
    ssta_->set_bench(bench_file);
    ssta_->set_critical_path(1);
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(1);
    CorrelationMatrix matrix = ssta_->getPathEndpointCorrelationMatrix(paths);

    // Should contain A (start) and Y (end), but not N1 (intermediate)
    std::set<std::string> node_set(matrix.node_names.begin(), matrix.node_names.end());
    EXPECT_TRUE(node_set.find("A") != node_set.end()) << "Start node A should be included";
    EXPECT_TRUE(node_set.find("Y") != node_set.end()) << "End node Y should be included";
    EXPECT_TRUE(node_set.find("N1") == node_set.end()) << "Intermediate node N1 should NOT be included";

    deleteTestFile("ep_corr_chain.dlib");
    deleteTestFile("ep_corr_chain.bench");
}

// Test: Multiple paths share endpoints
TEST_F(PathEndpointCorrelationTest, MultiplePathsSharedEndpoints) {
    // Two paths: A -> N1 -> Y and B -> N2 -> Y (Y is shared endpoint)
    std::string dlib_content =
        "gate1 0 y gauss(10.0, 1.0)\n"
        "gate2 0 y gauss(5.0, 0.5)\n"
        "gate2 1 y gauss(6.0, 0.6)\n";  // Two-input gate with both pins
    std::string bench_content =
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y)\n"
        "N1 = gate1(A)\n"
        "N2 = gate1(B)\n"
        "Y = gate2(N1, N2)\n";

    std::string dlib_file = createTestFile("ep_corr_multi.dlib", dlib_content);
    std::string bench_file = createTestFile("ep_corr_multi.bench", bench_content);

    ssta_->set_dlib(dlib_file);
    ssta_->set_bench(bench_file);
    ssta_->set_critical_path(3);
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(3);
    CorrelationMatrix matrix = ssta_->getPathEndpointCorrelationMatrix(paths);

    // Unique endpoints should be collected (no duplicates)
    std::set<std::string> node_set(matrix.node_names.begin(), matrix.node_names.end());
    EXPECT_EQ(node_set.size(), matrix.node_names.size()) << "No duplicate nodes";

    deleteTestFile("ep_corr_multi.dlib");
    deleteTestFile("ep_corr_multi.bench");
}

// Test: Self-correlation is 1.0
TEST_F(PathEndpointCorrelationTest, SelfCorrelationIsOne) {
    std::string dlib_content = "gate1 0 y gauss(10.0, 1.0)\n";
    std::string bench_content = "INPUT(A)\nOUTPUT(Y)\nY = gate1(A)\n";

    std::string dlib_file = createTestFile("ep_corr_self.dlib", dlib_content);
    std::string bench_file = createTestFile("ep_corr_self.bench", bench_content);

    ssta_->set_dlib(dlib_file);
    ssta_->set_bench(bench_file);
    ssta_->set_critical_path(1);
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(1);
    CorrelationMatrix matrix = ssta_->getPathEndpointCorrelationMatrix(paths);

    // Self-correlation should be 1.0
    for (const auto& node : matrix.node_names) {
        double corr = matrix.getCorrelation(node, node);
        EXPECT_NEAR(corr, 1.0, 0.001) << "Self-correlation of " << node << " should be 1.0";
    }

    deleteTestFile("ep_corr_self.dlib");
    deleteTestFile("ep_corr_self.bench");
}

// Test: Empty paths returns empty matrix
TEST_F(PathEndpointCorrelationTest, EmptyPathsReturnsEmptyMatrix) {
    CriticalPaths empty_paths;
    CorrelationMatrix matrix = ssta_->getPathEndpointCorrelationMatrix(empty_paths);

    EXPECT_TRUE(matrix.node_names.empty());
    EXPECT_TRUE(matrix.correlations.empty());
}

// Test: Correlation values match existing getCorrelationMatrix
TEST_F(PathEndpointCorrelationTest, CorrelationValuesMatchExisting) {
    std::string dlib_content = "gate1 0 y gauss(10.0, 1.0)\n";
    std::string bench_content = "INPUT(A)\nOUTPUT(Y)\nY = gate1(A)\n";

    std::string dlib_file = createTestFile("ep_corr_match.dlib", dlib_content);
    std::string bench_file = createTestFile("ep_corr_match.bench", bench_content);

    ssta_->set_dlib(dlib_file);
    ssta_->set_bench(bench_file);
    ssta_->set_lat();
    ssta_->set_correlation();
    ssta_->set_critical_path(1);
    ssta_->read_dlib();
    ssta_->read_bench();

    // Get full correlation matrix
    CorrelationMatrix full_matrix = ssta_->getCorrelationMatrix();

    // Get endpoint correlation matrix
    CriticalPaths paths = ssta_->getCriticalPaths(1);
    CorrelationMatrix endpoint_matrix = ssta_->getPathEndpointCorrelationMatrix(paths);

    // Correlation between A and Y should be the same in both matrices
    double full_corr = full_matrix.getCorrelation("A", "Y");
    double endpoint_corr = endpoint_matrix.getCorrelation("A", "Y");
    EXPECT_NEAR(full_corr, endpoint_corr, 0.001)
        << "Correlation should match between full and endpoint matrices";

    deleteTestFile("ep_corr_match.dlib");
    deleteTestFile("ep_corr_match.bench");
}

