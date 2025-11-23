// -*- c++ -*-
// Unit tests for DFF gate support
// Issue #61: 機能改善: dffゲートの適切なサポート実装

#include <gtest/gtest.h>
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>
#include <fstream>
#include <sstream>
#include <cmath>
#include "test_path_helper.h"

using namespace Nh;
using RandomVar = ::RandomVariable::RandomVariable;
using Normal = ::RandomVariable::Normal;

class DffGateTest : public ::testing::Test {
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

// Test: DFF gate with data input (d) should be considered
// Currently, set_dff_out() ignores data input (d) and only considers clock (ck)
// This test verifies that data input is properly handled
TEST_F(DffGateTest, DffGateConsidersDataInput) {
    // Create dlib with dff gate definition and and gate
    std::string dlib_content = 
        "and 0 y gauss (38.0, 4.0)\n"
        "and 1 y gauss (41.0, 4.0)\n"
        "dff ck q gauss (30.0, 3.5)\n"
        "dff d q const (0.0)\n";
    
    // Create bench with dff gate that has data input
    // Q = DFF(N1) where N1 is a signal with delay from AND gate
    std::string bench_content = 
        "INPUT(D)\n"
        "INPUT(CK)\n"
        "OUTPUT(Q)\n"
        "N1 = AND(D, CK)\n"  // Intermediate signal with delay from AND gate
        "Q = DFF(N1)\n";     // DFF should consider N1 (data input)
    
    std::string dlib_path = createTestFile("test_dff_data.dlib", dlib_content);
    std::string bench_path = createTestFile("test_dff_data.bench", bench_content);
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    
    // Should not throw
    EXPECT_NO_THROW(ssta_->read_bench());
    
    // Get LAT results
    LatResults results = ssta_->getLatResults();
    
    // Q should have timing that considers N1 (data input)
    // Currently, Q will only have ck->q delay (30.0, 3.5)
    // After proper implementation, Q should consider N1 + d->q delay
    bool found_q = false;
    bool found_n1 = false;
    double n1_mean = 0.0;
    for (const auto& result : results) {
        if (result.node_name == "Q") {
            found_q = true;
            // Current implementation: Q = 0.0 + (30.0, 3.5) = (30.0, 3.5)
            // Expected after fix: Q should consider N1 delay + d->q delay
            // For now, we just verify Q exists and has some delay
            EXPECT_GE(result.mean, 0.0);
        }
        if (result.node_name == "N1") {
            found_n1 = true;
            n1_mean = result.mean;
            // N1 should have delay from AND gate
            EXPECT_GT(result.mean, 0.0);
        }
    }
    EXPECT_TRUE(found_q);
    EXPECT_TRUE(found_n1);
    
    // After proper implementation, Q should have timing >= N1 + d->q delay
    // Currently Q = 30.0 (ck->q delay only), ignoring N1 (data input)
    // After fix: Q should be MAX(ck delay, N1 delay + d->q delay)
    // Since d->q delay is 0.0, Q should be >= N1 delay
    // This assertion will fail with current implementation, which is expected
    if (found_q && found_n1) {
        for (const auto& result : results) {
            if (result.node_name == "Q") {
                // Current implementation ignores data input (N1)
                // Q = 0.0 (input) + 30.0 (ck->q delay) = 30.0
                // After proper implementation, Q should consider N1 delay
                // Expected: Q.mean >= n1_mean (since d->q delay is 0.0)
                // This test will fail with current implementation
                EXPECT_GE(result.mean, n1_mean) 
                    << "DFF output Q should consider data input N1 delay. "
                    << "Current: Q.mean=" << result.mean 
                    << ", N1.mean=" << n1_mean
                    << ". Q should be >= N1 delay after proper implementation.";
            }
        }
    }
    
    deleteTestFile("test_dff_data.dlib");
    deleteTestFile("test_dff_data.bench");
}

// Test: DFF gate should be processed in connect_instances()
// Currently, dff gates are handled separately and not added to net_
// This test verifies that dff gates can be processed through normal flow
TEST_F(DffGateTest, DffGateProcessedInConnectInstances) {
    std::string dlib_content = 
        "and 0 y gauss (38.0, 4.0)\n"
        "and 1 y gauss (41.0, 4.0)\n"
        "dff ck q gauss (30.0, 3.5)\n"
        "dff d q const (0.0)\n";
    
    // Simple circuit: DFF with data input from AND gate
    std::string bench_content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "INPUT(CK)\n"
        "OUTPUT(Q)\n"
        "N1 = AND(A, B)\n"  // N1 has delay from AND gate
        "Q = DFF(N1)\n";     // DFF should process N1 as data input
    
    std::string dlib_path = createTestFile("test_dff_connect.dlib", dlib_content);
    std::string bench_path = createTestFile("test_dff_connect.bench", bench_content);
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    
    EXPECT_NO_THROW(ssta_->read_bench());
    
    LatResults results = ssta_->getLatResults();
    
    // Verify that Q has timing that considers N1
    bool found_q = false;
    bool found_n1 = false;
    double n1_mean = 0.0;
    for (const auto& result : results) {
        if (result.node_name == "Q") {
            found_q = true;
            // Q should have delay >= N1 delay + d->q delay (0.0)
            // Currently Q = 0.0 + ck->q delay (30.0, 3.5)
            // After fix: Q should be MAX(ck delay, N1 delay + d->q delay)
            EXPECT_GE(result.mean, 0.0);
        }
        if (result.node_name == "N1") {
            found_n1 = true;
            n1_mean = result.mean;
            // N1 should have delay from AND gate
            EXPECT_GT(result.mean, 0.0);
        }
    }
    EXPECT_TRUE(found_q);
    EXPECT_TRUE(found_n1);
    
    // After proper implementation, Q should have timing >= N1 delay
    // Currently Q = 30.0 (ck->q delay only), ignoring N1 (data input)
    // After fix: Q should be MAX(ck delay, N1 delay + d->q delay)
    // This assertion will fail with current implementation, which is expected
    if (found_q && found_n1) {
        for (const auto& result : results) {
            if (result.node_name == "Q") {
                // Current implementation ignores data input (N1)
                // Q = 0.0 (input) + 30.0 (ck->q delay) = 30.0
                // After proper implementation, Q should consider N1 delay
                // Expected: Q.mean >= n1_mean (since d->q delay is 0.0)
                // This test will fail with current implementation
                EXPECT_GE(result.mean, n1_mean)
                    << "DFF output Q should consider data input N1 delay. "
                    << "Current: Q.mean=" << result.mean 
                    << ", N1.mean=" << n1_mean
                    << ". Q should be >= N1 delay after proper implementation.";
            }
        }
    }
    
    deleteTestFile("test_dff_connect.dlib");
    deleteTestFile("test_dff_connect.bench");
}

// Test: DFF gate with clock input should work correctly
// Clock input (ck) should be treated as a timing reference
TEST_F(DffGateTest, DffGateClockInput) {
    std::string dlib_content = 
        "dff ck q gauss (30.0, 3.5)\n"
        "dff d q const (0.0)\n";
    
    // DFF with explicit clock input
    std::string bench_content = 
        "INPUT(D)\n"
        "INPUT(CK)\n"
        "OUTPUT(Q)\n"
        "Q = DFF(D, CK)\n";  // DFF with both data and clock inputs
    
    std::string dlib_path = createTestFile("test_dff_clock.dlib", dlib_content);
    std::string bench_path = createTestFile("test_dff_clock.bench", bench_content);
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    
    EXPECT_NO_THROW(ssta_->read_bench());
    
    LatResults results = ssta_->getLatResults();
    
    bool found_q = false;
    for (const auto& result : results) {
        if (result.node_name == "Q") {
            found_q = true;
            // Q should have timing that considers both D and CK
            EXPECT_GE(result.mean, 0.0);
            break;
        }
    }
    EXPECT_TRUE(found_q);
    
    deleteTestFile("test_dff_clock.dlib");
    deleteTestFile("test_dff_clock.bench");
}

// Test: DFF gate in sequential circuit (like s27.bench)
// This is an integration test for real-world usage
TEST_F(DffGateTest, DffGateInSequentialCircuit) {
    // Use existing dlib that has dff definition
    std::string dlib_path = example_dir + "/ex4_gauss.dlib";
    std::string bench_path = example_dir + "/s27.bench";
    
    // Check if files exist
    std::ifstream dlib_file(dlib_path);
    std::ifstream bench_file(bench_path);
    if (!dlib_file.good() || !bench_file.good()) {
        GTEST_SKIP() << "Test files not found, skipping integration test";
        return;
    }
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    
    // Should process without errors
    EXPECT_NO_THROW(ssta_->read_bench());
    
    LatResults results = ssta_->getLatResults();
    
    // Verify that dff outputs have reasonable timing
    // In s27.bench, G5, G6, G7 are DFF outputs
    std::vector<std::string> dff_outputs = {"G5", "G6", "G7"};
    for (const auto& dff_out : dff_outputs) {
        bool found = false;
        for (const auto& result : results) {
            if (result.node_name == dff_out) {
                found = true;
                // DFF outputs should have timing >= clock delay
                EXPECT_GE(result.mean, 0.0);
                break;
            }
        }
        // Note: Currently dff gates might not appear in results
        // This test documents expected behavior after proper implementation
        if (!found) {
            // This is expected with current implementation
            // After proper implementation, dff outputs should appear
        }
    }
}

// Test: DFF gate timing should consider setup/hold constraints
// Currently, this is a placeholder for future enhancement
TEST_F(DffGateTest, DffGateTimingConstraints) {
    // This test documents expected behavior for setup/hold time consideration
    // For now, we just verify that dff gates can be processed
    std::string dlib_content = 
        "dff ck q gauss (30.0, 3.5)\n"
        "dff d q const (0.0)\n";
    
    std::string bench_content = 
        "INPUT(D)\n"
        "INPUT(CK)\n"
        "OUTPUT(Q)\n"
        "Q = DFF(D)\n";
    
    std::string dlib_path = createTestFile("test_dff_constraints.dlib", dlib_content);
    std::string bench_path = createTestFile("test_dff_constraints.bench", bench_content);
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    
    EXPECT_NO_THROW(ssta_->read_bench());
    
    deleteTestFile("test_dff_constraints.dlib");
    deleteTestFile("test_dff_constraints.bench");
}

