// -*- c++ -*-
// Integration tests for SSTA logic functions (getLatResults/getCorrelationMatrix)
// Phase 2: I/O and logic separation

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>

#include "../src/parser.hpp"
#include "test_path_helper.h"

using namespace Nh;
using RandomVar = ::RandomVariable::RandomVariable;
using Normal = ::RandomVariable::Normal;

namespace {
std::string locate_example_dir() {
    std::string dir = find_example_dir();
    if (dir.empty()) {
        dir = "../example";
    }
    return dir;
}

void loadExample(Ssta& ssta, const std::string& dlib, const std::string& bench) {
    std::string example_dir = locate_example_dir();
    ssta.set_dlib(example_dir + "/" + dlib);
    ssta.set_bench(example_dir + "/" + bench);
    ssta.set_lat();
    ssta.set_correlation();
    ssta.check();
    ssta.read_dlib();
    ssta.read_bench();
}
}  // namespace

// Test getLatResults with ex4_gauss.dlib + ex4.bench
TEST(SstaLogicIntegrationTest, LatAndCorrelationChecks) {
    Ssta ssta;
    loadExample(ssta, "ex4_gauss.dlib", "ex4.bench");

    LatResults results = ssta.getLatResults();
    EXPECT_GT(results.size(), 0);

    bool found_A = false, found_B = false, found_C = false, found_Y = false;
    for (const auto& result : results) {
        EXPECT_FALSE(result.node_name.empty());
        EXPECT_FALSE(std::isnan(result.mean));
        EXPECT_FALSE(std::isnan(result.std_dev));
        EXPECT_FALSE(std::isinf(result.mean));
        EXPECT_FALSE(std::isinf(result.std_dev));
        EXPECT_GE(result.std_dev, 0.0);
        if (result.node_name == "A") {
            found_A = true;
        } else if (result.node_name == "B") {
            found_B = true;
        } else if (result.node_name == "C") {
            found_C = true;
        } else if (result.node_name == "Y") {
            found_Y = true;
            EXPECT_GT(result.mean, 0.0);
        }
    }
    EXPECT_TRUE(found_A);
    EXPECT_TRUE(found_B);
    EXPECT_TRUE(found_C);
    EXPECT_TRUE(found_Y);

    CorrelationMatrix matrix = ssta.getCorrelationMatrix();
    EXPECT_GT(matrix.node_names.size(), 0);
    EXPECT_EQ(matrix.correlations.size(), matrix.node_names.size() * matrix.node_names.size());

    for (const auto& node1 : matrix.node_names) {
        for (const auto& node2 : matrix.node_names) {
            double corr12 = matrix.getCorrelation(node1, node2);
            double corr21 = matrix.getCorrelation(node2, node1);
            EXPECT_NEAR(corr12, corr21, 1e-6);
            double self_corr = matrix.getCorrelation(node1, node1);
            EXPECT_NEAR(self_corr, 1.0, 1e-6);
            EXPECT_GE(corr12, -1.0);
            EXPECT_LE(corr12, 1.0);
        }
    }

    for (const auto& pair : matrix.correlations) {
        EXPECT_FALSE(std::isnan(pair.second));
        EXPECT_FALSE(std::isinf(pair.second));
    }
}
