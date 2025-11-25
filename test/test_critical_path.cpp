// -*- c++ -*-
// Unit tests for critical path functionality
// Test-first implementation for critical path detection

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cmath>
#include <fstream>
#include <nhssta/ssta.hpp>
#include <nhssta/ssta_results.hpp>
#include <sstream>

#include "test_path_helper.h"

using namespace Nh;

class CriticalPathTest : public ::testing::Test {
   protected:
    void SetUp() override {
        ssta_ = new Ssta();
        example_dir = find_example_dir();
        if (example_dir.empty()) {
            example_dir = "../example";
        }

        // Create test directory if needed
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

// Test: CriticalPaths data structure exists
TEST_F(CriticalPathTest, CriticalPathsStructureExists) {
    // This test will fail initially until we add the data structure
    // It verifies that CriticalPaths type exists
    CriticalPaths paths;
    EXPECT_EQ(paths.size(), 0);
}

// Test: CriticalPath data structure exists
TEST_F(CriticalPathTest, CriticalPathStructureExists) {
    // This test will fail initially until we add the data structure
    CriticalPath path;
    EXPECT_TRUE(path.node_names.empty());
    EXPECT_TRUE(path.instance_names.empty());
    EXPECT_DOUBLE_EQ(path.delay_mean, 0.0);
}

// Test: Simple path tracking - single gate
TEST_F(CriticalPathTest, SimplePathSingleGate) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content = "INPUT(A)\nOUTPUT(Y)\nY = gate1(A)\n";
    std::string dlib_path = createTestFile("simple_path.dlib", dlib_content);
    std::string bench_path = createTestFile("simple_path.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    EXPECT_GE(paths.size(), 1);

    if (!paths.empty()) {
        const CriticalPath& path = paths[0];
        // Path should be A -> Y
        EXPECT_GE(path.node_names.size(), 2);
        EXPECT_EQ(path.node_names[0], "A");
        EXPECT_EQ(path.node_names[path.node_names.size() - 1], "Y");
        // Delay should be approximately 10.0 (gate delay)
        EXPECT_NEAR(path.delay_mean, 10.0, 0.1);
    }

    deleteTestFile("simple_path.dlib");
    deleteTestFile("simple_path.bench");
}

// Test: Multiple paths - should return top N
TEST_F(CriticalPathTest, MultiplePathsTopN) {
    std::string dlib_content =
        "gate1 0 y gauss (10.0, 2.0)\n"
        "gate2 0 y gauss (20.0, 3.0)\n"
        "gate3 0 y gauss (15.0, 2.5)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "INPUT(B)\n"
        "INPUT(C)\n"
        "OUTPUT(Y1)\n"
        "OUTPUT(Y2)\n"
        "OUTPUT(Y3)\n"
        "Y1 = gate1(A)\n"
        "Y2 = gate2(B)\n"
        "Y3 = gate3(C)\n";
    std::string dlib_path = createTestFile("multi_path.dlib", dlib_content);
    std::string bench_path = createTestFile("multi_path.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(3);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(3);
    EXPECT_LE(paths.size(), 3);
    EXPECT_GE(paths.size(), 1);

    // Paths should be sorted by delay (descending)
    if (paths.size() >= 2) {
        EXPECT_GE(paths[0].delay_mean, paths[1].delay_mean);
    }
    if (paths.size() >= 3) {
        EXPECT_GE(paths[1].delay_mean, paths[2].delay_mean);
    }

    deleteTestFile("multi_path.dlib");
    deleteTestFile("multi_path.bench");
}

// Test: Path with multiple gates in series
TEST_F(CriticalPathTest, PathMultipleGatesSeries) {
    std::string dlib_content =
        "gate1 0 y gauss (10.0, 2.0)\n"
        "gate2 0 y gauss (15.0, 3.0)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "N1 = gate1(A)\n"
        "Y = gate2(N1)\n";
    std::string dlib_path = createTestFile("series_path.dlib", dlib_content);
    std::string bench_path = createTestFile("series_path.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    EXPECT_GE(paths.size(), 1);

    if (!paths.empty()) {
        const CriticalPath& path = paths[0];
        // Path should be A -> N1 -> Y
        EXPECT_GE(path.node_names.size(), 3);
        EXPECT_EQ(path.node_names[0], "A");
        // Total delay should be approximately 25.0 (10.0 + 15.0)
        EXPECT_NEAR(path.delay_mean, 25.0, 0.1);
    }

    deleteTestFile("series_path.dlib");
    deleteTestFile("series_path.bench");
}

// Test: Path includes both node names and instance names
TEST_F(CriticalPathTest, PathIncludesNodeAndInstanceNames) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content = "INPUT(A)\nOUTPUT(Y)\nY = gate1(A)\n";
    std::string dlib_path = createTestFile("path_names.dlib", dlib_content);
    std::string bench_path = createTestFile("path_names.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    EXPECT_GE(paths.size(), 1);

    if (!paths.empty()) {
        const CriticalPath& path = paths[0];
        // Should have both node names and instance names
        EXPECT_FALSE(path.node_names.empty());
        EXPECT_FALSE(path.instance_names.empty());
        // Instance name should contain gate type
        EXPECT_TRUE(path.instance_names[0].find("gate1") != std::string::npos);
    }

    deleteTestFile("path_names.dlib");
    deleteTestFile("path_names.bench");
}

// Test: Critical path includes LAT-style stats for each node
TEST_F(CriticalPathTest, PathIncludesLatStyleStats) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content = "INPUT(A)\nOUTPUT(Y)\nY = gate1(A)\n";
    std::string dlib_path = createTestFile("path_stats.dlib", dlib_content);
    std::string bench_path = createTestFile("path_stats.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    ASSERT_FALSE(paths.empty());
    const auto& stats = paths[0].node_stats;
    ASSERT_EQ(stats.size(), paths[0].node_names.size());
    EXPECT_EQ(stats.front().node_name, "A");
    EXPECT_EQ(stats.back().node_name, "Y");
    EXPECT_NEAR(stats.back().mean, 10.0, 0.1);
    EXPECT_GT(stats.back().std_dev, 0.0);

    deleteTestFile("path_stats.dlib");
    deleteTestFile("path_stats.bench");
}

// Test: Default top_n is 5
TEST_F(CriticalPathTest, DefaultTopNIsFive) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content = "INPUT(A)\nOUTPUT(Y)\nY = gate1(A)\n";
    std::string dlib_path = createTestFile("default_topn.dlib", dlib_content);
    std::string bench_path = createTestFile("default_topn.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path();  // No argument, should default to 5
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths();
    EXPECT_LE(paths.size(), 5);

    deleteTestFile("default_topn.dlib");
    deleteTestFile("default_topn.bench");
}

// Test: DFF outputs act as path boundaries (sequential circuit support)
TEST_F(CriticalPathTest, DffOutputsServeAsPathSources) {
    std::string dlib_path = example_dir + "/gaussdelay.dlib";
    std::string bench_content =
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "N1 = DFF(A)\n"
        "Y = inv(N1)\n";
    std::string bench_path = createTestFile("dff_boundary.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(3);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(3);
    ASSERT_FALSE(paths.empty());
    const auto& nodes = paths[0].node_names;
    ASSERT_GE(nodes.size(), 2u);
    EXPECT_EQ(nodes.front(), "N1");  // DFF output acts as pseudo-input boundary
    EXPECT_EQ(nodes.back(), "Y");

    deleteTestFile("dff_boundary.bench");
}

// Test: Zero-argument getCriticalPaths uses stored top_n
TEST_F(CriticalPathTest, StoredTopNUsedWithoutExplicitArgument) {
    std::string dlib_content =
        "gate1 0 y gauss (10.0, 2.0)\n"
        "gate2 0 y gauss (20.0, 3.0)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y1)\n"
        "OUTPUT(Y2)\n"
        "Y1 = gate1(A)\n"
        "Y2 = gate2(B)\n";
    std::string dlib_path = createTestFile("stored_topn.dlib", dlib_content);
    std::string bench_path = createTestFile("stored_topn.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(1);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths();
    EXPECT_LE(paths.size(), 1);

    deleteTestFile("stored_topn.dlib");
    deleteTestFile("stored_topn.bench");
}

// Integration test: Critical path with actual circuit (ex4.bench)
TEST_F(CriticalPathTest, IntegrationEx4Bench) {
    std::string dlib_path = example_dir + "/ex4_gauss.dlib";
    std::string bench_path = example_dir + "/ex4.bench";

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    EXPECT_GE(paths.size(), 1);

    if (!paths.empty()) {
        const CriticalPath& path = paths[0];
        // Path should start from an input and end at output Y
        EXPECT_FALSE(path.node_names.empty());
        EXPECT_EQ(path.node_names[path.node_names.size() - 1], "Y");
        
        // Path should have valid delay
        EXPECT_GT(path.delay_mean, 0.0);
        
        // Path should have both nodes and instances
        EXPECT_FALSE(path.instance_names.empty());
        EXPECT_EQ(path.node_names.size(), path.instance_names.size() + 1);
    }
}

// Integration test: Critical path with multiple outputs
TEST_F(CriticalPathTest, IntegrationMultipleOutputs) {
    std::string dlib_content =
        "gate1 0 y gauss (10.0, 2.0)\n"
        "gate2 0 y gauss (20.0, 3.0)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y1)\n"
        "OUTPUT(Y2)\n"
        "Y1 = gate1(A)\n"
        "Y2 = gate2(B)\n";
    std::string dlib_path = createTestFile("multi_out.dlib", dlib_content);
    std::string bench_path = createTestFile("multi_out.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    EXPECT_GE(paths.size(), 1);
    
    // Should have paths for both outputs
    EXPECT_LE(paths.size(), 2);
    
    // Paths should be sorted by delay (descending)
    if (paths.size() >= 2) {
        EXPECT_GE(paths[0].delay_mean, paths[1].delay_mean);
        // First path should be from gate2 (delay 20.0)
        EXPECT_NEAR(paths[0].delay_mean, 20.0, 0.1);
    }

    deleteTestFile("multi_out.dlib");
    deleteTestFile("multi_out.bench");
}

// Integration test: Critical path with complex circuit (fan-in and fan-out)
TEST_F(CriticalPathTest, IntegrationComplexCircuit) {
    std::string dlib_content =
        "gate1 0 y gauss (10.0, 2.0)\n"
        "gate2 0 y gauss (15.0, 3.0)\n"
        "gate3 0 y gauss (12.0, 2.5)\n"
        "gate3 1 y gauss (12.0, 2.5)\n"
        "gate4 0 y gauss (8.0, 1.5)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "INPUT(B)\n"
        "INPUT(C)\n"
        "OUTPUT(Y)\n"
        "N1 = gate1(A)\n"
        "N2 = gate2(B)\n"
        "N3 = gate3(N1)\n"
        "N4 = gate4(N2)\n"
        "Y = gate3(N3, N4)\n";
    std::string dlib_path = createTestFile("complex.dlib", dlib_content);
    std::string bench_path = createTestFile("complex.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(3);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(3);
    EXPECT_GE(paths.size(), 1);
    
    // Should find at least one path
    if (!paths.empty()) {
        const CriticalPath& path = paths[0];
        // Path should start from an input
        EXPECT_TRUE(path.node_names[0] == "A" || path.node_names[0] == "B" || path.node_names[0] == "C");
        // Path should end at output Y
        EXPECT_EQ(path.node_names[path.node_names.size() - 1], "Y");
        // Path delay should be positive
        EXPECT_GT(path.delay_mean, 0.0);
    }

    deleteTestFile("complex.dlib");
    deleteTestFile("complex.bench");
}

// Test: Unconnected gates should not influence critical path extraction
TEST_F(CriticalPathTest, DisconnectedGateIsIgnored) {
    std::string dlib_content =
        "gate1 0 y gauss (12.0, 2.0)\n"
        "gate2 0 y gauss (30.0, 4.0)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y)\n"
        "Y = gate1(A)\n"
        "Z = gate2(B)\n";  // Z is never observed

    std::string dlib_path = createTestFile("disconnected.dlib", dlib_content);
    std::string bench_path = createTestFile("disconnected.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    ASSERT_FALSE(paths.empty());
    EXPECT_EQ(paths[0].node_names.front(), "A");
    EXPECT_EQ(paths[0].node_names.back(), "Y");
    EXPECT_LE(paths.size(), 1);  // Only Y should appear

    deleteTestFile("disconnected.dlib");
    deleteTestFile("disconnected.bench");
}

// Test: Circuits without OUTPUT statements return no paths
TEST_F(CriticalPathTest, NoOutputsReturnsEmptyPaths) {
    std::string dlib_content = "gate1 0 y gauss (5.0, 1.0)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "N1 = gate1(A)\n";  // No OUTPUT directive

    std::string dlib_path = createTestFile("no_output.dlib", dlib_content);
    std::string bench_path = createTestFile("no_output.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    EXPECT_TRUE(paths.empty());

    deleteTestFile("no_output.dlib");
    deleteTestFile("no_output.bench");
}

// Test: Loops in circuit description should raise an exception during parsing
TEST_F(CriticalPathTest, LoopingCircuitRaisesException) {
    std::string dlib_content =
        "gate1 0 y gauss (5.0, 1.0)\n"
        "gate2 0 y gauss (5.0, 1.0)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "N1 = gate1(Y)\n"
        "Y = gate2(N1)\n";  // Forms a loop via Y -> N1 -> Y

    std::string dlib_path = createTestFile("loop.dlib", dlib_content);
    std::string bench_path = createTestFile("loop.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();

    EXPECT_THROW(ssta_->read_bench(), Nh::RuntimeException);

    deleteTestFile("loop.dlib");
    deleteTestFile("loop.bench");
}

// Test: When multiple inputs contribute equally, the first input is selected consistently
TEST_F(CriticalPathTest, EqualDelayInputsSelectFirstSignal) {
    std::string dlib_content =
        "gatex 0 y gauss (10.0, 1.0)\n"
        "gatex 1 y gauss (10.0, 1.0)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y)\n"
        "Y = gatex(A, B)\n";

    std::string dlib_path = createTestFile("equal_delay.dlib", dlib_content);
    std::string bench_path = createTestFile("equal_delay.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    ASSERT_FALSE(paths.empty());
    const auto& nodes = paths[0].node_names;
    ASSERT_GE(nodes.size(), 2u);
    EXPECT_EQ(nodes.front(), "A");  // First input should be selected
    EXPECT_EQ(nodes.back(), "Y");

    deleteTestFile("equal_delay.dlib");
    deleteTestFile("equal_delay.bench");
}

// Test: Zero-delay gates still produce valid (zero) critical path delay
TEST_F(CriticalPathTest, ZeroDelayGateProducesZeroDelayPath) {
    std::string dlib_content = "gate1 0 y gauss (0.0, 0.1)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = gate1(A)\n";

    std::string dlib_path = createTestFile("zero_delay.dlib", dlib_content);
    std::string bench_path = createTestFile("zero_delay.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths paths = ssta_->getCriticalPaths(5);
    ASSERT_FALSE(paths.empty());
    EXPECT_NEAR(paths[0].delay_mean, 0.0, 1e-6);

    deleteTestFile("zero_delay.dlib");
    deleteTestFile("zero_delay.bench");
}

// Test: Calling getCriticalPaths() without prior analysis returns empty results
TEST_F(CriticalPathTest, GetCriticalPathsWithoutAnalysisIsSafe) {
    Ssta local_ssta;
    local_ssta.set_critical_path(5);
    CriticalPaths paths = local_ssta.getCriticalPaths(5);
    EXPECT_TRUE(paths.empty());
}

// Test: Requesting top 0 critical paths returns an empty result even if paths exist
TEST_F(CriticalPathTest, RequestingZeroTopNReturnsEmpty) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content =
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = gate1(A)\n";

    std::string dlib_path = createTestFile("zero_topn.dlib", dlib_content);
    std::string bench_path = createTestFile("zero_topn.bench", bench_content);

    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_critical_path(5);
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();

    CriticalPaths non_empty = ssta_->getCriticalPaths(5);
    ASSERT_FALSE(non_empty.empty());

    CriticalPaths empty_paths = ssta_->getCriticalPaths(0);
    EXPECT_TRUE(empty_paths.empty());

    deleteTestFile("zero_topn.dlib");
    deleteTestFile("zero_topn.bench");
}

