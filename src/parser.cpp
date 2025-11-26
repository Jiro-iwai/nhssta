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
        std::getline(infile_, line_);  // not support Mac. "\r"
        line_number_++;
        tokenizer_ = std::make_unique<Tokenizer>(line_, drop_separator_, keep_separator_);

        // If we reached EOF or found a non-empty, non-comment line, return
        if (infile_.eof() || (begin() != end() && (*begin())[0] != begin_comment_)) {
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
