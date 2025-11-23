// -*- c++ -*-
// Unit tests for SSTA results logic functions
// Phase 2: I/O and logic separation

#include <gtest/gtest.h>
#include "Ssta.h"
#include "SstaResults.h"
#include "RandomVariable.h"
#include "Normal.h"
#include <cmath>
#include <algorithm>

using namespace Nh;
using RandomVar = ::RandomVariable::RandomVariable;
using Normal = ::RandomVariable::Normal;

class SstaResultsTest : public ::testing::Test {
protected:
    void SetUp() override {
        ssta_ = new Ssta();
    }

    void TearDown() override {
        delete ssta_;
    }

    Ssta* ssta_;
};

// Test getLatResults with empty signals
TEST_F(SstaResultsTest, EmptyLatResults) {
    LatResults results = ssta_->getLatResults();
    EXPECT_EQ(results.size(), 0);
}

// Test getCorrelationMatrix with empty signals
TEST_F(SstaResultsTest, EmptyCorrelationMatrix) {
    CorrelationMatrix matrix = ssta_->getCorrelationMatrix();
    EXPECT_EQ(matrix.node_names.size(), 0);
    EXPECT_EQ(matrix.correlations.size(), 0);
}

// Test getLatResults with single signal
// Note: This test requires setting up signals, which is complex.
// We'll test the integration with actual data files instead.

// Test CorrelationMatrix::getCorrelation
TEST(CorrelationMatrixTest, GetCorrelationSameNode) {
    CorrelationMatrix matrix;
    matrix.node_names.push_back("node1");
    matrix.correlations[std::make_pair("node1", "node1")] = 1.0;
    
    EXPECT_DOUBLE_EQ(matrix.getCorrelation("node1", "node1"), 1.0);
}

TEST(CorrelationMatrixTest, GetCorrelationDifferentNodes) {
    CorrelationMatrix matrix;
    matrix.node_names.push_back("node1");
    matrix.node_names.push_back("node2");
    matrix.correlations[std::make_pair("node1", "node2")] = 0.5;
    
    EXPECT_DOUBLE_EQ(matrix.getCorrelation("node1", "node2"), 0.5);
    EXPECT_DOUBLE_EQ(matrix.getCorrelation("node2", "node1"), 0.5);
}

TEST(CorrelationMatrixTest, GetCorrelationNotFound) {
    CorrelationMatrix matrix;
    matrix.node_names.push_back("node1");
    matrix.node_names.push_back("node2");
    
    EXPECT_DOUBLE_EQ(matrix.getCorrelation("node1", "node3"), 0.0);
}

// Integration test: Test that getLatResults and report_lat produce consistent results
// This will be tested via integration tests with actual data files

