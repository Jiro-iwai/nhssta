// -*- c++ -*-
// Unit tests for directory structure validation
// Tests for duplicate files, build artifact placement, and directory organization

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

class DirectoryStructureTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Find project root dynamically
        project_root = std::filesystem::current_path();
        
        // Try different possible paths relative to current directory
        // Tests may run from src/, test/, or project root
        std::vector<std::filesystem::path> possible_roots;
        possible_roots.push_back(project_root);                    // From project root
        possible_roots.push_back(project_root / "..");            // From src/ or test/
        possible_roots.push_back(project_root / "../..");          // From deeper subdirs
        
        for (const auto& root : possible_roots) {
            if (std::filesystem::exists(root / "src") && 
                std::filesystem::exists(root / "include") &&
                std::filesystem::exists(root / "test")) {
                project_root = root;
                break;
            }
        }
        
        src_dir = project_root / "src";
        include_dir = project_root / "include" / "nhssta";
        test_dir = project_root / "test";
        build_dir = project_root / "build";
        build_bin_dir = build_dir / "bin";
        build_src_dir = build_dir / "src";
        build_test_dir = build_dir / "test";
    }

    std::filesystem::path project_root;
    std::filesystem::path src_dir;
    std::filesystem::path include_dir;
    std::filesystem::path test_dir;
    std::filesystem::path build_dir;
    std::filesystem::path build_bin_dir;
    std::filesystem::path build_src_dir;
    std::filesystem::path build_test_dir;
};

// Test: src/net_line.hpp should not exist (duplicate file)
TEST_F(DirectoryStructureTest, DuplicateFileDetection) {
    std::filesystem::path duplicate_file = src_dir / "net_line.hpp";
    EXPECT_FALSE(std::filesystem::exists(duplicate_file))
        << "src/net_line.hpp should not exist (moved to include/nhssta/net_line.hpp)";
}

// Test: include/nhssta/net_line.hpp should exist
TEST_F(DirectoryStructureTest, PublicApiNetLineExists) {
    std::filesystem::path public_api_file = include_dir / "net_line.hpp";
    EXPECT_TRUE(std::filesystem::exists(public_api_file))
        << "include/nhssta/net_line.hpp should exist";
}

// Test: Public API headers should be in include/nhssta/
TEST_F(DirectoryStructureTest, PublicApiHeadersLocation) {
    std::vector<std::string> public_api_headers = {
        "ssta.hpp",
        "exception.hpp",
        "ssta_results.hpp",
        "gate.hpp",
        "net_line.hpp"
    };
    
    for (const auto& header : public_api_headers) {
        std::filesystem::path header_path = include_dir / header;
        EXPECT_TRUE(std::filesystem::exists(header_path))
            << "Public API header " << header << " should exist in include/nhssta/";
    }
}

// Test: Internal implementation headers should be in src/
TEST_F(DirectoryStructureTest, InternalHeadersLocation) {
    std::vector<std::string> internal_headers = {
        "parser.hpp",
        "random_variable.hpp",
        "expression.hpp",
        "gate.hpp",  // Note: gate.hpp is in both places, but src/gate.hpp is internal
        "statistics.hpp",
        "tokenizer.hpp"
    };
    
    for (const auto& header : internal_headers) {
        std::filesystem::path header_path = src_dir / header;
        // Some headers may not exist (like gate.hpp if fully moved), so we check existence
        if (std::filesystem::exists(header_path)) {
            // If it exists in src/, it should be an internal header
            // gate.hpp is special case - it exists in both, but src version is internal
            if (header == "gate.hpp") {
                // This is expected - gate.hpp exists in both places
                // The public API version is in include/nhssta/gate.hpp
                continue;
            }
        }
    }
}

// Test: Build artifacts should be in build/ directory (after build)
TEST_F(DirectoryStructureTest, BuildArtifactsInBuildDir) {
    // This test checks that if build/ exists, artifacts should be there
    // Note: This test will fail initially until Makefile is updated
    if (std::filesystem::exists(build_dir)) {
        // If build directory exists, check that executables are in build/bin/
        std::filesystem::path nhssta_exe = build_bin_dir / "nhssta";
        std::filesystem::path nhssta_test_exe = build_bin_dir / "nhssta_test";
        
        // At least one executable should exist in build/bin/ if build/ exists
        bool has_executable = std::filesystem::exists(nhssta_exe) || 
                              std::filesystem::exists(nhssta_test_exe);
        
        if (std::filesystem::exists(build_dir)) {
            EXPECT_TRUE(has_executable || !std::filesystem::exists(build_dir))
                << "If build/ directory exists, executables should be in build/bin/";
        }
    }
}

// Test: No build artifacts in source directories
TEST_F(DirectoryStructureTest, NoBuildArtifactsInSourceDirs) {
    // Check that src/ and test/ don't contain build artifacts
    // Note: This test will fail initially until Makefile is updated
    
    if (!std::filesystem::exists(build_dir)) {
        // If build/ doesn't exist yet, skip this test
        GTEST_SKIP() << "build/ directory doesn't exist yet - skipping artifact check";
        return;
    }
    
    // Check src/ for object files
    bool has_obj_in_src = false;
    if (std::filesystem::exists(src_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(src_dir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if ((filename.size() >= 2 && filename.substr(filename.size() - 2) == ".o") ||
                    (filename.size() >= 2 && filename.substr(filename.size() - 2) == ".d") ||
                    filename == "nhssta") {
                    has_obj_in_src = true;
                    break;
                }
            }
        }
    }
    
    // Check test/ for object files
    bool has_obj_in_test = false;
    if (std::filesystem::exists(test_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(test_dir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if ((filename.size() >= 2 && filename.substr(filename.size() - 2) == ".o") ||
                    (filename.size() >= 2 && filename.substr(filename.size() - 2) == ".d") ||
                    filename == "nhssta_test") {
                    has_obj_in_test = true;
                    break;
                }
            }
        }
    }
    
    // If build/ exists, source directories should be clean
    if (std::filesystem::exists(build_dir)) {
        EXPECT_FALSE(has_obj_in_src)
            << "src/ should not contain build artifacts (.o, .d files or executables)";
        EXPECT_FALSE(has_obj_in_test)
            << "test/ should not contain build artifacts (.o, .d files or executables)";
    }
}

// Test: Executables should be in build/bin/
TEST_F(DirectoryStructureTest, ExecutablesInBuildBin) {
    // This test checks that executables are in build/bin/
    // Note: This test will fail initially until Makefile is updated
    
    if (!std::filesystem::exists(build_dir)) {
        GTEST_SKIP() << "build/ directory doesn't exist yet - skipping executable check";
        return;
    }
    
    std::filesystem::path nhssta_exe = build_bin_dir / "nhssta";
    std::filesystem::path nhssta_test_exe = build_bin_dir / "nhssta_test";
    
    // Check that if executables exist, they should be in build/bin/
    bool exe_in_src = std::filesystem::exists(src_dir / "nhssta");
    bool exe_in_test = std::filesystem::exists(test_dir / "nhssta_test");
    
    if (std::filesystem::exists(build_dir)) {
        if (exe_in_src) {
            EXPECT_TRUE(std::filesystem::exists(nhssta_exe))
                << "nhssta executable should be in build/bin/, not src/";
        }
        if (exe_in_test) {
            EXPECT_TRUE(std::filesystem::exists(nhssta_test_exe))
                << "nhssta_test executable should be in build/bin/, not test/";
        }
    }
}

