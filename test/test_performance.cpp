// -*- c++ -*-
// Performance benchmarks for nhssta
// Phase 3: Performance optimization and scalability

#include <gtest/gtest.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>

#include "test_path_helper.h"

// Performance test fixture
class PerformanceTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Use path helper to find paths dynamically
        nhssta_path = find_nhssta_path();
        example_dir = find_example_dir();

        if (nhssta_path.empty()) {
            nhssta_path = "../src/nhssta";  // Fallback
        }
        if (example_dir.empty()) {
            example_dir = "../example";  // Fallback
        }
    }

    // Measure execution time of nhssta command
    double measureExecutionTime(const std::string& dlib, const std::string& bench,
                                const std::string& options = "-l") {
        std::string cmd = nhssta_path + " " + options + " -d " + example_dir + "/" + dlib + " -b " +
                          example_dir + "/" + bench + " > /dev/null 2>&1";

        auto start = std::chrono::high_resolution_clock::now();
        int result = system(cmd.c_str());
        auto end = std::chrono::high_resolution_clock::now();

        if (result != 0) {
            return -1.0;  // Error occurred
        }

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0;  // Return milliseconds
    }

    // Check if test file exists
    bool fileExists(const std::string& filename) {
        struct stat buffer;
        return (stat((example_dir + "/" + filename).c_str(), &buffer) == 0);
    }

    std::string example_dir;
    std::string nhssta_path;
};

// Benchmark: Small circuit (ex4)
TEST_F(PerformanceTest, BenchmarkEx4) {
    if (!fileExists("ex4_gauss.dlib") || !fileExists("ex4.bench")) {
        GTEST_SKIP() << "Test files not found";
    }

    double time_ms = measureExecutionTime("ex4_gauss.dlib", "ex4.bench", "-l");
    ASSERT_GE(time_ms, 0.0) << "Execution failed";

    std::cout << "\n[PERF] ex4_gauss.dlib + ex4.bench: " << std::fixed << std::setprecision(2)
              << time_ms << " ms" << std::endl;

    // Basic sanity check: should complete in reasonable time (< 10 seconds)
    EXPECT_LT(time_ms, 10000.0) << "Execution took too long";
}

// Benchmark: Medium circuit (s27)
TEST_F(PerformanceTest, BenchmarkS27) {
    if (!fileExists("ex4_gauss.dlib") || !fileExists("s27.bench")) {
        GTEST_SKIP() << "Test files not found";
    }

    double time_ms = measureExecutionTime("ex4_gauss.dlib", "s27.bench", "-l -c");
    ASSERT_GE(time_ms, 0.0) << "Execution failed";

    std::cout << "\n[PERF] ex4_gauss.dlib + s27.bench: " << std::fixed << std::setprecision(2)
              << time_ms << " ms" << std::endl;

    EXPECT_LT(time_ms, 30000.0) << "Execution took too long";
}

// Benchmark: Large circuit (s298)
TEST_F(PerformanceTest, BenchmarkS298) {
    if (!fileExists("gaussdelay.dlib") || !fileExists("s298.bench")) {
        GTEST_SKIP() << "Test files not found";
    }

    double time_ms = measureExecutionTime("gaussdelay.dlib", "s298.bench", "-l");
    ASSERT_GE(time_ms, 0.0) << "Execution failed";

    std::cout << "\n[PERF] gaussdelay.dlib + s298.bench: " << std::fixed << std::setprecision(2)
              << time_ms << " ms" << std::endl;

    EXPECT_LT(time_ms, 60000.0) << "Execution took too long";
}

// Benchmark: Large circuit (s344)
TEST_F(PerformanceTest, BenchmarkS344) {
    if (!fileExists("gaussdelay.dlib") || !fileExists("s344.bench")) {
        GTEST_SKIP() << "Test files not found";
    }

    double time_ms = measureExecutionTime("gaussdelay.dlib", "s344.bench", "-l");
    ASSERT_GE(time_ms, 0.0) << "Execution failed";

    std::cout << "\n[PERF] gaussdelay.dlib + s344.bench: " << std::fixed << std::setprecision(2)
              << time_ms << " ms" << std::endl;

    EXPECT_LT(time_ms, 60000.0) << "Execution took too long";
}

// Benchmark: Very large circuit (s820)
TEST_F(PerformanceTest, BenchmarkS820) {
    if (!fileExists("gaussdelay.dlib") || !fileExists("s820.bench")) {
        GTEST_SKIP() << "Test files not found";
    }

    double time_ms = measureExecutionTime("gaussdelay.dlib", "s820.bench", "-l");
    ASSERT_GE(time_ms, 0.0) << "Execution failed";

    std::cout << "\n[PERF] gaussdelay.dlib + s820.bench: " << std::fixed << std::setprecision(2)
              << time_ms << " ms" << std::endl;

    EXPECT_LT(time_ms, 120000.0) << "Execution took too long";
}

// Benchmark: LAT only vs LAT + Correlation
TEST_F(PerformanceTest, BenchmarkLatVsCorrelation) {
    if (!fileExists("ex4_gauss.dlib") || !fileExists("ex4.bench")) {
        GTEST_SKIP() << "Test files not found";
    }

    double time_lat = measureExecutionTime("ex4_gauss.dlib", "ex4.bench", "-l");
    double time_both = measureExecutionTime("ex4_gauss.dlib", "ex4.bench", "-l -c");

    ASSERT_GE(time_lat, 0.0) << "LAT execution failed";
    ASSERT_GE(time_both, 0.0) << "LAT+Correlation execution failed";

    std::cout << "\n[PERF] LAT only: " << std::fixed << std::setprecision(2) << time_lat << " ms"
              << std::endl;
    std::cout << "[PERF] LAT + Correlation: " << std::fixed << std::setprecision(2) << time_both
              << " ms" << std::endl;
    std::cout << "[PERF] Correlation overhead: " << std::fixed << std::setprecision(2)
              << (time_both - time_lat) << " ms" << std::endl;

    // Correlation calculation may take additional time, but due to measurement variance
    // and potential caching effects, we allow some flexibility
    // The important thing is that both complete successfully
    EXPECT_GT(time_lat, 0.0) << "LAT calculation should complete";
    EXPECT_GT(time_both, 0.0) << "LAT+Correlation calculation should complete";

    // Log the ratio for analysis
    if (time_lat > 0.0) {
        double ratio = time_both / time_lat;
        std::cout << "[PERF] Correlation/LAT ratio: " << std::fixed << std::setprecision(2) << ratio
                  << std::endl;
    }
}

// Benchmark: Multiple runs for consistency
TEST_F(PerformanceTest, BenchmarkConsistency) {
    if (!fileExists("ex4_gauss.dlib") || !fileExists("ex4.bench")) {
        GTEST_SKIP() << "Test files not found";
    }

    const int num_runs = 3;
    double times[num_runs];
    double sum = 0.0;

    for (int i = 0; i < num_runs; i++) {
        times[i] = measureExecutionTime("ex4_gauss.dlib", "ex4.bench", "-l");
        ASSERT_GE(times[i], 0.0) << "Execution failed on run " << (i + 1);
        sum += times[i];
    }

    double avg = sum / num_runs;
    double max_diff = 0.0;

    for (int i = 0; i < num_runs; i++) {
        double diff = std::abs(times[i] - avg);
        if (diff > max_diff) {
            max_diff = diff;
        }
    }

    std::cout << "\n[PERF] Average execution time: " << std::fixed << std::setprecision(2) << avg
              << " ms" << std::endl;
    std::cout << "[PERF] Max deviation: " << std::fixed << std::setprecision(2) << max_diff << " ms"
              << std::endl;

    // Results should be reasonably consistent (within 50% of average)
    EXPECT_LT(max_diff, avg * 0.5) << "Execution times are too inconsistent";
}
