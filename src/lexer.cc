#include "ast.hh"

#include <array>
#include <cstring>
#include <utility>

Lexer& Lexer::feed(const std::string& code) {
    lineno = 1;
    current = 0;
    this->code = code;
    return *this;
}

///////////////////////////////////////////////////////////////

// valid operator characters
static const char* OperatorChars = "+-*/%.:=<>|&^";

// language keywords
static const std::vector<std::string> Keywords = {
    KeywordSwitch, KeywordCase, KeywordWhen,
    KeywordIf, KeywordElse, KeywordThen,
    KeywordDeclare, KeywordImport, KeywordRef,
    KeywordReturn, KeywordFunction
};

// language operators
static const std::vector<std::string> Operators = {
    // match operators
    "+", "-", "*", "/", "%",
    // binary operators
    "<<", ">>", "&", "^", "|",
    // assignment / access
    ".", "=", ":=", "->", "...",
    // comparison operators
    ">", "<", ">=", "<=", "==", "!=", "&&", "||"
};

// check if char is an operator
static inline bool is_operator(const char c) {
    return std::strchr(OperatorChars, c) != nullptr;
}

// check if char is not a string delimiter
static inline bool is_not_str(const char c) {
    return c != '"';
}

// check if char is a digit
static inline bool is_digit(const char c) {
    return (c >= '0' && c <= '9');
}

// check if char is numeric (decimal point or digit)
static inline bool is_numeric(const char c) {
    return is_digit(c) || c == '.';
}

// check if char is valid identifier start
static inline bool is_ident_start(const char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        || c == '_' || c == '$';
}

// check if char is an identifier
static inline bool is_ident(const char c) {
    return is_ident_start(c) || is_digit(c);
}

// check if char is a whitespace
static inline bool is_whitespace(const char c) {
    switch (c) {
        case ' ': case '\n': case '\t': case '\r': return true;
        default: return false;
    }
}

// check if char is valid grammar
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

// check if string is from a list of strings
static inline bool is_from(const std::vector<std::string>& group, const std::string& text) {
    for (const std::string& str : group)
        if (text == str)
            return true;
    return false;
}

// check if lexer still has content
#define is_valid(lexer) \
    ((lexer).current < (lexer).code.size())

// get current char of lexer
#define lex_char(lexer) \
    ((lexer).code[(lexer).current])

// read until condition
#define read_until(lexer, check) \
    register std::size_t size = 0;                       \
    std::size_t start = (lexer).current;                 \
    const std::size_t lineno = lexer.lineno;             \
    while (is_valid(lexer) && check(lex_char(lexer))) {  \
        size++;                                          \
        (lexer).current++;                               \
    }

// get token text string
#define token_str(lexer) \
    std::string((lexer).code.c_str() + start, size)

// parse a string
static inline Token parse_string(Lexer& lexer) {
    lexer.current++;
    read_until(lexer, is_not_str)
    lexer.current++;
    return Token(String, token_str(lexer), start, lineno);
}

// parse an identifier
static inline Token parse_ident(Lexer& lexer) {
    TokenType type = Ident;
    read_until(lexer, is_ident)
    std::string text = token_str(lexer);
    if (is_from(Keywords, text))
        type = Keyword;
    return Token(type, text, start, lineno);
}

// parse a number
static inline Token parse_number(Lexer& lexer) {
    read_until(lexer, is_numeric)
    std::string text = token_str(lexer);
    if (strcount(text, '.') > 1)
        throw ParserError::from(lexer, start, lineno,
            "Invalid float literal %s", text.c_str());
    return Token(Number, text, start, lineno);
}

// parse an operator
static inline Token parse_operator(Lexer& lexer) {
    TokenType type = Operator;
    read_until(lexer, is_operator)
    std::string text = token_str(lexer);
    if (text == "->") type = Arrow;
    if (!is_from(Operators, text))
        throw ParserError::from(lexer, start, lineno,
            "Invalid operator %s", text.c_str());
    return Token(type, text, start, lineno);
}

// parse a grammar character
static inline Token parse_grammar(Lexer& lexer) {
    TokenType type = None;
    const std::size_t size = 1;
    const std::size_t start = lexer.current;
    switch (lexer.code[lexer.current++]) {
        case '(': type = LParen; break;
        case ')': type = RParen; break;
        case '{': type = LCurly; break;
        case '}': type = RCurly; break;
        case '[': type = LBracket; break;
        case ']': type = RBracket; break;
        case ',': type = Comma; break;
        case ';': type = Semicolon; break;
        default: break;
    }
    return Token(type, token_str(lexer), start, lexer.lineno);
}

// parse next token
Token Lexer::next() {
    // skip whitespace / lines
    while (is_valid(*this) && is_whitespace(lex_char(*this)))
        if (code.at(current++) == '\n')
            lineno++;
    
    // no more tokens
    if (!is_valid(*this))
        return Token(Eof);

    // parse the current token
    if (lex_char(*this) == '"')
        return std::move(parse_string(*this));
    if (is_digit(lex_char(*this)))
        return std::move(parse_number(*this));
    if (is_operator(lex_char(*this)))
        return std::move(parse_operator(*this));
    if (is_ident_start(lex_char(*this)))
        return std::move(parse_ident(*this));
    if (is_grammar(lex_char(*this)))
        return std::move(parse_grammar(*this));

    // invalid character found
    throw ParserError::from(*this, current, lineno,
        "Invalid char: %c", code[current]);

    return Token();
}