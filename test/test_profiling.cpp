// -*- c++ -*-
// Unit tests for profiling and optimization
// Issue #49: 性能改善: ホットパスのプロファイリングと局所最適化

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <chrono>
#include <fstream>
#include <nhssta/exception.hpp>
#include <nhssta/ssta.hpp>
#include <sstream>
#include <string>

using namespace Nh;

class ProfilingTest : public ::testing::Test {
   protected:
    void SetUp() override {
        test_data_dir = "../test/test_data";
        struct stat info;
        if (stat(test_data_dir.c_str(), &info) != 0) {
            std::string cmd = "mkdir -p " + test_data_dir;
            system(cmd.c_str());
        }
    }

    void TearDown() override {
        // Clean up any test files created
    }

    std::string createTestFile(const std::string& filename, const std::string& content) {
        std::string filepath = test_data_dir + "/" + filename;
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << content;
            file.close();
        }
        return filepath;
    }

    void deleteTestFile(const std::string& filename) {
        std::string filepath = test_data_dir + "/" + filename;
        remove(filepath.c_str());
    }

    // Helper to measure execution time
    template <typename Func>
    double measureTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0;  // Convert to milliseconds
    }

    std::string test_data_dir;
};

// Test: Performance baseline for small circuit
TEST_F(ProfilingTest, SmallCircuitBaseline) {
    std::string dlib_content = "and2 0 y gauss(10.0, 1.0)\nand2 1 y gauss(10.0, 1.0)\n";
    std::string bench_content = "INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = and2(a, b)\n";

    std::string dlib_file = createTestFile("test_small.dlib", dlib_content);
    std::string bench_file = createTestFile("test_small.bench", bench_content);

    Ssta ssta;
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);

    double time_ms = measureTime([&]() {
        ssta.read_dlib();
        ssta.read_bench();
    });

    // Small circuit should complete quickly (< 100ms)
    EXPECT_LT(time_ms, 100.0) << "Small circuit should complete in < 100ms";

    deleteTestFile("test_small.dlib");
    deleteTestFile("test_small.bench");
}

// Test: Performance measurement for read_dlib
TEST_F(ProfilingTest, ReadDlibPerformance) {
    // Create a larger dlib file with multiple gates
    std::ostringstream dlib_content;
    for (int i = 0; i < 10; ++i) {
        dlib_content << "gate" << i << " 0 y gauss(10.0, 1.0)\n";
        dlib_content << "gate" << i << " 1 y gauss(10.0, 1.0)\n";
    }

    std::string dlib_file = createTestFile("test_dlib_perf.dlib", dlib_content.str());

    Ssta ssta;
    ssta.set_dlib(dlib_file);

    double time_ms = measureTime([&]() { ssta.read_dlib(); });

    // read_dlib should be reasonably fast even with multiple gates
    EXPECT_LT(time_ms, 50.0) << "read_dlib should complete in < 50ms for 10 gates";

    deleteTestFile("test_dlib_perf.dlib");
}

// Test: Performance measurement for connect_instances
TEST_F(ProfilingTest, ConnectInstancesPerformance) {
    std::string dlib_content = "and2 0 y gauss(10.0, 1.0)\nand2 1 y gauss(10.0, 1.0)\n";

    // Create a chain of gates to test connect_instances performance
    std::ostringstream bench_content;
    bench_content << "INPUT(a0)\n";
    for (int i = 0; i < 10; ++i) {
        bench_content << "y" << i << " = and2(a" << i << ", a" << i << ")\n";
        if (i < 9) {
            bench_content << "INPUT(a" << (i + 1) << ")\n";
        }
    }
    bench_content << "OUTPUT(y9)\n";

    std::string dlib_file = createTestFile("test_connect.dlib", dlib_content);
    std::string bench_file = createTestFile("test_connect.bench", bench_content.str());

    Ssta ssta;
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    ssta.read_dlib();

    double time_ms = measureTime([&]() { ssta.read_bench(); });

    // connect_instances should complete reasonably fast
    EXPECT_LT(time_ms, 200.0) << "connect_instances should complete in < 200ms for 10 gates";

    deleteTestFile("test_connect.dlib");
    deleteTestFile("test_connect.bench");
}

// Test: Memory allocation patterns (vector reserve)
TEST_F(ProfilingTest, VectorReserveEffectiveness) {
    // This test verifies that vector operations are efficient
    // Actual reserve implementation will be tested in integration tests
    std::vector<std::string> vec;

    // Measure time without reserve
    double time_without_reserve = measureTime([&]() {
        vec.clear();
        for (int i = 0; i < 1000; ++i) {
            vec.push_back("test_string_" + std::to_string(i));
        }
    });

    // Measure time with reserve
    double time_with_reserve = measureTime([&]() {
        vec.clear();
        vec.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            vec.push_back("test_string_" + std::to_string(i));
        }
    });

    // Reserve may improve performance for large vectors, but for small vectors
    // the overhead might be noticeable. The important thing is that both complete successfully.
    // We allow up to 20% overhead for small vectors (1000 elements)
    EXPECT_LE(time_with_reserve, time_without_reserve * 1.2)
        << "reserve should not significantly degrade performance";
}

// Test: Map vs unordered_map performance hint
TEST_F(ProfilingTest, MapPerformanceBaseline) {
    // This test establishes a baseline for map operations
    // Actual unordered_map implementation will be tested in integration tests
    std::map<std::string, int> map_data;

    double time_map = measureTime([&]() {
        for (int i = 0; i < 1000; ++i) {
            map_data["key_" + std::to_string(i)] = i;
        }
        for (int i = 0; i < 1000; ++i) {
            int value = map_data["key_" + std::to_string(i)];
            (void)value;  // Suppress unused variable warning
        }
    });

    // Map operations should complete in reasonable time
    EXPECT_LT(time_map, 10.0) << "Map operations should complete in < 10ms for 1000 entries";
}

// Test: Profiling framework can measure multiple operations
TEST_F(ProfilingTest, ProfilingFrameworkMultipleOperations) {
    std::string dlib_content = "and2 0 y gauss(10.0, 1.0)\nand2 1 y gauss(10.0, 1.0)\n";
    std::string bench_content = "INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = and2(a, b)\n";

    std::string dlib_file = createTestFile("test_prof.dlib", dlib_content);
    std::string bench_file = createTestFile("test_prof.bench", bench_content);

    Ssta ssta;
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);

    double time_read_dlib = measureTime([&]() { ssta.read_dlib(); });

    double time_read_bench = measureTime([&]() { ssta.read_bench(); });

    // Both operations should complete
    EXPECT_GT(time_read_dlib, 0.0) << "read_dlib should take measurable time";
    EXPECT_GT(time_read_bench, 0.0) << "read_bench should take measurable time";

    deleteTestFile("test_prof.dlib");
    deleteTestFile("test_prof.bench");
}

// Test: Large circuit performance (s820.bench scale)
TEST_F(ProfilingTest, LargeCircuitPerformanceBaseline) {
    // Create a larger circuit to simulate s820.bench
    std::ostringstream dlib_content;
    dlib_content << "and2 0 y gauss(10.0, 1.0)\n";
    dlib_content << "and2 1 y gauss(10.0, 1.0)\n";
    dlib_content << "or2 0 y gauss(12.0, 1.5)\n";
    dlib_content << "or2 1 y gauss(12.0, 1.5)\n";

    std::ostringstream bench_content;
    bench_content << "INPUT(a0)\n";
    for (int i = 0; i < 50; ++i) {
        bench_content << "y" << i << " = and2(a" << i << ", a" << i << ")\n";
        if (i < 49) {
            bench_content << "INPUT(a" << (i + 1) << ")\n";
        }
    }
    bench_content << "OUTPUT(y49)\n";

    std::string dlib_file = createTestFile("test_large.dlib", dlib_content.str());
    std::string bench_file = createTestFile("test_large.bench", bench_content.str());

    Ssta ssta;
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);

    double total_time = measureTime([&]() {
        ssta.read_dlib();
        ssta.read_bench();
    });

    // Large circuit should complete in reasonable time (< 1 second)
    EXPECT_LT(total_time, 1000.0) << "Large circuit (50 gates) should complete in < 1 second";

    deleteTestFile("test_large.dlib");
    deleteTestFile("test_large.bench");
}
