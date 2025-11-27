// -*- c++ -*-
// Test for covariance calculation order dependency bug (Issue #154)

#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>

#include "test_path_helper.h"

using namespace Nh;

class CovarianceOrderTest : public ::testing::Test {
   protected:
    void SetUp() override {
        example_dir = find_example_dir();
        if (example_dir.empty()) {
            example_dir = "../example";
        }
    }

    std::string example_dir;
};

// Test: Correlation values should be consistent regardless of option order
TEST_F(CovarianceOrderTest, CorrelationConsistentWithAndWithoutFullMatrix) {
    // First run: -p only (endpoint correlation without full matrix)
    Ssta ssta1;
    ssta1.set_dlib(example_dir + "/gaussdelay.dlib");
    ssta1.set_bench(example_dir + "/s820.bench");
    ssta1.set_critical_path(5);
    ssta1.read_dlib();
    ssta1.read_bench();

    CriticalPaths paths1 = ssta1.getCriticalPaths(5);
    CorrelationMatrix endpoint_matrix1 = ssta1.getPathEndpointCorrelationMatrix(paths1);

    // Second run: -c -p (full matrix first, then endpoint)
    Ssta ssta2;
    ssta2.set_dlib(example_dir + "/gaussdelay.dlib");
    ssta2.set_bench(example_dir + "/s820.bench");
    ssta2.set_lat();
    ssta2.set_correlation();
    ssta2.set_critical_path(5);
    ssta2.read_dlib();
    ssta2.read_bench();

    // Compute full correlation matrix first (this populates the cache)
    CorrelationMatrix full_matrix = ssta2.getCorrelationMatrix();

    CriticalPaths paths2 = ssta2.getCriticalPaths(5);
    CorrelationMatrix endpoint_matrix2 = ssta2.getPathEndpointCorrelationMatrix(paths2);

    // Both endpoint matrices should have the same values
    ASSERT_EQ(endpoint_matrix1.node_names.size(), endpoint_matrix2.node_names.size());

    for (const auto& name_i : endpoint_matrix1.node_names) {
        for (const auto& name_j : endpoint_matrix1.node_names) {
            double corr1 = endpoint_matrix1.getCorrelation(name_i, name_j);
            double corr2 = endpoint_matrix2.getCorrelation(name_i, name_j);

            // Values should be identical (or very close)
            EXPECT_NEAR(corr1, corr2, 0.001)
                << "Correlation mismatch for " << name_i << " <-> " << name_j
                << ": without full matrix = " << corr1
                << ", with full matrix = " << corr2;
        }
    }
}

// Test: Endpoint correlation should match values from full correlation matrix
TEST_F(CovarianceOrderTest, EndpointCorrelationMatchesFullMatrix) {
    Ssta ssta;
    ssta.set_dlib(example_dir + "/gaussdelay.dlib");
    ssta.set_bench(example_dir + "/s820.bench");
    ssta.set_lat();
    ssta.set_correlation();
    ssta.set_critical_path(5);
    ssta.read_dlib();
    ssta.read_bench();

    CorrelationMatrix full_matrix = ssta.getCorrelationMatrix();
    CriticalPaths paths = ssta.getCriticalPaths(5);
    CorrelationMatrix endpoint_matrix = ssta.getPathEndpointCorrelationMatrix(paths);

    // Check that endpoint correlations match full matrix
    for (const auto& name_i : endpoint_matrix.node_names) {
        for (const auto& name_j : endpoint_matrix.node_names) {
            double endpoint_corr = endpoint_matrix.getCorrelation(name_i, name_j);
            double full_corr = full_matrix.getCorrelation(name_i, name_j);

            EXPECT_NEAR(endpoint_corr, full_corr, 0.001)
                << "Correlation mismatch for " << name_i << " <-> " << name_j
                << ": endpoint = " << endpoint_corr
                << ", full = " << full_corr;
        }
    }
}

