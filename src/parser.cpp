// -*- c++ -*-
// Authors: IWAI Jiro

#include "parser.hpp"

Parser::Parser(const std::string& file, const char begin_comment, const char* keep_separator,
               const char* drop_separator)
    : file_(file)
    , infile_(file.c_str())
    , drop_separator_(drop_separator != nullptr ? drop_separator : "")
    , keep_separator_(keep_separator != nullptr ? keep_separator : "")
    , begin_comment_(begin_comment) {}

std::istream& Parser::getLine() {
    // Use a loop instead of recursion to avoid stack overflow
    // when there are many consecutive comment or empty lines.
    // Issue #137: The original recursive implementation could cause
    // stack overflow with files containing thousands of comment lines.
    while (true) {
        tokenizer_.reset();

        // Use getline return value for EOF detection (PR #142 review feedback)
        // This ensures we don't try to process a line that couldn't be read,
        // and properly handles files ending with comment/empty lines.
        if (!std::getline(infile_, line_)) {
            // Read failed (including EOF) - create empty tokenizer for consistency
            // This ensures tokenizer_ is never null when callers access end()
            line_ = "";
            tokenizer_ = std::make_unique<Tokenizer>(line_, drop_separator_, keep_separator_);
            token_ = tokenizer_->begin();
            return infile_;
        }

        line_number_++;
        tokenizer_ = std::make_unique<Tokenizer>(line_, drop_separator_, keep_separator_);

        // Check if this is a valid non-comment, non-empty line
        // Note: We use local iterators to avoid leaving token_ pointing to
        // a tokenizer that might be reset in the next iteration (dangling iterator fix)
        auto it_begin = tokenizer_->begin();
        auto it_end = tokenizer_->end();
        bool is_empty_or_comment = (it_begin == it_end) ||
                                    (it_begin != it_end && (*it_begin)[0] == begin_comment_);

        if (!is_empty_or_comment) {
            // Found a valid line - set token_ and return
            token_ = it_begin;
            return infile_;
        }
        // Otherwise, continue to the next line (skip comment/empty lines)
    }
}

void Parser::checkTermination() {
    if (token_ == end()) {
        std::string what = "unexpected termination";
        throw Nh::ParseException(getFileName(), getNumLine(), what);
    }
}

void Parser::unexpectedToken() {
    unexpectedToken_(pre_);
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void Parser::unexpectedToken_(const std::string& token) {
    std::string what = "unexpected token \"";
    what += token;
    what += "\"";
    throw Nh::ParseException(getFileName(), getNumLine(), what);
}

void Parser::checkFile() {
    if (fail()) {
        std::string what = "failed to open file";
        throw Nh::FileException(getFileName(), what);
    }
}

void Parser::checkSeparator(char separator) {
    checkTermination();
    if ((*token_)[0] != separator) {
        unexpectedToken_(*token_);
    }
    pre_ = *token_++;
}

void Parser::checkEnd() {
    if (token_ != end()) {
        unexpectedToken_(*token_);
    }
}
