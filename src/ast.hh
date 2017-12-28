#pragma once

#include <map>
#include <any>
#include <queue>
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

// token types
typedef enum {
    None      = 0,
    Eof       = 1,
    Ident     = 2,
    String    = 3,
    Number    = 4,
    Keyword   = 5,
    Operator  = 6,
    LParen    = 7,
    RParen    = 8,
    LCurly    = 9,
    RCurly    = 10,
    LBracket  = 11,
    RBracket  = 12,
    Comma     = 13,
    Arrow     = 14,
    Semicolon = 15
} TokenType;

const char* token_type_str(TokenType type);

// token object
class Token {
public:
    TokenType type;
    std::string text;
    std::size_t start;
    std::size_t lineno;

    Token() : Token(None) {}
    Token(TokenType _type) : type(_type) {}
    Token(TokenType _type, const std::string& _text)
        : type(_type), text(_text) {}

    bool is(TokenType _type) const {
        return type == _type;
    }
    bool is(const std::string& str) const {
        return text == str;
    }

    operator bool() const {
        switch (type) {
            case None:
            case Eof:
                return false;
            default:
                return true;
        }
    }
};

// lexer tracking class
class Lexer {
public:
    std::string code;
    std::size_t lineno;
    std::size_t current;

    Lexer() = default;
    Token next();
    Lexer& feed(const std::string& code);
};

// custom parser error
class ParserError : public std::exception {
private:
    std::string message;
public:
    ParserError(const std::string& msg) : message(msg) {}
    const char* what() const throw() { return message.c_str(); } ;
};

// count occurances of char in string
std::size_t strcount(const std::string& str, const char c);

// pythons string.format using sprintf
template <typename ...Args>
std::string sformat(const std::string& format, Args... args);

// raise a formatted parser error
template <typename ... Args>
ParserError parser_error(const Lexer& lexer,
    const std::size_t& lineno, std::size_t start,
    const std::string& fmt, Args... args);

// expression types
typedef enum {
    ExprUnop,
    ExprBinop,
    ExprConst,
    ExprCall,
    ExprFunction,
    ExprReturn,
    ExprBlock,
    ExprIf,
    ExprCase,
    ExprSwitch,
    ExprCaseCond,
} ExprType;

// base class expression
#define ExprPtr Expr*
class Expr {
public:
    Token token;
    ExprType type;
    Expr(ExprType _type, Token _token = Token(None))
        : token(_token), type(_type) {}

    static void free(ExprPtr* e) {
        delete *e;
        *e = nullptr; 
    }

    static void free_list(std::vector<ExprPtr>& list) {
        for (ExprPtr e : list)
            Expr::free(&e);
        list.clear();
    }
};

/// ----------------------
//  Expression definitions
/// -----------------------

#define SingleExpr(Name, Type)                             \
    class Name : public Expr {                             \
    public:                                                \
        ExprPtr value;                                     \
        ~Name() { Expr::free(&value); }                     \
        Name(const Token& token, ExprPtr _value = nullptr) \
            : Expr(Type, token), value(_value) {}          \
    }

#define ListExpr(Name, Type, list)                      \
    class Name : public Expr {                          \
    public:                                             \
        std::string name;                               \
        std::vector<ExprPtr> list;                      \
        ~Name() { Expr::free_list(list); }              \
        Name(const Token& token)                        \
            : Expr(Type, token), name(token.text) {}    \
    }

#define DoubleExpr(Name, Type, first, second) \
    class Name : public Expr {                \
    public:                                   \
        ExprPtr first;                        \
        ExprPtr second;                       \
        ~Name() {                             \
            Expr::free(&first);               \
            Expr::free(&second);              \
        }                                     \
        Name(const Token& token,              \
            ExprPtr _arg1 = nullptr,          \
            ExprPtr _arg2 = nullptr)          \
            : Expr(Type, token),              \
            first(_arg1), second(_arg2) {}    \
    }

SingleExpr(Unop, ExprUnop);

DoubleExpr(Binop, ExprBinop, left, right);

SingleExpr(Return, ExprReturn);

ListExpr(Call, ExprCall, args);

ListExpr(Block, ExprBlock, body);

ListExpr(Switch, ExprSwitch, cases);

DoubleExpr(Case, ExprCase, body, condition);

DoubleExpr(CaseCondition, ExprCaseCond, value, condition);

// constant expression types
typedef enum {
    ConstInt,
    ConstNull,
    ConstThis,
    ConstFloat,
    ConstIdent,
    ConstString
} ConstType;

class Const : public Expr {
public:
    ConstType const_type;
    std::any value;
    Const(const Token& token);
};

class Function : public Expr {
public:
    ExprPtr body;
    std::string name;
    std::vector<ExprPtr> args;

    ~Function() {
        Expr::free(&body);
        Expr::free_list(args);
    }

    Function(const Token& token,
        const std::string& _name,
        ExprPtr _body = nullptr)
        : Expr(ExprFunction, token),
        body(_body), name(_name) {}
};

class If : public Expr {
public:
    ExprPtr body;
    ExprPtr expr_else;
    ExprPtr condition;

    ~If() {
        Expr::free(&expr_else);
        Expr::free(&condition);
        Expr::free(&body);
    }

    If(const Token& token,
        ExprPtr _else = nullptr,
        ExprPtr _cond = nullptr,
        ExprPtr _body = nullptr)
        : Expr(ExprIf, token),
        body(_body), expr_else(_else), condition(_cond) {}
};

/// Parser definition

class Parser {
private:
    Token consume_error(TokenType, const std::string&, bool, bool);

public:
    Lexer lexer;
    Token current;
    std::queue<Token> peeks;

    Parser();

    Token next();

    Token peek();

    ExprPtr parse(const std::string& code);

    Token consume(TokenType, bool maybe = false);
    Token consume(const std::string&, bool maybe = false);
    Token consume(TokenType, const std::string&, bool maybe = false, bool has_type = true);

    template <typename ...Args>
    ParserError error(const Token&, const std::string&, Args...);
};