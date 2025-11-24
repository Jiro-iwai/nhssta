#ifndef TEST_PATH_HELPER_H
#define TEST_PATH_HELPER_H

#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

// Helper function to find nhssta executable path
// Returns the path if found, empty string otherwise
inline std::string find_nhssta_path() {
    char cwd[PATH_MAX];
    std::string current_dir;
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        current_dir = cwd;
    }

    // Try different possible paths relative to current directory
    // Test binary is typically in test/ directory, so try both test/ and root
    std::vector<std::string> paths;

    // Relative paths from current directory (most common cases)
    paths.push_back("src/nhssta");     // From project root
    paths.push_back("../src/nhssta");  // From test/ directory
    paths.push_back("./src/nhssta");   // Explicit current dir

    // Absolute paths if we have current directory
    if (!current_dir.empty()) {
        paths.push_back(current_dir + "/src/nhssta");
        // If we're in test/ directory, go up one level
        size_t test_pos = current_dir.find("/test");
        if (test_pos != std::string::npos) {
            std::string parent = current_dir.substr(0, test_pos);
            paths.push_back(parent + "/src/nhssta");
        }
        // Also try going up from any directory
        size_t last_slash = current_dir.find_last_of("/");
        if (last_slash != std::string::npos) {
            std::string parent = current_dir.substr(0, last_slash);
            paths.push_back(parent + "/src/nhssta");
        }
    }

    struct stat info;
    for (const auto& path : paths) {
        if (stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode)) {
            // Check if executable
            if (access(path.c_str(), X_OK) == 0) {
                return path;
            }
        }
    }

    return "";
}

// Helper function to find example directory path
inline std::string find_example_dir() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        return "../example";
    }

    std::string current_dir(cwd);

    // Try different possible paths relative to current directory
    std::string paths[] = {"example", "../example", "./example", current_dir + "/example",
                           current_dir + "/../example"};

    struct stat info;
    for (const auto& path : paths) {
        if (stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
            return path;
        }
    }

    return "../example";  // Default fallback
}

#endif  // TEST_PATH_HELPER_H
