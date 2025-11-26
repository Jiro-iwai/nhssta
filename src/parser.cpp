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

        // Check if we should return (EOF or valid non-comment line)
        // Note: We check the tokenizer state locally to avoid leaving token_
        // pointing to a tokenizer that might be reset in the next iteration.
        auto it_begin = tokenizer_->begin();
        auto it_end = tokenizer_->end();
        bool is_empty_or_comment = (it_begin == it_end) || 
                                    (it_begin != it_end && (*it_begin)[0] == begin_comment_);

        if (infile_.eof() || !is_empty_or_comment) {
            // Set token_ only when we're about to return, ensuring it points
            // to the current (and final) tokenizer.
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
