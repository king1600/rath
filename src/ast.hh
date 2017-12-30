#pragma once

#include <queue>
#include <string>
#include <memory>
#include <cstdio>
#include <vector>
#include <cstdint>
#include <exception>

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
    Semicolon = 15,
    Newline   = 16
} TokenType;

// changeable keywords
#define KeywordSwitch "switch"
#define KeywordCase "case"
#define KeywordWhen "when"
#define KeywordIf "if"
#define KeywordElse "else"
#define KeywordThen "then"
#define KeywordDeclare "let"
#define KeywordImport "open"
#define KeywordReturn "return"
#define KeywordFunction "func"
#define KeywordNull "null"
#define KeywordThis "this"
#define KeywordRef "ref"
#define KeywordConst "const"

// token object
class Token {
public:
    TokenType type;
    std::string text;
    std::size_t start;
    std::size_t lineno;

    static const char* type_str(TokenType);

    Token() : Token(None) {}
    Token(TokenType _type) : type(_type) {}
    Token(TokenType _type, const std::string& _text,
        const std::size_t& _start, const std::size_t& _lineno)
        : type(_type), text(_text), start(_start), lineno(_lineno) {}

    operator bool() const;
    std::string debug() const;
    bool is(TokenType type) const;
    bool is(const std::string& text) const;
};

// lexer interface
class Lexer {
public:
    std::string code;
    std::string file;
    std::size_t lineno;
    std::size_t current;

    Lexer() = default;
    Token next();
    Lexer& feed(const std::string& filename, const std::string& code);
};

// count occurances of char in string
std::size_t strcount(const std::string& str, const char c);

// format a string using sprintf
template <typename ...Args>
std::string sformat(const std::string& format, Args... args) {
    std::size_t size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

// Custome error object
class ParserError : public std::exception {
public:
    std::string message;

    ParserError(const std::string& msg) : message(msg) {}
    const char* what() const throw() {
        return message.c_str();
    }

    template <typename ...Args>
    static ParserError from(
        const Lexer& lexer,
        std::size_t start,
        const std::string& filename,
        const std::size_t& lineno,
        const std::string& format,
        Args... args)
    {
        // find start line of error
        while (start > 0 && lexer.code[start - 1] != '\n')
            start--;
        
        // get rid of beginning whitepace
        parse_trim_front:
        switch (lexer.code[start]) {
            case ' ': case '\t': case '\r':
                start++;
                goto parse_trim_front;
            default:
                break;
        }
        // find end line of error
        std::size_t end = lexer.code.find('\n', start);
        if (end == std::string::npos) end = lexer.code.size();

        // create error text
        std::string err = sformat("Error in %s:%lu:\n%.*s\n  > %s\n",
            filename.c_str(), lineno, (int)(end - start),
            lexer.code.c_str() + start, sformat(format, args...).c_str());
        
        // return parser error object
        return ParserError(err);
    }
};

////////////////////////////////////////////////////////

typedef enum {
    EUnop     = 0,
    EBinop    = 1,
    EConst    = 2,
    ECall     = 3,
    EFunction = 4,
    EReturn   = 5,
    EBlock    = 6,
    EIf       = 7,
    ESwitch   = 8,
    ECase     = 9,
    ECaseCond = 10,
    EAssign   = 11
} ExprType;

#define ExprPtr Expr*
class Expr {
public:
    Token token;
    ExprType type;

    static const char* type_str(ExprType);

    virtual ~Expr() = default;
    Expr(ExprType _type, const Token& _token)
        : token(_token), type(_type) {}

    template <typename T>
    inline T* as() {
        return reinterpret_cast<T*>(this);
    }

    inline bool is(ExprType t) const {
        return type == t;
    }

    inline ExprPtr ptr() {
        return this;
    }

    template <typename T>
    static void free(T** expr) {
        if (expr) delete *expr;
        *expr = nullptr;
    }

    template <typename T>
    static void free_list(std::vector<T>& list) {
        for (T expr : list) Expr::free(&expr);
        list.clear();
    }

    virtual void print() const = 0;
};

typedef enum {
    EConstInt    = 0,
    EConstFloat  = 1,
    EConstString = 2,
    EConstIdent  = 3,
    EConstNull   = 4,
    EConstThis   = 5
} ConstExprType;

class Const : public Expr {
public:
    void print() const;
    ConstExprType const_type;
    Const(const Token& token, ConstExprType type)
        : Expr(EConst, token), const_type(type) {}

    static const char* type_str(ConstExprType type);
};

class ConstInt : public Const {
public:
    void print() const;
    std::uint64_t value;
    ConstInt(const Token& token, const std::uint64_t& _value)
        : Const(token, EConstInt), value(_value) {}
};

class ConstFloat : public Const {
public:
    void print() const;
    double value;
    ConstFloat(const Token& token, const double& _value)
        : Const(token, EConstFloat), value(_value) {}
};

class ConstString : public Const {
public:
    void print() const;
    std::string value;
    ConstString(const Token& token, const std::string& _value)
        : Const(token, EConstString), value(_value) {}
};

class Var : public Const {
public:
    struct Flag {
        static constexpr int 
            Ref = 1 << 1,
            Const = 1 << 2,
            Packed = 1 << 3;
    };

    void print() const;
    int flags = 0;
    std::string name;
    Var(const Token& token, const int& _flags, const std::string& _name)
        : Const(token, EConstIdent), flags(_flags), name(_name) {}
};

class Unop : public Expr {
public:
    void print() const;
    ExprPtr value;
    ~Unop() { Expr::free(&value); }
    Unop(const Token& token, ExprPtr _value)
        : Expr(EUnop, token), value(_value) {}
};

class Binop : public Expr {
public:
    void print() const;
    ExprPtr left;
    ExprPtr right;
    ~Binop() { Expr::free(&left); Expr::free(&right); }
    Binop(const Token& token, ExprPtr _left, ExprPtr _right)
        : Expr(EBinop, token), left(_left), right(_right) {}
};

class Return : public Expr {
public:
    void print() const;
    ExprPtr value;
    ~Return() { Expr::free(&value); }
    Return(const Token& token, ExprPtr _value)
        : Expr(EReturn, token), value(_value) {}
};

class Call : public Expr {
public:
    void print() const;
    std::string name;
    std::vector<ExprPtr> args;
    ~Call() { Expr::free_list(args); }
    Call(const Token& token, const std::string& _name)
        : Expr(ECall, token), name(_name) {}
};

class Block : public Expr {
public:
    void print() const;
    std::vector<ExprPtr> body;
    ~Block() { Expr::free_list(body); }
    Block(const Token& token) : Expr(EBlock, token) {}
};

class Case;
class Switch : public Expr {
public:
    void print() const;
    ExprPtr value;
    std::vector<Case*> cases;
    ~Switch() { Expr::free(&value); Expr::free_list(cases); }
    Switch(const Token& token, ExprPtr _value)
        : Expr(ESwitch, token), value(_value) {}
};

class CaseCondition : public Expr {
public:
    void print() const;
    ExprPtr value;
    ExprPtr condition;
    bool is_direct = true;
    CaseCondition(const Token& token, ExprPtr _value, ExprPtr _condition)
        : Expr(ECaseCond, token), value(_value), condition(_condition) {}
    ~CaseCondition() {
        Expr::free(&value);
        if (is_direct && condition && condition->is(EBinop)) {
            Binop* op = condition->as<Binop>();
            op->left = nullptr;
            op->right = nullptr;
            delete op;
        }
    }
};

class Case : public Expr {
public:
    void print() const;
    ExprPtr body;
    CaseCondition* condition;
    ~Case() { Expr::free(&body); Expr::free(&condition); }
    Case(const Token& token, ExprPtr _body, CaseCondition* _condition)
        : Expr(ECase, token), body(_body), condition(_condition) {}
};

class Function : public Expr {
public:
    void print() const;
    ExprPtr body;
    std::string name;
    std::vector<Var*> args;
    ~Function() { Expr::free(&body); Expr::free_list(args); }
    Function(const Token& token, const std::string& _name)
        : Expr(EFunction, token), name(_name) {}
};

class Assign : public Expr {
public:
    void print() const;
    ExprPtr value;
    std::vector<Var*> vars;
    ~Assign() { Expr::free(&value); Expr::free_list(vars); }
    Assign(const Token& token, ExprPtr _value)
        : Expr(EAssign, token), value(_value) {}
};

class If : public Expr {
public:
    void print() const;
    ExprPtr body;
    ExprPtr else_body;
    ExprPtr condition;
    ~If() { Expr::free(&body); Expr::free(&else_body); Expr::free(&condition); }
    If(const Token& token, ExprPtr x, ExprPtr y, ExprPtr z)
        : Expr(EIf, token), body(x), else_body(y), condition(z) {}
};

class Parser {
private:
    Lexer lexer;
    std::queue<Token> peeks;
    Token consume_error(TokenType, const std::string&, bool, bool);

public:
    Token current;

    Parser() {};
    Token next();
    Token peek();
    ExprPtr parse(const std::string& filename, const std::string& code);

    Token consume(bool maybe = false);
    Token consume(TokenType, bool maybe = false);
    Token consume(const std::string&, bool maybe = false);
    Token consume(TokenType, const std::string&, bool maybe = false, bool has_type = true);

    template <typename ...Args>
    void error(const Token& token, const std::string& format, Args... args) {
        throw ParserError::from(lexer, token.start, lexer.file, token.lineno, format, args...);
    }
};