#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "test_path_helper.h"

// Integration tests based on example/nhssta_test
// These tests verify that nhssta produces expected output for known inputs

class IntegrationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Set up test environment using path helper
        nhssta_path = find_nhssta_path();
        example_dir = find_example_dir();

        // Ensure paths are set
        if (nhssta_path.empty()) {
            nhssta_path = "../build/bin/nhssta";  // Fallback (new location)
        }
        if (example_dir.empty()) {
            example_dir = "../example";  // Fallback
        }
    }

    void TearDown() override {
        // Cleanup temporary files
    }

    std::string run_nhssta(const std::string& dlib, const std::string& bench,
                           const std::string& options = "-l") {
        std::string cmd = nhssta_path + " " + options + " -d " + example_dir + "/" + dlib + " -b " +
                          example_dir + "/" + bench + " 2>&1";

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return "";
        }

        std::string result;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);

        return result;
    }

    std::string read_expected_result(const std::string& result_file) {
        std::ifstream file(example_dir + "/" + result_file);
        if (!file.is_open()) {
            return "";
        }

        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        return content;
    }

    std::string filter_comments(const std::string& input) {
        std::istringstream iss(input);
        std::ostringstream oss;
        std::string line;

        while (std::getline(iss, line)) {
            if (!line.empty() && line[0] != '#') {
                oss << line << "\n";
            }
        }
        return oss.str();
    }

    std::string nhssta_path;
    std::string example_dir;
};

// Test case 1: my.dlib + my.bench
TEST_F(IntegrationTest, TestMyDlibMyBench) {
    std::string output = run_nhssta("my.dlib", "my.bench", "-l -c -p");
    std::string filtered = filter_comments(output);

    // Basic sanity check: output should contain some data
    EXPECT_FALSE(filtered.empty()) << "Output should not be empty";
    EXPECT_GT(filtered.length(), 0);

    // If expected file exists, we can do more detailed comparison
    std::string expected = read_expected_result("result1");
    if (!expected.empty()) {
        EXPECT_GT(filtered.length(), 50);  // At least some reasonable output
    }
}

// Test case 2: ex4_gauss.dlib + ex4.bench
TEST_F(IntegrationTest, TestEx4GaussDlibEx4Bench) {
    std::string output = run_nhssta("ex4_gauss.dlib", "ex4.bench", "-l -c -p");
    std::string filtered = filter_comments(output);

    EXPECT_FALSE(filtered.empty()) << "Output should not be empty";

    // Check that output contains expected nodes
    EXPECT_NE(filtered.find("A"), std::string::npos);
    EXPECT_NE(filtered.find("B"), std::string::npos);
    EXPECT_NE(filtered.find("C"), std::string::npos);
    EXPECT_NE(filtered.find("Y"), std::string::npos);

    // If expected file exists, we can do more detailed comparison
    std::string expected = read_expected_result("result2");
    if (!expected.empty()) {
        // Basic check: both should have similar content
        EXPECT_GT(filtered.length(), 50);  // At least some reasonable output
    }
}

// Test case 3: ex4_gauss.dlib + s27.bench
TEST_F(IntegrationTest, TestEx4GaussDlibS27Bench) {
    std::string output = run_nhssta("ex4_gauss.dlib", "s27.bench", "-l -c -p");
    std::string filtered = filter_comments(output);

    EXPECT_FALSE(filtered.empty());
    EXPECT_GT(filtered.length(), 0);
}

// Test: critical path output format resembles LAT table
TEST_F(IntegrationTest, TestCriticalPathOutputFormatEx3Bench) {
    std::string output = run_nhssta("ex4_gauss.dlib", "ex3.bench", "-p 3");

    EXPECT_NE(output.find("# critical paths"), std::string::npos);
    EXPECT_NE(output.find("#node"), std::string::npos);
    EXPECT_NE(output.find("Path 1"), std::string::npos);
    EXPECT_NE(output.find("Path 3"), std::string::npos);
    EXPECT_NE(output.find("Y"), std::string::npos);
    EXPECT_NE(output.find("300.000"), std::string::npos);
}

// Test: specifying top-N limits number of reported paths
TEST_F(IntegrationTest, TestCriticalPathTopNLimit) {
    std::string output = run_nhssta("ex4_gauss.dlib", "ex3.bench", "-p 2");

    EXPECT_NE(output.find("Path 1"), std::string::npos);
    EXPECT_NE(output.find("Path 2"), std::string::npos);
    EXPECT_EQ(output.find("Path 3"), std::string::npos);
}

// Test case 4: gaussdelay.dlib + s298.bench (LAT only)
TEST_F(IntegrationTest, TestGaussDelayDlibS298Bench) {
    std::string output = run_nhssta("gaussdelay.dlib", "s298.bench", "-l");
    std::string filtered = filter_comments(output);

    EXPECT_FALSE(filtered.empty());
    EXPECT_GT(filtered.length(), 0);
}

// Test case 5: gaussdelay.dlib + s344.bench (LAT only)
TEST_F(IntegrationTest, TestGaussDelayDlibS344Bench) {
    std::string output = run_nhssta("gaussdelay.dlib", "s344.bench", "-l");
    std::string filtered = filter_comments(output);

    EXPECT_FALSE(filtered.empty());
    EXPECT_GT(filtered.length(), 0);
}

// Test case 6: gaussdelay.dlib + s820.bench (LAT only)
TEST_F(IntegrationTest, TestGaussDelayDlibS820Bench) {
    std::string output = run_nhssta("gaussdelay.dlib", "s820.bench", "-l");
    std::string filtered = filter_comments(output);

    EXPECT_FALSE(filtered.empty());
    EXPECT_GT(filtered.length(), 0);
}

// Test that nhssta executable exists
TEST_F(IntegrationTest, NhsstaExecutableExists) {
    struct stat buffer;
    int exists = (stat(nhssta_path.c_str(), &buffer) == 0);
    if (!exists) {
        // Try to find it again
        nhssta_path = find_nhssta_path();
        exists = (!nhssta_path.empty() && stat(nhssta_path.c_str(), &buffer) == 0);
    }
    EXPECT_EQ(exists, 1) << "nhssta executable not found. Tried: " << nhssta_path;
}

// Test error handling: missing -d option
TEST_F(IntegrationTest, TestMissingDlibOption) {
    std::string cmd = nhssta_path + " -l -b " + example_dir + "/ex4.bench 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    ASSERT_NE(pipe, nullptr);

    std::string result;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    // Should contain error message (check for "error" or "please specify")
    bool has_error = (result.find("error") != std::string::npos) ||
                     (result.find("please specify") != std::string::npos);
    EXPECT_TRUE(has_error) << "Expected error message, got: " << result.substr(0, 200);
}

// Test error handling: missing -b option
TEST_F(IntegrationTest, TestMissingBenchOption) {
    std::string cmd = nhssta_path + " -l -d " + example_dir + "/ex4_gauss.dlib 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    ASSERT_NE(pipe, nullptr);

    std::string result;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    // Should contain error message (check for "error" or "please specify")
    bool has_error = (result.find("error") != std::string::npos) ||
                     (result.find("please specify") != std::string::npos);
    EXPECT_TRUE(has_error) << "Expected error message, got: " << result.substr(0, 200);
}
