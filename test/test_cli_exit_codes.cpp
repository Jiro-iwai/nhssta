// Test for Issue #48: CLI error messages and exit codes
// This test verifies exit codes and error messages for different failure patterns

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "test_path_helper.h"

class CliExitCodeTest : public ::testing::Test {
   protected:
    void SetUp() override {
        nhssta_path = find_nhssta_path();
        if (nhssta_path.empty()) {
            nhssta_path = "../src/nhssta";  // Fallback
        }
        example_dir = find_example_dir();
        if (example_dir.empty()) {
            example_dir = "../example";  // Fallback
        }
    }

    // Run nhssta and return exit code
    int run_nhssta_get_exit_code(const std::string& args) {
        std::string cmd = nhssta_path + " " + args + " > /dev/null 2>&1";
        return system(cmd.c_str());
    }

    // Run nhssta and capture stderr output
    std::string run_nhssta_get_stderr(const std::string& args) {
        std::string cmd = nhssta_path + " " + args + " 2>&1";

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return "";
        }

        std::string result;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        int exit_code = pclose(pipe);
        (void)exit_code;  // Exit code from pclose is not the program exit code

        return result;
    }

    std::string nhssta_path;
    std::string example_dir;
};

// Test: Success case should return exit code 0
TEST_F(CliExitCodeTest, SuccessExitCode) {
    std::string args = "-l -d " + example_dir + "/ex4_gauss.dlib -b " + example_dir + "/ex4.bench";
    int exit_code = run_nhssta_get_exit_code(args);
    EXPECT_EQ(WEXITSTATUS(exit_code), 0) << "Success case should return exit code 0";
}

// Test: Usage error (invalid option) should return exit code 1
TEST_F(CliExitCodeTest, UsageErrorExitCode) {
    std::string args = "-x -d " + example_dir + "/ex4_gauss.dlib -b " + example_dir + "/ex4.bench";
    int exit_code = run_nhssta_get_exit_code(args);
    EXPECT_EQ(WEXITSTATUS(exit_code), 1) << "Usage error should return exit code 1";
}

// Test: Missing -d option should return exit code 1
TEST_F(CliExitCodeTest, MissingDlibOptionExitCode) {
    std::string args = "-l -b " + example_dir + "/ex4.bench";
    int exit_code = run_nhssta_get_exit_code(args);
    EXPECT_EQ(WEXITSTATUS(exit_code), 1) << "Missing -d option should return exit code 1";
}

// Test: Missing -b option should return exit code 1
TEST_F(CliExitCodeTest, MissingBenchOptionExitCode) {
    std::string args = "-l -d " + example_dir + "/ex4_gauss.dlib";
    int exit_code = run_nhssta_get_exit_code(args);
    EXPECT_EQ(WEXITSTATUS(exit_code), 1) << "Missing -b option should return exit code 1";
}

// Test: Non-existent dlib file should return exit code 1 (FileException)
TEST_F(CliExitCodeTest, NonExistentDlibFileExitCode) {
    std::string args = "-l -d nonexistent.dlib -b " + example_dir + "/ex4.bench";
    int exit_code = run_nhssta_get_exit_code(args);
    EXPECT_EQ(WEXITSTATUS(exit_code), 1) << "Non-existent dlib file should return exit code 1";
}

// Test: Non-existent bench file should return exit code 1 (FileException)
TEST_F(CliExitCodeTest, NonExistentBenchFileExitCode) {
    std::string args = "-l -d " + example_dir + "/ex4_gauss.dlib -b nonexistent.bench";
    int exit_code = run_nhssta_get_exit_code(args);
    EXPECT_EQ(WEXITSTATUS(exit_code), 1) << "Non-existent bench file should return exit code 1";
}

// Test: Parse error should return exit code 1 (ParseException)
TEST_F(CliExitCodeTest, ParseErrorExitCode) {
    // Create a temporary invalid dlib file
    std::string temp_dlib = "/tmp/test_parse_error.dlib";
    FILE* f = fopen(temp_dlib.c_str(), "w");
    if (f) {
        fprintf(f, "invalid format line\n");
        fclose(f);
    }

    std::string args = "-l -d " + temp_dlib + " -b " + example_dir + "/ex4.bench";
    int exit_code = run_nhssta_get_exit_code(args);
    EXPECT_EQ(WEXITSTATUS(exit_code), 1) << "Parse error should return exit code 1";

    // Cleanup
    unlink(temp_dlib.c_str());
}

// Test: Runtime error should return exit code 1 (RuntimeException)
TEST_F(CliExitCodeTest, RuntimeErrorExitCode) {
    // This test may need to be adjusted based on actual runtime error scenarios
    // For now, we test that invalid gate causes runtime error
    std::string temp_dlib = "/tmp/test_runtime_error.dlib";
    FILE* f = fopen(temp_dlib.c_str(), "w");
    if (f) {
        fprintf(f, "unknown_gate 0 y gauss (10.0, 2.0)\n");
        fclose(f);
    }

    std::string args = "-l -d " + temp_dlib + " -b " + example_dir + "/ex4.bench";
    int exit_code = run_nhssta_get_exit_code(args);
    EXPECT_EQ(WEXITSTATUS(exit_code), 1) << "Runtime error should return exit code 1";

    // Cleanup
    unlink(temp_dlib.c_str());
}

// Test: Error messages should be clear and informative
TEST_F(CliExitCodeTest, ErrorMessageClarity) {
    std::string output = run_nhssta_get_stderr("-l -b " + example_dir + "/ex4.bench");

    // Should contain error message about -d
    EXPECT_NE(output.find("error"), std::string::npos)
        << "Error message should contain 'error'. Got: " << output.substr(0, 200);
    EXPECT_NE(output.find("-d"), std::string::npos) << "Error message should mention '-d' option";
}

// Test: File not found error message
TEST_F(CliExitCodeTest, FileNotFoundErrorMessage) {
    std::string output =
        run_nhssta_get_stderr("-l -d nonexistent.dlib -b " + example_dir + "/ex4.bench");

    // Should contain error message about file
    EXPECT_NE(output.find("error"), std::string::npos)
        << "File not found should show error message";
}

// Test: Usage message format
TEST_F(CliExitCodeTest, UsageMessageFormat) {
    std::string output = run_nhssta_get_stderr("-h");

    EXPECT_NE(output.find("usage"), std::string::npos) << "Help should show usage message";
    EXPECT_NE(output.find("-d"), std::string::npos) << "Usage should mention -d option";
    EXPECT_NE(output.find("-b"), std::string::npos) << "Usage should mention -b option";
}
