// -*- c++ -*-
// Unit tests for std::map to std::unordered_map optimization
// Issue #49: 性能改善: ホットパスのプロファイリングと局所最適化

#include <gtest/gtest.h>
#include <nhssta/ssta.hpp>
#include <nhssta/exception.hpp>
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <map>
#include <unordered_map>

using namespace Nh;

class MapToUnorderedMapTest : public ::testing::Test {
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
    template<typename Func>
    double measureTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0; // Convert to milliseconds
    }

    std::string test_data_dir;
};

// Test: Verify that std::map can be replaced with std::unordered_map for string keys
// This test establishes that string-based maps are good candidates for optimization
TEST_F(MapToUnorderedMapTest, StringKeyMapPerformanceBaseline) {
    // Create a map with string keys (similar to Gates map)
    std::map<std::string, int> map_data;
    
    // Insert many entries
    for (int i = 0; i < 1000; ++i) {
        map_data["gate_" + std::to_string(i)] = i;
    }
    
    // Measure lookup time
    double lookup_time = measureTime([&]() {
        for (int i = 0; i < 1000; ++i) {
            int value = map_data["gate_" + std::to_string(i)];
            (void)value; // Suppress unused variable warning
        }
    });
    
    // Map lookup should complete in reasonable time
    EXPECT_LT(lookup_time, 10.0) << "Map lookup should complete in < 10ms for 1000 entries";
}

// Test: Verify that Gates map operations work correctly
// This ensures we can safely replace std::map with std::unordered_map
TEST_F(MapToUnorderedMapTest, GatesMapOperations) {
    std::string dlib_content = "and2 0 y gauss(10.0, 1.0)\nand2 1 y gauss(10.0, 1.0)\n";
    std::string bench_content = "INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = and2(a, b)\n";
    
    std::string dlib_file = createTestFile("test_gates.dlib", dlib_content);
    std::string bench_file = createTestFile("test_gates.bench", bench_content);
    
    Ssta ssta;
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    
    // read_dlib should populate gates map
    ssta.read_dlib();
    
    // read_bench should use gates map for lookups
    EXPECT_NO_THROW(ssta.read_bench());
    
    deleteTestFile("test_gates.dlib");
    deleteTestFile("test_gates.bench");
}

// Test: Verify that Signals map operations work correctly
TEST_F(MapToUnorderedMapTest, SignalsMapOperations) {
    std::string dlib_content = "and2 0 y gauss(10.0, 1.0)\nand2 1 y gauss(10.0, 1.0)\n";
    std::string bench_content = "INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = and2(a, b)\n";
    
    std::string dlib_file = createTestFile("test_signals.dlib", dlib_content);
    std::string bench_file = createTestFile("test_signals.bench", bench_content);
    
    Ssta ssta;
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    
    // Both operations should work correctly
    EXPECT_NO_THROW({
        ssta.read_dlib();
        ssta.read_bench();
    });
    
    deleteTestFile("test_signals.dlib");
    deleteTestFile("test_signals.bench");
}

// Test: Performance comparison baseline for map vs unordered_map
// This test will be used to verify performance improvement after optimization
TEST_F(MapToUnorderedMapTest, MapVsUnorderedMapPerformanceBaseline) {
    const int num_entries = 1000;
    
    // Measure std::map performance
    std::map<std::string, int> map_data;
    double map_insert_time = measureTime([&]() {
        for (int i = 0; i < num_entries; ++i) {
            map_data["key_" + std::to_string(i)] = i;
        }
    });
    
    double map_lookup_time = measureTime([&]() {
        for (int i = 0; i < num_entries; ++i) {
            int value = map_data["key_" + std::to_string(i)];
            (void)value;
        }
    });
    
    // Measure std::unordered_map performance
    std::unordered_map<std::string, int> unordered_map_data;
    double unordered_map_insert_time = measureTime([&]() {
        for (int i = 0; i < num_entries; ++i) {
            unordered_map_data["key_" + std::to_string(i)] = i;
        }
    });
    
    double unordered_map_lookup_time = measureTime([&]() {
        for (int i = 0; i < num_entries; ++i) {
            int value = unordered_map_data["key_" + std::to_string(i)];
            (void)value;
        }
    });
    
    // Both should complete successfully
    EXPECT_GT(map_insert_time, 0.0) << "Map insert should take measurable time";
    EXPECT_GT(map_lookup_time, 0.0) << "Map lookup should take measurable time";
    EXPECT_GT(unordered_map_insert_time, 0.0) << "Unordered map insert should take measurable time";
    EXPECT_GT(unordered_map_lookup_time, 0.0) << "Unordered map lookup should take measurable time";
    
    // Log performance comparison for analysis
    std::cout << "\n[PERF] std::map insert: " << map_insert_time << " ms" << std::endl;
    std::cout << "[PERF] std::map lookup: " << map_lookup_time << " ms" << std::endl;
    std::cout << "[PERF] std::unordered_map insert: " << unordered_map_insert_time << " ms" << std::endl;
    std::cout << "[PERF] std::unordered_map lookup: " << unordered_map_lookup_time << " ms" << std::endl;
}

// Test: Verify that pair-based maps (like Delays) can potentially use unordered_map
// Note: This requires a hash function for std::pair
TEST_F(MapToUnorderedMapTest, PairKeyMapConsiderations) {
    // For now, we just verify that pair-based maps work correctly
    // Actual optimization will require hash function implementation
    std::map<std::pair<std::string, std::string>, int> pair_map;
    
    pair_map[std::make_pair("input", "output")] = 10;
    pair_map[std::make_pair("input2", "output2")] = 20;
    
    EXPECT_EQ(pair_map[std::make_pair("input", "output")], 10);
    EXPECT_EQ(pair_map[std::make_pair("input2", "output2")], 20);
}

// Test: Large circuit performance with current map implementation
// This establishes a baseline for comparison after optimization
TEST_F(MapToUnorderedMapTest, LargeCircuitMapPerformanceBaseline) {
    std::ostringstream dlib_content;
    dlib_content << "and2 0 y gauss(10.0, 1.0)\n";
    dlib_content << "and2 1 y gauss(10.0, 1.0)\n";
    dlib_content << "or2 0 y gauss(12.0, 1.5)\n";
    dlib_content << "or2 1 y gauss(12.0, 1.5)\n";
    
    std::ostringstream bench_content;
    bench_content << "INPUT(a0)\n";
    for (int i = 0; i < 100; ++i) {
        bench_content << "y" << i << " = and2(a" << i << ", a" << i << ")\n";
        if (i < 99) {
            bench_content << "INPUT(a" << (i + 1) << ")\n";
        }
    }
    bench_content << "OUTPUT(y99)\n";
    
    std::string dlib_file = createTestFile("test_large_map.dlib", dlib_content.str());
    std::string bench_file = createTestFile("test_large_map.bench", bench_content.str());
    
    Ssta ssta;
    ssta.set_dlib(dlib_file);
    ssta.set_bench(bench_file);
    
    double total_time = measureTime([&]() {
        ssta.read_dlib();
        ssta.read_bench();
    });
    
    // Large circuit should complete successfully
    EXPECT_LT(total_time, 2000.0) << "Large circuit (100 gates) should complete in < 2 seconds";
    
    deleteTestFile("test_large_map.dlib");
    deleteTestFile("test_large_map.bench");
}

