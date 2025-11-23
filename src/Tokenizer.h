// -*- c++ -*-
// Custom tokenizer to replace boost::tokenizer
// Implements char_separator-like behavior with standard library

#ifndef TOKENIZER__H
#define TOKENIZER__H

#include <string>
#include <vector>
#include <cstring>

class Tokenizer {
public:
    class iterator {
    public:
        iterator() : tokens_(nullptr), index_(0) {}
        iterator(const std::vector<std::string>* tokens, size_t index)
            : tokens_(tokens), index_(index) {}

        const std::string& operator*() const {
            return (*tokens_)[index_];
        }

        const std::string* operator->() const {
            return &(*tokens_)[index_];
        }

        iterator& operator++() {
            ++index_;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++index_;
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return tokens_ == other.tokens_ && index_ == other.index_;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

    private:
        const std::vector<std::string>* tokens_;
        size_t index_;
    };

    Tokenizer(const std::string& str, const std::string& drop_separator, const std::string& keep_separator)
        : tokens_() {
        tokenize(str, drop_separator, keep_separator);
    }

    iterator begin() const {
        return iterator(&tokens_, 0);
    }

    iterator end() const {
        return iterator(&tokens_, tokens_.size());
    }

private:
    void tokenize(const std::string& str, const std::string& drop_separator, const std::string& keep_separator) {
        tokens_.clear();
        if (str.empty()) {
            return;
        }

        std::string current_token;
        size_t i = 0;

        while (i < str.length()) {
            char c = str[i];

            // Check if it's a keep separator
            if (keep_separator.find(c) != std::string::npos) {
                // Save current token if not empty
                if (!current_token.empty()) {
                    tokens_.push_back(current_token);
                    current_token.clear();
                }
                // Add separator as a token
                tokens_.push_back(std::string(1, c));
                ++i;
            }
            // Check if it's a drop separator
            else if (drop_separator.find(c) != std::string::npos) {
                // Save current token if not empty
                if (!current_token.empty()) {
                    tokens_.push_back(current_token);
                    current_token.clear();
                }
                // Skip drop separator
                ++i;
            }
            // Regular character
            else {
                current_token += c;
                ++i;
            }
        }

        // Add final token if not empty
        if (!current_token.empty()) {
            tokens_.push_back(current_token);
        }
    }

    std::vector<std::string> tokens_;
};

#endif // TOKENIZER__H

