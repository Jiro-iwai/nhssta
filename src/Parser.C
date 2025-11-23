// -*- c++ -*-
// Authors: IWAI Jiro

#include "Parser.h"

Parser::Parser(
    const std::string& file,
    const char begin_comment,
    const char* keep_separator,
    const char* drop_separator
    ) :
    line_number_(0),
    file_(file),
    infile_(file.c_str()),
    drop_separator_(drop_separator ? drop_separator : ""),
    keep_separator_(keep_separator ? keep_separator : ""),
    begin_comment_(begin_comment),
    tokenizer_(0)
{
}

std::istream& Parser::getLine() {
    delete tokenizer_;
    std::istream& r = std::getline(infile_,line_); // not support Mac. "\r"
    line_number_++;
    tokenizer_ = new Tokenizer(line_, drop_separator_, keep_separator_);
    if( ( begin() == end() || (*begin())[0] == begin_comment_ ) && !r.eof() )
        return getLine();
    return (r);
}

void Parser::checkTermination() {
    if( token_ == end() ) {
        std::string what = "unexpected termination";
        throw Nh::ParseException(getFileName(), getNumLine(), what);
    }
}

void Parser::unexpectedToken(){
    unexpectedToken_(pre_);
}

void Parser::unexpectedToken_(const std::string& token){
    std::string what = "unexpected token \"";
    what += token;
    what += "\"";
    throw Nh::ParseException(getFileName(), getNumLine(), what);
}

void Parser::checkFile(){
    if( fail() ) {
        std::string what = "failed to open file";
        throw Nh::FileException(getFileName(), what);
    }
}

void Parser::checkSepalator(char sepalator) {
    checkTermination();
    if( (*token_)[0] != sepalator )
        unexpectedToken_(*token_);
    pre_ = *token_++;
}

void Parser::checkEnd(){
    if( token_ != end() )
        unexpectedToken_(*token_);
}
