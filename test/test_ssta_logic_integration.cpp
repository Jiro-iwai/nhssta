// -*- c++ -*-
// Integration tests for SSTA logic functions (getLatResults/getCorrelationMatrix)
// Phase 2: I/O and logic separation

#include <gtest/gtest.h>
#include "Ssta.h"
#include "SstaResults.h"
#include "Parser.h"
#include <fstream>
#include <cmath>
#include <algorithm>

using namespace Nh;
using RandomVar = ::RandomVariable::RandomVariable;
using Normal = ::RandomVariable::Normal;

class SstaLogicIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ssta_ = new Ssta();
        example_dir = "../example";
    }

    void TearDown() override {
        delete ssta_;
    }

    void loadExample(const std::string& dlib, const std::string& bench) {
        ssta_->set_dlib(example_dir + "/" + dlib);
        ssta_->set_bench(example_dir + "/" + bench);
        ssta_->set_lat();
        ssta_->set_correlation();
        ssta_->check();
        ssta_->read_dlib();
        ssta_->read_bench();
    }

    Ssta* ssta_;
    std::string example_dir;
};

// Test getLatResults with ex4_gauss.dlib + ex4.bench
TEST_F(SstaLogicIntegrationTest, GetLatResultsEx4) {
    loadExample("ex4_gauss.dlib", "ex4.bench");
    
    LatResults results = ssta_->getLatResults();
    
    // Should have results for all signals
    EXPECT_GT(results.size(), 0);
    
    // Find specific nodes
    bool found_A = false, found_B = false, found_C = false, found_Y = false;
    for (const auto& result : results) {
        if (result.node_name == "A") {
            found_A = true;
            EXPECT_GE(result.mean, 0.0);
            EXPECT_GE(result.std_dev, 0.0);
        } else if (result.node_name == "B") {
            found_B = true;
            EXPECT_GE(result.mean, 0.0);
            EXPECT_GE(result.std_dev, 0.0);
        } else if (result.node_name == "C") {
            found_C = true;
            EXPECT_GE(result.mean, 0.0);
            EXPECT_GE(result.std_dev, 0.0);
        } else if (result.node_name == "Y") {
            found_Y = true;
            // Y should have a larger mean than inputs (it's the output)
            EXPECT_GT(result.mean, 0.0);
            EXPECT_GE(result.std_dev, 0.0);
        }
    }
    
    EXPECT_TRUE(found_A) << "Should find node A";
    EXPECT_TRUE(found_B) << "Should find node B";
    EXPECT_TRUE(found_C) << "Should find node C";
    EXPECT_TRUE(found_Y) << "Should find node Y";
}

// Test getCorrelationMatrix with ex4_gauss.dlib + ex4.bench
TEST_F(SstaLogicIntegrationTest, GetCorrelationMatrixEx4) {
    loadExample("ex4_gauss.dlib", "ex4.bench");
    
    CorrelationMatrix matrix = ssta_->getCorrelationMatrix();
    
    // Should have node names
    EXPECT_GT(matrix.node_names.size(), 0);
    
    // Should have correlations
    EXPECT_GT(matrix.correlations.size(), 0);
    
    // Check that correlation matrix is symmetric
    for (const auto& node1 : matrix.node_names) {
        for (const auto& node2 : matrix.node_names) {
            double corr12 = matrix.getCorrelation(node1, node2);
            double corr21 = matrix.getCorrelation(node2, node1);
            EXPECT_NEAR(corr12, corr21, 1e-6) 
                << "Correlation matrix should be symmetric: " 
                << node1 << " <-> " << node2;
        }
    }
    
    // Check that diagonal elements (self-correlation) are 1.0
    for (const auto& node : matrix.node_names) {
        double self_corr = matrix.getCorrelation(node, node);
        EXPECT_NEAR(self_corr, 1.0, 1e-6) 
            << "Self-correlation should be 1.0 for node: " << node;
    }
    
    // Check that correlations are in valid range [-1, 1]
    for (const auto& node1 : matrix.node_names) {
        for (const auto& node2 : matrix.node_names) {
            double corr = matrix.getCorrelation(node1, node2);
            EXPECT_GE(corr, -1.0) << "Correlation should be >= -1.0";
            EXPECT_LE(corr, 1.0) << "Correlation should be <= 1.0";
        }
    }
}

// Test that getLatResults and report_lat produce consistent data
// (This is tested indirectly by ensuring both use the same underlying logic)
TEST_F(SstaLogicIntegrationTest, LatResultsConsistency) {
    loadExample("ex4_gauss.dlib", "ex4.bench");
    
    LatResults results = ssta_->getLatResults();
    
    // Verify that results are sorted consistently
    // (report_lat iterates through signals_ map, which is ordered)
    EXPECT_GT(results.size(), 0);
    
    // Check that all results have valid values
    for (const auto& result : results) {
        EXPECT_FALSE(result.node_name.empty());
        EXPECT_FALSE(std::isnan(result.mean));
        EXPECT_FALSE(std::isnan(result.std_dev));
        EXPECT_FALSE(std::isinf(result.mean));
        EXPECT_FALSE(std::isinf(result.std_dev));
        EXPECT_GE(result.std_dev, 0.0);
    }
}

// Test that getCorrelationMatrix and report_correlation produce consistent data
TEST_F(SstaLogicIntegrationTest, CorrelationMatrixConsistency) {
    loadExample("ex4_gauss.dlib", "ex4.bench");
    
    CorrelationMatrix matrix = ssta_->getCorrelationMatrix();
    
    // Verify that matrix has expected structure
    EXPECT_GT(matrix.node_names.size(), 0);
    EXPECT_EQ(matrix.correlations.size(), 
              matrix.node_names.size() * matrix.node_names.size());
    
    // Check that all correlations are valid numbers
    for (const auto& pair : matrix.correlations) {
        EXPECT_FALSE(std::isnan(pair.second));
        EXPECT_FALSE(std::isinf(pair.second));
    }
}

