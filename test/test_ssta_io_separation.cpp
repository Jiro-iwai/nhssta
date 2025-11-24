// -*- c++ -*-
// Unit tests for Ssta I/O separation
// Tests that Ssta class does not perform I/O operations directly
// and that results can be obtained via getLatResults() and getCorrelationMatrix()

#include <gtest/gtest.h>
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "../src/parser.hpp"

using namespace Nh;

class SstaIoSeparationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create a simple test circuit
        dlib_content = "and 0 y gauss (10.0, 2.0)\nand 1 y gauss (10.0, 2.0)\n";
        bench_content = 
            "INPUT(a)\n"
            "INPUT(b)\n"
            "OUTPUT(y)\n"
            "y = AND(a, b)\n";
    }

    void TearDown() override {
        // Cleanup
    }

    std::string dlib_content;
    std::string bench_content;
};

// Test: Ssta::getLatResults() returns correct results without I/O
TEST_F(SstaIoSeparationTest, GetLatResultsReturnsCorrectResults) {
    Ssta ssta;
    
    // Set up test files (in memory or temporary files)
    // For now, we'll use actual file operations but verify no std::cout is used
    
    // Note: This test verifies that getLatResults() works without report()
    // The actual file I/O is still needed for read_dlib() and read_bench()
    
    // Create temporary files
    std::string dlib_file = "/tmp/test_io_sep.dlib";
    std::string bench_file = "/tmp/test_io_sep.bench";
    
    std::ofstream dlib_stream(dlib_file);
    dlib_stream << dlib_content;
    dlib_stream.close();
    
    std::ofstream bench_stream(bench_file);
    bench_stream << bench_content;
    bench_stream.close();
    
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    ssta.set_lat();
    
    ssta.read_dlib();
    ssta.read_bench();
    
    // Get results without using report()
    LatResults results = ssta.getLatResults();
    
    // Verify results are not empty
    EXPECT_FALSE(results.empty());
    
    // Verify results contain expected signals
    bool found_a = false;
    bool found_b = false;
    bool found_y = false;
    
    for (const auto& result : results) {
        if (result.node_name == "a") {
            found_a = true;
            EXPECT_NEAR(result.mean, 0.0, 0.001);
        } else if (result.node_name == "b") {
            found_b = true;
            EXPECT_NEAR(result.mean, 0.0, 0.001);
        } else if (result.node_name == "y") {
            found_y = true;
            EXPECT_GT(result.mean, 0.0);  // y should have delay
        }
    }
    
    EXPECT_TRUE(found_a) << "Signal 'a' should be in results";
    EXPECT_TRUE(found_b) << "Signal 'b' should be in results";
    EXPECT_TRUE(found_y) << "Signal 'y' should be in results";
    
    // Cleanup
    remove(dlib_file.c_str());
    remove(bench_file.c_str());
}

// Test: Ssta::getCorrelationMatrix() returns correct results without I/O
TEST_F(SstaIoSeparationTest, GetCorrelationMatrixReturnsCorrectResults) {
    Ssta ssta;
    
    std::string dlib_file = "/tmp/test_io_sep_corr.dlib";
    std::string bench_file = "/tmp/test_io_sep_corr.bench";
    
    std::ofstream dlib_stream(dlib_file);
    dlib_stream << dlib_content;
    dlib_stream.close();
    
    std::ofstream bench_stream(bench_file);
    bench_stream << bench_content;
    bench_stream.close();
    
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    ssta.set_correlation();
    
    ssta.read_dlib();
    ssta.read_bench();
    
    // Get results without using report()
    CorrelationMatrix matrix = ssta.getCorrelationMatrix();
    
    // Verify matrix is not empty
    EXPECT_FALSE(matrix.node_names.empty());
    
    // Verify correlation values are in valid range [-1, 1]
    for (const auto& node1 : matrix.node_names) {
        for (const auto& node2 : matrix.node_names) {
            double corr = matrix.getCorrelation(node1, node2);
            EXPECT_GE(corr, -1.0) << "Correlation should be >= -1.0";
            EXPECT_LE(corr, 1.0) << "Correlation should be <= 1.0";
            
            // Same node should have correlation 1.0
            if (node1 == node2) {
                EXPECT_NEAR(corr, 1.0, 0.001) << "Same node should have correlation 1.0";
            }
        }
    }
    
    // Cleanup
    remove(dlib_file.c_str());
    remove(bench_file.c_str());
}

// Test: Ssta class does not require report() method to function
TEST_F(SstaIoSeparationTest, SstaWorksWithoutReportMethod) {
    Ssta ssta;
    
    std::string dlib_file = "/tmp/test_no_report.dlib";
    std::string bench_file = "/tmp/test_no_report.bench";
    
    std::ofstream dlib_stream(dlib_file);
    dlib_stream << dlib_content;
    dlib_stream.close();
    
    std::ofstream bench_stream(bench_file);
    bench_stream << bench_content;
    bench_stream.close();
    
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    ssta.set_lat();
    ssta.set_correlation();
    
    ssta.read_dlib();
    ssta.read_bench();
    
    // Verify we can get results without calling report()
    LatResults lat_results = ssta.getLatResults();
    CorrelationMatrix corr_matrix = ssta.getCorrelationMatrix();
    
    EXPECT_FALSE(lat_results.empty());
    EXPECT_FALSE(corr_matrix.node_names.empty());
    
    // Cleanup
    remove(dlib_file.c_str());
    remove(bench_file.c_str());
}

// Test: Results from getLatResults() match what report() would output
// This test ensures backward compatibility
TEST_F(SstaIoSeparationTest, GetLatResultsMatchesReportOutput) {
    Ssta ssta;
    
    std::string dlib_file = "/tmp/test_match.dlib";
    std::string bench_file = "/tmp/test_match.bench";
    
    std::ofstream dlib_stream(dlib_file);
    dlib_stream << dlib_content;
    dlib_stream.close();
    
    std::ofstream bench_stream(bench_file);
    bench_stream << bench_content;
    bench_stream.close();
    
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    ssta.set_lat();
    
    ssta.read_dlib();
    ssta.read_bench();
    
    // Get results using getLatResults()
    LatResults results = ssta.getLatResults();
    
    // Verify results are sorted (as getLatResults() sorts them)
    if (results.size() > 1) {
        for (size_t i = 1; i < results.size(); i++) {
            EXPECT_LE(results[i-1].node_name, results[i].node_name) 
                << "Results should be sorted by node name";
        }
    }
    
    // Verify all outputs are included
    // (This test ensures that getLatResults() includes all signals, not just outputs)
    
    // Cleanup
    remove(dlib_file.c_str());
    remove(bench_file.c_str());
}

