#pragma once
#include "ast.hh"

#include <memory>
#include <cstdio>
#include <sstream>
#include <cstring>
#include <functional>

static const char* TokenTypeMap[] = {
    "None", "Eof", "Ident", "String", "Number",
    "Keyword", "Operator", "LParen", "RParen",
    "LCurly", "RCurly", "LBracket", "RBracket",
    "Comma", "Arrow", "Semicolon"
};

const char* token_type_str(TokenType type) {
    return TokenTypeMap[type];
}

// sformat impl
template <typename ...Args>
std::string sformat(const std::string& format, Args... args) {
    std::size_t size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

// create a parser error
template <typename ...Args>
ParserError parser_error(const Lexer& lexer,
    const std::size_t& lineno, std::size_t start,
    const std::string& format, Args... args)
{
    // find start line of error
    while (start > 0 && lexer.code[start] != '\n')
        start--;

    // find end line of error
    std::size_t end = lexer.code.find('\n', start + 1);
    if (end == std::string::npos) end = lexer.code.size();

    // create error text
    std::string err = sformat("Error on line %lu: %.*s\n  %s\n",
        lineno, (int)(end - start), lexer.code.c_str() + start,
        sformat(format, args...).c_str());
    
    return ParserError(err);
}

// set lexer input
Lexer& Lexer::feed(const std::string& _code) {
    lineno = 1;
    current = 0;
    code = _code;
    return *this;
}

// language defs

static const char* OperatorChars = "+-*/%.:=<>|&^";

static const std::vector<std::string> Keywords = {
    "switch", "case", "when",
    "func", "return", "open",
    "let", "if", "else", "then"
};

static const std::vector<std::string> Operators = {
    // assignment / access
    ".", "=", ":=", "->",
    // match operators
    "+", "-", "*", "/", "%",
    // binary operators
    "<<", ">>", "&", "^", "|",
    // comparison operators
    ">", "<", ">=", "<=", "==", "!=", "&&", "||"
};

// macro helpers

#define is_valid(lexer) \
    ((lexer).current < (lexer).code.size())

#define lex_char(lexer) \
    ((lexer).code[(lexer).current])

#define init_parser(lexer, _type)  \
    std::size_t size;              \
    token.type = (_type);          \
    token.start = (lexer).current; \
    token.lineno = (lexer).lineno

#define complete_parser() \
    token.text = std::string(lexer.code.c_str() + token.start, size)

// parser helpers

static inline bool is_operator(const char c) {
    return std::strchr(OperatorChars, c) != nullptr;
}
static inline bool is_not_str(const char c) {
    return c != '"';
}
static inline bool is_digit(const char c) {
    return (c >= '0' && c <= '9');
}
static inline bool is_numeric(const char c) {
    return is_digit(c) || c == '.';
}
static inline bool is_ident_start(const char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        || c == '_' || c == '$';
}
static inline bool is_ident(const char c) {
    return is_ident_start(c) || is_digit(c);
}
static inline bool is_whitespace(const char c) {
    switch (c) {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
            return true;
        default:
            return false;
    }
}
static inline bool is_grammar(const char c) {
    switch (c) {
        case '(':
        case ')':
        case '{':
        case '}':
        case '[':
        case ']':
        case ',':
        case ';':
            return true;
        default:
            return false;
    }
}

// count occcurences of char in string
std::size_t strcount(const std::string& str, const char c) {
    std::size_t pos = 0, count = 0;
    while ((pos = str.find(c, pos)) < str.size()) {
        count++;
        pos++;
    }
    return count;
}

// check if string in vector
static inline bool is_from(
    const std::vector<std::string>& group,
    const std::string& text)
{
    for (const std::string& str : group)
        if (str == text)
            return true;
    return false;
}

// read token until check condition
static inline void read_until(
    Lexer& lexer,
    std::size_t* start, std::size_t* size,
    const std::function<bool(const char& c)>& check)
{
    *size = 0;
    *start = lexer.current;
    while (is_valid(lexer) && check(lex_char(lexer))) {
        (*size)++;
        lexer.current++;
    }
}

// parser functions

// parse a string
static inline Token& parse_string(Lexer& lexer, Token& token) {
    lexer.current++;
    init_parser(lexer, String);
    read_until(lexer, &token.start, &size, is_not_str);
    lexer.current++;
    complete_parser();
    return token;
}

// parse a keyword or identifier
static inline Token& parse_ident(Lexer& lexer, Token& token) {
    init_parser(lexer, Ident);
    read_until(lexer, &token.start, &size, is_ident);
    complete_parser();
    if (is_from(Keywords, token.text))
        token.type = Keyword;
    return token;
}

// parse a number
static inline Token& parse_number(Lexer& lexer, Token& token) {
    init_parser(lexer, Number);
    read_until(lexer, &token.start, &size, is_numeric);
    complete_parser();
    if (strcount(token.text, '.') > 1)
        throw parser_error(lexer, token.lineno, token.start,
            "Invalid float literal %s", token.text.c_str());
    return token;
}

// parse an operator
static inline Token& parse_operator(Lexer& lexer, Token& token) {
    init_parser(lexer, Operator);
    read_until(lexer, &token.start, &size, is_operator);
    complete_parser();
    if (token.text == "->")
        token.type = Arrow;
    if (!is_from(Operators, token.text))
        throw parser_error(lexer, token.lineno, token.start,
            "Invalid operator %s", token.text.c_str());
    return token;
}

// parse a grammar character
static inline Token& parse_grammar(Lexer& lexer, Token& token) {
    init_parser(lexer, None);
    size = 1;
    switch (lexer.code[lexer.current++]) {
        case '(': token.type = LParen; break;
        case ')': token.type = RParen; break;
        case '{': token.type = LCurly; break;
        case '}': token.type = RCurly; break;
        case '[': token.type = LBracket; break;
        case ']': token.type = RBracket; break;
        case ',': token.type = Comma; break;
        case ';': token.type = Semicolon; break;
        default: break;
    }
    complete_parser();
    return token;
}

Token Lexer::next() {
    Token token(Eof);
    
    // skip whitespace / lines
    while (is_valid(*this) && is_whitespace(lex_char(*this)))
        if (code.at(current++) == '\n')
            lineno++;
    
    // no more tokens
    if (!is_valid(*this))
        return token;

    // parse the current token
    if (lex_char(*this) == '"')
        return parse_string(*this, token);
    if (is_digit(lex_char(*this)))
        return parse_number(*this, token);
    if (is_operator(lex_char(*this)))
        return parse_operator(*this, token);
    if (is_ident_start(lex_char(*this)))
        return parse_ident(*this, token);
    if (is_grammar(lex_char(*this)))
        return parse_grammar(*this, token);

    // invalid character found
    throw parser_error(*this, lineno, current,
        "Invalid char: %c", code[current]);
    return token;
}