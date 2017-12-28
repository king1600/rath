#include "lexer.hh"

#include <utility>

Parser::Parser() : current(Eof) {
}

template <typename ...Args>
ParserError Parser::error(const Token& token, const std::string& format, Args... args) {
    return parser_error(lexer, token.lineno, token.start, format, args...);
}

Token Parser::next() {
    if (!peeks.empty()) {
        Token token = std::move(peeks.front());
        peeks.pop();
        return token;
    }
    return lexer.next();
}

Token Parser::peek() {
    Token token = next();
    peeks.push(token);
    return token;
}

Token Parser::consume(TokenType type, bool maybe) {
    return consume(type, std::string(), maybe, true);
}

Token Parser::consume(const std::string& str, bool maybe) {
    return consume(None, std::string(), maybe, false);
}

Token Parser::consume_error(TokenType type, const std::string& str, bool maybe, bool has_type) {
    if (maybe) return Token(None);
    ParserError err = !has_type ?
        error(current, "Expected %s, got %s",
            str.c_str(), current.text.c_str()) :
        error(current, "Expected %s, got %s",
            token_type_str(type), token_type_str(current.type));
    throw err;
    return Token(None);
}

Token Parser::consume(TokenType type, const std::string& str, bool maybe, bool has_type) {
    if (str.size() > 0)
        if (current.text != str)
            return consume_error(type, str, maybe, has_type);
    if (has_type)
        if (current.type != type)
            return consume_error(type, str, maybe, has_type);
    Token last = current;
    current = next();
    return last;
}

//// Parser helper functions

template <typename T>
static inline T expr_cast(ExprPtr e) {
    return static_cast<T>(e);
}

static inline bool expects_end(ExprPtr e) {
    if (e == nullptr)
        return false;
    switch (e->type) {
        case ExprSwitch:
        case ExprBlock:
            return false;
        case ExprIf:
            return expects_end(expr_cast<If*>(e)->body);
        case ExprBinop:
            return expects_end(expr_cast<Binop*>(e)->right);
        case ExprFunction:
            return expects_end(expr_cast<Function*>(e)->body);
        default:
            return true;
    }
}

Const::Const(const Token& token) : Expr(ExprConst, token) {
    switch (token.type) {
        case String:
            const_type = ConstString;
            value = new std::string(token.text);
            break;
        case Ident:
            if (token.text == "null")
                const_type = ConstNull;
            else if (token.text == "this")
                const_type = ConstThis;
            else
                const_type = ConstIdent;
            value = std::string(token.text);
            break;
        case Number:
            if (strcount(token.text, '.') > 0) {
                const_type = ConstFloat;
                value = std::stod(token.text);
            } else {
                const_type = ConstInt;
                value = std::stoull(token.text);
            }
        default:
            break;
    }
}

/// Operator associativity and precedence

static const char* comparators[] = {
    "==", "!=", ">", "<", ">=", "<="
};

typedef enum { OpLeft, OpRight } OpAssoc;

// get operator associativity
static inline OpAssoc op_assoc(const std::string& op) {
    return (op == "=" || op == ":=")
        ? OpRight : OpLeft;
}

// check if operator is unary
static inline bool op_unary(const std::string& op) {
    return (op == "-" || op == "&");
}

// get operator precedence
static inline int op_prec(const std::string& op) {
    if (op == "=" || op == ":=")
        return 0;
    if (op == "||")
        return 1;
    if (op == "&&")
        return 2;
    if (op == "|")
        return 3;
    if (op == "^")
        return 4;
    if (op == "&")
        return 5;
    if (op == "!=" || op == "==")
        return 6;
    for (const char* str : comparators)
        if (op == str)
            return 7;
    if (op == "+" || op == "-")
        return 9;
    if (op == "*" || op == "/" || op == "%")
        return 10;
    return -1;
}

///----------------------
// Expression Parsing
///----------------------

ExprPtr parse_if(Parser& parser);
ExprPtr parse_call(Parser& parser);
ExprPtr parse_expr(Parser& parser);
ExprPtr parse_block(Parser& parser);
ExprPtr parse_switch(Parser& parser);
ExprPtr parse_assign(Parser& parser);
ExprPtr parse_constant(Parser& parser);
ExprPtr parse_positional(Parser& parser);
ExprPtr parse_case(Parser& parser, ExprPtr value);
ExprPtr parse_func(Parser& parser, bool has_name = true);
ExprPtr parse_statement(Parser& parser, int precedence = 0);
ExprPtr parse_case_condition(Parser& parser, ExprPtr value);

ExprPtr Parser::parse(const std::string& code) {
    current = lexer.feed(code).next();

    ExprPtr result = parse_block(*this);
    Block* block = expr_cast<Block*>(result);
    if (block == nullptr) return nullptr;

    switch (block->body.size()) {
        case 0:
            return nullptr;
        case 1:
            return std::move(block->body[0]);
        default:
            return result;
    }
}

ExprPtr parse_block(Parser& p) {
    ExprPtr expr = nullptr;
    Block* block = new Block(p.current);
    p.consume(LCurly, true);

    while (true) {
        if (p.consume(Eof, true)) break;
        if (p.consume(RCurly, true)) break;

        expr = parse_expr(p);
        block->body.push_back(expr);

        if (p.consume(RCurly, true)) break;
        if (p.consume(Eof, true).is(Eof)) break;

        if (expects_end(expr))
            p.consume(Semicolon);
    }

    return expr_cast<ExprPtr>(block);
}

static inline ExprPtr parse_return(Parser& p, const Token& token) {
    Return* e = new Return(
        std::move(p.consume(token.type)),
        parse_statement(p));
    return expr_cast<ExprPtr>(e);
}

ExprPtr parse_expr(Parser& p) {
    const Token& token = p.current;

    if (token.is(LCurly))
        return parse_block(p);

    if (token.is(Keyword)) {
        if (token.is("let"))
            return parse_assign(p);
        if (token.is("func"))
            return parse_func(p);
        if (token.is("if"))
            return parse_if(p);
        if (token.is("switch"))
            return parse_switch(p);
        if (token.is("return"))
            return parse_return(p, token);
    }

    return parse_statement(p);
}

ExprPtr parse_statement(Parser& p, int precedence) {
    int next_precedence;
    ExprPtr value = parse_positional(p);

    while (p.current.is(Operator) && op_prec(p.current.text) >= precedence) {
        Token token = std::move(p.consume(p.current.type));
        next_precedence = op_prec(token.text);
        if (op_assoc(token.text) == OpLeft)
            next_precedence++;
        value = new Binop(token,
            value,
            parse_statement(p, next_precedence));
    }

    return value;
}

ExprPtr parse_positional(Parser& p) {
    if (op_unary(p.current.text)) {
        Token token = std::move(p.consume(p.current.type));
        int next_precedence = op_prec(token.text);
        Unop* value = new Unop(token, parse_statement(p, next_precedence));
        return expr_cast<ExprPtr>(value);
    }

    if (p.current.is(LParen)) {
        Token token = std::move(p.consume(LParen));
        ExprPtr value = parse_statement(p);
        p.consume(RParen);
        return value;
    }

    return parse_constant(p);
}

ExprPtr parse_constant(Parser& p) {
    const Token& token = p.current;

    switch (token.type) {
        case Ident:
            if (p.peek().is(LParen))
                return parse_call(p);
        case Number:
        case String:
            return expr_cast<ExprPtr>(new Const(
                std::move(p.consume(token.type))));

        case Keyword:
            if (token.is("func"))
                return parse_func(p, false);
            if (token.is("switch"))
                return parse_switch(p);
            if (token.is("if"))
                return parse_if(p);
            throw p.error(token,
                "Unexpected keyword '%s'", token.text.c_str());
            return nullptr;

        default:
            return nullptr;
    }
}

ExprPtr parse_call(Parser& p) {
    Call* expr = new Call(p.current);
    p.consume(Ident);
    p.consume(LParen);

    while (true) {
        if (p.consume(RParen, true)) break;
        expr->args.push_back(parse_statement(p));
        if (p.consume(RParen, true)) break;
        p.consume(Comma);
    }

    return expr_cast<ExprPtr>(expr);
}

ExprPtr parse_assign(Parser& p) {
    Token token = std::move(p.consume(Keyword, "let"));
    Token name = std::move(p.consume(Ident));
    p.consume(Operator, "=");

    Binop *expr = new Binop(token,
        new Const(name),
        parse_statement(p));

    return expr_cast<ExprPtr>(expr);
}

ExprPtr parse_func(Parser& p, bool has_name) {
    Token token = std::move(p.consume(Keyword));
    std::string name = has_name ? std::move(p.consume(Ident)).text : "";
    Function* expr = new Function(token, name);

    bool has_paren = p.consume(LParen, true);
    while (true) {
        if (p.consume(has_paren ? RParen : Arrow, true)) break;
        expr->args.push_back(new Const(p.consume(Ident)));
        if (p.consume(has_paren ? RParen : Arrow, true)) break;
        p.consume(Comma);
    }

    p.consume(Arrow, true);
    expr->body = parse_expr(p);
    return expr_cast<ExprPtr>(expr);
}

ExprPtr parse_if(Parser& p) {
    Token token = std::move(p.consume(Keyword, "if"));
    Token paren = std::move(p.consume(LParen, true));
    ExprPtr condition = parse_statement(p);

    if (paren) {
        p.consume(RParen);
        paren.type = None;
    }
    if (!p.consume(Keyword, "then", true).is("then"))
        p.consume(Arrow, paren ? true : false);

    ExprPtr body = parse_expr(p);
    ExprPtr else_expr = p.consume(Keyword, "else", true) ?
        parse_expr(p) : nullptr;

    return expr_cast<ExprPtr>(new If(
        token, else_expr, condition, body));
}

ExprPtr parse_switch(Parser& p) {
    Token token = std::move(p.consume(Keyword, "switch"));
    ExprPtr value = parse_statement(p);
    Switch* expr = new Switch(token);

    p.consume(Arrow, true);
    p.consume(LCurly);
    while (true) {
        if (p.consume(RCurly, true)) break;
        expr->cases.push_back(parse_case(p, value));
        if (p.consume(RCurly, true)) break;
    }

    return expr_cast<ExprPtr>(expr);
}

ExprPtr parse_case(Parser& p, ExprPtr value) {
    Token token = std::move(p.consume(Keyword, "case"));
    CaseCondition* cond = expr_cast<CaseCondition*>(
        std::move(parse_case_condition(p, value)));
    CaseCondition* and_cond = nullptr;

    while (p.consume(Keyword, "case", true)) {
        and_cond = expr_cast<CaseCondition*>(
            std::move(parse_case_condition(p, value)));
        cond->condition = expr_cast<ExprPtr>(new Binop(
            and_cond->token, cond->condition, and_cond->condition));
        cond->condition->token.text = "||";
        cond->value = and_cond->value;
    }

    p.consume(Arrow);
    return expr_cast<ExprPtr>(new Case(
        token, expr_cast<ExprPtr>(cond), parse_expr(p)));
}

ExprPtr parse_case_condition(Parser& p, ExprPtr value) {
    ExprPtr cond = nullptr;
    ExprPtr set_value = parse_statement(p);

    if (p.consume(Keyword, "when", true)) {
        cond = parse_statement(p);
    } else {
        cond = expr_cast<ExprPtr>(new Binop(
            value->token, value, set_value));
        cond->token.text = "==";
    }

    return expr_cast<ExprPtr>(new CaseCondition(
        set_value->token, set_value, cond));
}


