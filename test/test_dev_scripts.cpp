// -*- c++ -*-
// Unit tests for development scripts functionality
// Issue #54: 改善: テスト実行のプリセットスクリプト追加

#include <gtest/gtest.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

class DevScriptsTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Find scripts directory dynamically based on current working directory
        // Tests may run from src/ or test/ directory
        char cwd[PATH_MAX];
        std::string current_dir;
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            current_dir = cwd;
        }

        // Try different possible paths relative to current directory
        std::vector<std::string> possible_paths;
        possible_paths.push_back("../scripts");  // From src/ directory
        possible_paths.push_back("scripts");     // From project root
        possible_paths.push_back(
            "../../scripts");  // From test/ directory (if tests run from there)

        // Also try absolute paths if we have current directory
        if (!current_dir.empty()) {
            possible_paths.push_back(current_dir + "/../scripts");
            // If we're in test/ directory, go up two levels
            size_t test_pos = current_dir.find("/test");
            if (test_pos != std::string::npos) {
                std::string parent = current_dir.substr(0, test_pos);
                possible_paths.push_back(parent + "/scripts");
            }
        }

        struct stat info;
        scripts_dir = "";  // Initialize to empty
        for (const auto& path : possible_paths) {
            if (stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
                scripts_dir = path;
                break;
            }
        }

        if (scripts_dir.empty()) {
            GTEST_SKIP()
                << "scripts directory not found (tried: ../scripts, scripts, ../../scripts)";
        }
    }

    void TearDown() override {
        // Cleanup
    }

    bool file_exists(const std::string& path) {
        struct stat info;
        return stat(path.c_str(), &info) == 0;
    }

    bool is_executable(const std::string& path) {
        struct stat info;
        if (stat(path.c_str(), &info) != 0) {
            return false;
        }
        return (info.st_mode & S_IXUSR) != 0;
    }

    std::string scripts_dir;
};

// Test: Verify dev-setup.sh script exists
TEST_F(DevScriptsTest, DevSetupScriptExists) {
    std::string script_path = scripts_dir + "/dev-setup.sh";
    EXPECT_TRUE(file_exists(script_path)) << "dev-setup.sh should exist";
}

// Test: Verify dev-setup.sh is executable
TEST_F(DevScriptsTest, DevSetupScriptIsExecutable) {
    std::string script_path = scripts_dir + "/dev-setup.sh";
    if (file_exists(script_path)) {
        EXPECT_TRUE(is_executable(script_path)) << "dev-setup.sh should be executable";
    }
}

// Test: Verify run-all-checks.sh script exists
TEST_F(DevScriptsTest, RunAllChecksScriptExists) {
    std::string script_path = scripts_dir + "/run-all-checks.sh";
    EXPECT_TRUE(file_exists(script_path)) << "run-all-checks.sh should exist";
}

// Test: Verify run-all-checks.sh is executable
TEST_F(DevScriptsTest, RunAllChecksScriptIsExecutable) {
    std::string script_path = scripts_dir + "/run-all-checks.sh";
    if (file_exists(script_path)) {
        EXPECT_TRUE(is_executable(script_path)) << "run-all-checks.sh should be executable";
    }
}

// Test: Verify generate_coverage.sh script exists (existing script)
TEST_F(DevScriptsTest, GenerateCoverageScriptExists) {
    std::string script_path = scripts_dir + "/generate_coverage.sh";
    EXPECT_TRUE(file_exists(script_path)) << "generate_coverage.sh should exist";
}

// Test: Verify generate_coverage.sh is executable
TEST_F(DevScriptsTest, GenerateCoverageScriptIsExecutable) {
    std::string script_path = scripts_dir + "/generate_coverage.sh";
    if (file_exists(script_path)) {
        EXPECT_TRUE(is_executable(script_path)) << "generate_coverage.sh should be executable";
    }
}

// Test: Verify scripts directory structure
TEST_F(DevScriptsTest, ScriptsDirectoryStructure) {
    EXPECT_TRUE(file_exists(scripts_dir)) << "scripts directory should exist";

    // Check for expected scripts
    EXPECT_TRUE(file_exists(scripts_dir + "/dev-setup.sh"));
    EXPECT_TRUE(file_exists(scripts_dir + "/run-all-checks.sh"));
    EXPECT_TRUE(file_exists(scripts_dir + "/generate_coverage.sh"));
}
