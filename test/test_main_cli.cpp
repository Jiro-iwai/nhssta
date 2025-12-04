#include <gtest/gtest.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "test_path_helper.h"

// Test command-line argument parsing
// This test verifies the behavior of command-line argument parsing
// before replacing boost::spirit with standard library

class CommandLineTest : public ::testing::Test {
   protected:
    void SetUp() override {
        example_dir_ = find_example_dir();
    }

    void TearDown() override {
        // Cleanup
    }
    
    std::string example_path(const std::string& filename) const {
        if (example_dir_.empty()) {
            return filename;
        }
        if (!filename.empty() && filename[0] == '/') {
            return filename;
        }
        return example_dir_ + "/" + filename;
    }

    // Helper to create argc/argv from vector of strings
    struct Argv {
        std::vector<char*> args;
        std::vector<std::string> strings;

        Argv(const std::vector<std::string>& args_vec) {
            strings = args_vec;
            args.reserve(strings.size() + 1);
            for (auto& s : strings) {
                args.push_back(const_cast<char*>(s.c_str()));
            }
            args.push_back(nullptr);
        }

        int argc() const {
            return static_cast<int>(args.size() - 1);
        }
        char** argv() {
            return args.data();
        }
    };

    // Helper to run nhssta with given arguments and capture output
    std::string run_nhssta(const std::vector<std::string>& args) {
        // Find nhssta path dynamically
        std::string nhssta_path = find_nhssta_path();
        if (nhssta_path.empty()) {
            nhssta_path = "../build/bin/nhssta";  // Fallback (new location)
        }

        std::string cmd = nhssta_path;
        for (const auto& arg : args) {
            cmd += " " + arg;
        }
        cmd += " 2>&1";

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

    std::string example_dir_;
};

// Test: help option (-h)
TEST_F(CommandLineTest, TestHelpShort) {
    std::string output = run_nhssta({"-h"});
    EXPECT_NE(output.find("usage"), std::string::npos)
        << "Help output should contain 'usage'. Got: " << output.substr(0, 200);
    EXPECT_NE(output.find("-d"), std::string::npos) << "Help output should contain '-d' option";
    EXPECT_NE(output.find("-b"), std::string::npos) << "Help output should contain '-b' option";
}

// Test: help option (--help)
TEST_F(CommandLineTest, TestHelpLong) {
    std::string output = run_nhssta({"--help"});
    EXPECT_NE(output.find("usage"), std::string::npos) << "Help output should contain 'usage'";
}

// Test: dlib option (-d)
TEST_F(CommandLineTest, TestDlibShort) {
    std::string output = run_nhssta({"-d", "../example/my.dlib", "-b", "../example/my.bench"});
    // Should not show usage, should try to process files
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when valid options provided";
}

// Test: dlib option (--dlib)
TEST_F(CommandLineTest, TestDlibLong) {
    std::string output =
        run_nhssta({"--dlib", "../example/my.dlib", "--bench", "../example/my.bench"});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when valid options provided";
}

// Test: bench option (-b)
TEST_F(CommandLineTest, TestBenchShort) {
    std::string output = run_nhssta({"-d", "../example/my.dlib", "-b", "../example/my.bench"});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when valid options provided";
}

// Test: bench option (--bench)
TEST_F(CommandLineTest, TestBenchLong) {
    std::string output = run_nhssta({"-d", "../example/my.dlib", "--bench", "../example/my.bench"});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when valid options provided";
}

// Test: lat option (-l)
TEST_F(CommandLineTest, TestLatShort) {
    std::string output =
        run_nhssta({"-l", "-d", "../example/my.dlib", "-b", "../example/my.bench"});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when valid options provided";
}

// Test: lat option (--lat)
TEST_F(CommandLineTest, TestLatLong) {
    std::string output =
        run_nhssta({"--lat", "-d", "../example/my.dlib", "-b", "../example/my.bench"});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when valid options provided";
}

// Test: correlation option (-c)
TEST_F(CommandLineTest, TestCorrelationShort) {
    std::string output =
        run_nhssta({"-c", "-d", "../example/my.dlib", "-b", "../example/my.bench"});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when valid options provided";
}

// Test: correlation option (--correlation)
TEST_F(CommandLineTest, TestCorrelationLong) {
    std::string output =
        run_nhssta({"--correlation", "-d", "../example/my.dlib", "-b", "../example/my.bench"});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when valid options provided";
}

// Test: multiple options combined
TEST_F(CommandLineTest, TestMultipleOptions) {
    std::string output =
        run_nhssta({"-l", "-c", "-d", "../example/my.dlib", "-b", "../example/my.bench"});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when valid options provided";
}

// Test: invalid option
TEST_F(CommandLineTest, TestInvalidOption) {
    std::string output =
        run_nhssta({"-x", "-d", "../example/my.dlib", "-b", "../example/my.bench"});
    // Should show usage or error
    EXPECT_NE(output.find("usage"), std::string::npos) << "Should show usage for invalid option";
}

// Test: missing file argument for -d
TEST_F(CommandLineTest, TestMissingDlibFile) {
    std::string output = run_nhssta({"-d", "-b", "../example/my.bench"});
    // Should show usage or error
    EXPECT_NE(output.find("usage"), std::string::npos)
        << "Should show usage when -d is missing file argument";
}

// Test: missing file argument for -b
TEST_F(CommandLineTest, TestMissingBenchFile) {
    std::string output = run_nhssta({"-d", "../example/my.dlib", "-b"});
    // Should show usage or error
    EXPECT_NE(output.find("usage"), std::string::npos)
        << "Should show usage when -b is missing file argument";
}

// Test: empty arguments
TEST_F(CommandLineTest, TestEmptyArguments) {
    std::string output = run_nhssta({});
    // Current implementation may not show usage immediately, but should fail later
    // when required options are missing. Check for error message instead.
    bool has_error = (output.find("error") != std::string::npos) ||
                     (output.find("please specify") != std::string::npos) ||
                     (output.find("usage") != std::string::npos);
    EXPECT_TRUE(has_error) << "Should show error or usage when no arguments provided. Got: "
                           << output.substr(0, 200);
}

// Test: critical path option (-p) without explicit count uses default and prints header
TEST_F(CommandLineTest, TestCriticalPathShortOptionDefault) {
    std::string output =
        run_nhssta({"-p", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when -p is provided without explicit count";
    EXPECT_NE(output.find("critical paths"), std::string::npos)
        << "Critical path header should be printed when -p is enabled";
    EXPECT_NE(output.find("#node"), std::string::npos)
        << "Critical path output should include LAT-style table header";
}

// Test: critical path option (-p N) respects explicit count argument
TEST_F(CommandLineTest, TestCriticalPathShortOptionWithCount) {
    std::string output =
        run_nhssta({"-p", "2", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when -p has a numeric argument";
    EXPECT_NE(output.find("critical paths"), std::string::npos)
        << "Critical path header should be present when -p count is provided";
    EXPECT_NE(output.find("Path 1"), std::string::npos)
        << "At least one critical path should be printed";
    EXPECT_NE(output.find("#node"), std::string::npos)
        << "Critical path output should include LAT-style table header";
}

// Test: critical path option (--path invalid) shows clear error message
TEST_F(CommandLineTest, TestCriticalPathLongOptionInvalidArgument) {
    std::string output = run_nhssta(
        {"--path", "invalid", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    // Should show error message about invalid number format
    EXPECT_NE(output.find("error"), std::string::npos)
        << "Invalid numeric argument should show error message. Got: " << output.substr(0, 300);
    bool has_invalid_message = output.find("Invalid number format") != std::string::npos ||
                                output.find("invalid") != std::string::npos;
    EXPECT_TRUE(has_invalid_message)
        << "Error message should mention invalid number format";
}

// Test: critical path option with negative count should trigger usage (treated as invalid flag)
TEST_F(CommandLineTest, TestCriticalPathNegativeCountShowsUsage) {
    std::string output = run_nhssta(
        {"-p", "-5", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    EXPECT_NE(output.find("usage"), std::string::npos)
        << "Negative count should be treated as invalid flag and show usage";
}

// Test: critical path long option (--path) without explicit count uses default
TEST_F(CommandLineTest, TestCriticalPathLongOptionDefault) {
    std::string output =
        run_nhssta({"--path", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when --path is provided without explicit count";
    EXPECT_NE(output.find("critical paths"), std::string::npos)
        << "Critical path header should be printed when --path is enabled";
}

// Test: critical path long option (--path N) respects explicit count argument
TEST_F(CommandLineTest, TestCriticalPathLongOptionWithCount) {
    std::string output =
        run_nhssta({"--path", "3", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    EXPECT_EQ(output.find("usage"), std::string::npos)
        << "Should not show usage when --path has a numeric argument";
    EXPECT_NE(output.find("critical paths"), std::string::npos)
        << "Critical path header should be present";
    EXPECT_NE(output.find("Path 1"), std::string::npos)
        << "At least one critical path should be printed";
}

// Test: -p and --path produce identical output for the same count
TEST_F(CommandLineTest, TestShortAndLongPathOptionsEquivalent) {
    std::string output_short =
        run_nhssta({"-p", "2", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    std::string output_long =
        run_nhssta({"--path", "2", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    
    // Both should contain critical paths section
    EXPECT_NE(output_short.find("critical paths"), std::string::npos);
    EXPECT_NE(output_long.find("critical paths"), std::string::npos);
    
    // Count the number of "Path" occurrences to verify same number of paths
    auto count_paths = [](const std::string& s) {
        size_t count = 0;
        size_t pos = 0;
        while ((pos = s.find("Path ", pos)) != std::string::npos) {
            count++;
            pos += 5;
        }
        return count;
    };
    EXPECT_EQ(count_paths(output_short), count_paths(output_long))
        << "-p and --path should produce same number of paths for same count";
}

// Test: sensitivity analysis option (-s)
TEST_F(CommandLineTest, TestSensitivityShort) {
    std::string output =
        run_nhssta({"-s", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    
    // Should contain sensitivity analysis section
    EXPECT_NE(output.find("Sensitivity Analysis"), std::string::npos)
        << "Output should contain 'Sensitivity Analysis'. Got: " << output.substr(0, 500);
    EXPECT_NE(output.find("Objective:"), std::string::npos)
        << "Output should contain 'Objective:'";
    EXPECT_NE(output.find("Gate Sensitivities"), std::string::npos)
        << "Output should contain 'Gate Sensitivities'";
}

TEST_F(CommandLineTest, TestSensitivityLong) {
    std::string output =
        run_nhssta({"--sensitivity", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    
    // Should contain sensitivity analysis section
    EXPECT_NE(output.find("Sensitivity Analysis"), std::string::npos)
        << "Output should contain 'Sensitivity Analysis'. Got: " << output.substr(0, 500);
}

// Test: top N option (-n) for sensitivity
TEST_F(CommandLineTest, TestSensitivityTopN) {
    std::string output_n2 =
        run_nhssta({"-s", "-n", "2", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    std::string output_n3 =
        run_nhssta({"-s", "-n", "3", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    
    // Both should contain sensitivity analysis
    EXPECT_NE(output_n2.find("Sensitivity Analysis"), std::string::npos);
    EXPECT_NE(output_n3.find("Sensitivity Analysis"), std::string::npos);
}

// Test: Invalid number format for -p option should show clear error message
TEST_F(CommandLineTest, TestInvalidNumberFormatForPathOption) {
    std::string output = run_nhssta(
        {"-p", "abc", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    
    // Should show error message about invalid number format
    EXPECT_NE(output.find("error"), std::string::npos)
        << "Should show error message for invalid number format. Got: " << output.substr(0, 300);
    bool has_invalid_message = output.find("Invalid number format") != std::string::npos ||
                                output.find("invalid") != std::string::npos ||
                                output.find("abc") != std::string::npos;
    EXPECT_TRUE(has_invalid_message)
        << "Error message should mention invalid number format or the invalid value";
}

// Test: Invalid number format (decimal) for -p option should show clear error message
TEST_F(CommandLineTest, TestInvalidDecimalNumberFormatForPathOption) {
    std::string output = run_nhssta(
        {"-p", "12.34", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    
    // Should show error message about invalid number format
    EXPECT_NE(output.find("error"), std::string::npos)
        << "Should show error message for invalid number format. Got: " << output.substr(0, 300);
}

// Test: Invalid number format for -n option should show clear error message
TEST_F(CommandLineTest, TestInvalidNumberFormatForTopNOption) {
    std::string output = run_nhssta(
        {"-s", "-n", "xyz", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    
    // Should show error message about invalid number format
    EXPECT_NE(output.find("error"), std::string::npos)
        << "Should show error message for invalid number format. Got: " << output.substr(0, 300);
    bool has_invalid_message = output.find("Invalid number format") != std::string::npos ||
                                output.find("invalid") != std::string::npos ||
                                output.find("xyz") != std::string::npos;
    EXPECT_TRUE(has_invalid_message)
        << "Error message should mention invalid number format or the invalid value";
}

// Test: Out of range number for -p option should show clear error message
TEST_F(CommandLineTest, TestOutOfRangeNumberForPathOption) {
    // Use a number that exceeds size_t max value
    std::string output = run_nhssta(
        {"-p", "999999999999999999999999999", "-d", example_path("ex4_gauss.dlib"), "-b", example_path("ex4.bench")});
    
    // Should show error message about number out of range
    EXPECT_NE(output.find("error"), std::string::npos)
        << "Should show error message for out of range number. Got: " << output.substr(0, 300);
    bool has_range_message = output.find("out of range") != std::string::npos ||
                             output.find("range") != std::string::npos;
    EXPECT_TRUE(has_range_message)
        << "Error message should mention out of range";
}
