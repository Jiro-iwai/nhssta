// -*- c++ -*-
// Author: IWAI Jiro (refactored)
//
// Unified exception class for nhssta
// Phase 2: Design cleanup - Error handling unification

#ifndef NH_EXCEPTION__H
#define NH_EXCEPTION__H

#include <string>
#include <stdexcept>

namespace Nh {

    // Base exception class for all nhssta exceptions
    class Exception : public std::exception {
    public:
        Exception(const std::string& message) : message_(message) {}
        Exception(const std::string& context, const std::string& message) 
            : message_(context + ": " + message) {}
        
        ~Exception() noexcept = default;
        
        [[nodiscard]] const char* what() const noexcept override {
            return message_.c_str();
        }
        
        [[nodiscard]] const std::string& message() const { return message_; }

    protected:
        std::string message_;
    };

    // Exception categories for better error handling
    class FileException : public Exception {
    public:
        FileException(const std::string& filename, const std::string& message)
            : Exception("File error", filename + ": " + message) {}
    };

    class ParseException : public Exception {
    public:
        ParseException(const std::string& filename, int line, const std::string& message)
            : Exception("Parse error", filename + ":" + std::to_string(line) + ": " + message) {}
    };

    class ConfigurationException : public Exception {
    public:
        ConfigurationException(const std::string& message)
            : Exception("Configuration error", message) {}
    };

    class RuntimeException : public Exception {
    public:
        RuntimeException(const std::string& message)
            : Exception("Runtime error", message) {}
    };

}

#endif // NH_EXCEPTION__H

