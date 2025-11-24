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
        // Set up test environment
    }

    void TearDown() override {
        // Cleanup
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
            nhssta_path = "../src/nhssta";  // Fallback
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
