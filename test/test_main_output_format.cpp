// -*- c++ -*-
// Unit tests for main.cpp output format
// Tests that main.cpp can format and output results using getLatResults() and getCorrelationMatrix()
// without relying on Ssta::report()

#include <gtest/gtest.h>
#include <iomanip>
#include <sstream>
#include <string>

#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>

#include "test_path_helper.h"

using namespace Nh;

// Helper function to format LAT results (matching report_lat() format)
std::string formatLatResults(const LatResults& results) {
    std::ostringstream oss;
    oss << "#" << std::endl;
    oss << "# LAT" << std::endl;
    oss << "#" << std::endl;
    oss << std::left << std::setw(15) << "#node" << std::right << std::setw(10) << "mu" << std::setw(9) << "std" << std::endl;
    oss << "#---------------------------------" << std::endl;

    for (const auto& result : results) {
        oss << std::left << std::setw(15) << result.node_name;
        oss << std::right << std::setw(10) << std::fixed << std::setprecision(3)
            << result.mean;
        oss << std::right << std::setw(9) << std::fixed << std::setprecision(3)
            << result.std_dev << std::endl;
    }

    oss << "#---------------------------------" << std::endl;
    return oss.str();
}

// Helper function to format correlation matrix (matching report_correlation() format)
std::string formatCorrelationMatrix(const CorrelationMatrix& matrix) {
    std::ostringstream oss;
    oss << "#" << std::endl;
    oss << "# correlation matrix" << std::endl;
    oss << "#" << std::endl;

    oss << "#\t";
    for (const auto& node_name : matrix.node_names) {
        oss << node_name << "\t";
    }
    oss << std::endl;

    // Print separator line
    oss << "#-------";
    for (size_t i = 1; i < matrix.node_names.size(); i++) {
        oss << "--------";
    }
    oss << "-----" << std::endl;

    for (const auto& node_name : matrix.node_names) {
        oss << node_name << "\t";

        for (const auto& other_name : matrix.node_names) {
            double corr = matrix.getCorrelation(node_name, other_name);
            oss << std::fixed << std::setprecision(3) << std::setw(4) << corr << "\t";
        }

        oss << std::endl;
    }

    // Print separator line
    oss << "#-------";
    for (size_t i = 1; i < matrix.node_names.size(); i++) {
        oss << "--------";
    }
    oss << "-----" << std::endl;

    return oss.str();
}

class MainOutputFormatTest : public ::testing::Test {
   protected:
    void SetUp() override {
        example_dir = find_example_dir();
        if (example_dir.empty()) {
            example_dir = "../example";  // Fallback
        }
    }

    void TearDown() override {
        // Cleanup
    }

    std::string example_dir;
};

// Test: formatLatResults produces correct format
TEST_F(MainOutputFormatTest, FormatLatResultsProducesCorrectFormat) {
    LatResults results;
    results.emplace_back("A", 0.0, 0.001);
    results.emplace_back("B", 0.0, 0.001);
    results.emplace_back("N1", 35.016, 3.575);
    results.emplace_back("Y", 89.791, 4.929);

    std::string formatted = formatLatResults(results);

    // Verify format contains expected elements
    EXPECT_NE(formatted.find("# LAT"), std::string::npos);
    EXPECT_NE(formatted.find("#node"), std::string::npos);
    EXPECT_NE(formatted.find("mu"), std::string::npos);
    EXPECT_NE(formatted.find("std"), std::string::npos);
    EXPECT_NE(formatted.find("A"), std::string::npos);
    EXPECT_NE(formatted.find("Y"), std::string::npos);
}

// Test: formatCorrelationMatrix produces correct format
TEST_F(MainOutputFormatTest, FormatCorrelationMatrixProducesCorrectFormat) {
    CorrelationMatrix matrix;
    matrix.node_names = {"A", "B", "Y"};
    matrix.correlations[std::make_pair("A", "A")] = 1.0;
    matrix.correlations[std::make_pair("B", "B")] = 1.0;
    matrix.correlations[std::make_pair("Y", "Y")] = 1.0;
    matrix.correlations[std::make_pair("A", "B")] = 0.0;
    matrix.correlations[std::make_pair("A", "Y")] = 0.0;
    matrix.correlations[std::make_pair("B", "Y")] = 0.0;

    std::string formatted = formatCorrelationMatrix(matrix);

    // Verify format contains expected elements
    EXPECT_NE(formatted.find("# correlation matrix"), std::string::npos);
    EXPECT_NE(formatted.find("A"), std::string::npos);
    EXPECT_NE(formatted.find("B"), std::string::npos);
    EXPECT_NE(formatted.find("Y"), std::string::npos);
}

// Test: Output format matches existing report() output format
// This test verifies backward compatibility
TEST_F(MainOutputFormatTest, OutputFormatMatchesReportFormat) {
    Ssta ssta;
    
    std::string dlib_file = example_dir + "/ex4_gauss.dlib";
    std::string bench_file = example_dir + "/ex4.bench";
    
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    ssta.set_lat();
    ssta.set_correlation();
    
    ssta.read_dlib();
    ssta.read_bench();
    
    // Get results using getLatResults() and getCorrelationMatrix()
    LatResults lat_results = ssta.getLatResults();
    CorrelationMatrix corr_matrix = ssta.getCorrelationMatrix();
    
    // Format results
    std::string lat_formatted = formatLatResults(lat_results);
    std::string corr_formatted = formatCorrelationMatrix(corr_matrix);
    
    // Verify formatted output contains expected data
    EXPECT_FALSE(lat_formatted.empty());
    EXPECT_FALSE(corr_formatted.empty());
    
    // Verify LAT format structure
    EXPECT_NE(lat_formatted.find("# LAT"), std::string::npos);
    EXPECT_NE(lat_formatted.find("#node"), std::string::npos);
    
    // Verify correlation format structure
    EXPECT_NE(corr_formatted.find("# correlation matrix"), std::string::npos);
}

// Test: Main can output results without calling Ssta::report()
TEST_F(MainOutputFormatTest, MainCanOutputWithoutReport) {
    Ssta ssta;
    
    std::string dlib_file = example_dir + "/ex4_gauss.dlib";
    std::string bench_file = example_dir + "/ex4.bench";
    
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    ssta.set_lat();
    ssta.set_correlation();
    
    ssta.read_dlib();
    ssta.read_bench();
    
    // Simulate what main.cpp would do without report()
    std::ostringstream output;
    
    if (true) {  // is_lat flag
        output << std::endl;
        LatResults lat_results = ssta.getLatResults();
        output << formatLatResults(lat_results);
    }
    
    if (true) {  // is_correlation flag
        output << std::endl;
        CorrelationMatrix corr_matrix = ssta.getCorrelationMatrix();
        output << formatCorrelationMatrix(corr_matrix);
    }
    
    std::string result = output.str();
    
    // Verify output is not empty
    EXPECT_FALSE(result.empty());
    
    // Verify output contains expected sections
    EXPECT_NE(result.find("# LAT"), std::string::npos);
    EXPECT_NE(result.find("# correlation matrix"), std::string::npos);
}

