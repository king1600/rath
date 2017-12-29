#include "ast.hh"

template <typename ...Args>
ParserError Parser::error(const Token& token, const std::string& format, Args... args) const {
    return ParserError::from(lexer, token.start, token.lineno, format, args...);
}

Token Parser::next() {
    if (!peeks.empty()) {
        Token token = std::move(peeks.front());
        peeks.pop();
        return std::move(token);
    }
    return std::move(lexer.next());
}

Token Parser::peek() {
    Token token = next();
    peeks.push(token);
    return std::move(token);
}

Token Parser::consume_error(TokenType type, const std::string& str, bool maybe, bool has_type) {
    if (maybe) return Token(None);
    ParserError err = !has_type ?
        error(current, "Expected %s, got %s",
            str.c_str(), current.text.c_str()) :
        error(current, "Expected %s, got %s",
            Token::type_str(type), Token::type_str(current.type));
    throw err;
    return Token(None);
}

Token Parser::consume(bool maybe) {
    return consume(current.type, std::string(), maybe, true);
}

Token Parser::consume(TokenType type, bool maybe) {
    return consume(type, std::string(), maybe, true);
}

Token Parser::consume(const std::string& str, bool maybe) {
    return consume(None, str, maybe, false);
}

Token Parser::consume(TokenType type, const std::string& str, bool maybe, bool has_type) {
    if (str.size() > 0)
        if (current.text != str)
            return consume_error(type, str, maybe, has_type);
    if (has_type)
        if (current.type != type)
            return consume_error(type, str, maybe, has_type);
    Token last = std::move(current);
    current = next();
    return last;
}

/// Operator associativity and precedence

// check if operator is unary
static inline bool op_unary(const std::string& op) {
    return (op == "-" || op == "&");
}

// get operator associativity
typedef enum { OpLeft, OpRight } OpAssoc;
static inline OpAssoc op_assoc(const std::string& op) {
    return (op == "=" || op == ":=")
        ? OpRight : OpLeft;
}

// get operator precedence
static const char* comparators[] = { "==", "!=", ">", "<", ">=", "<=" };
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
        return 8;
    if (op == "*" || op == "/" || op == "%")
        return 9;
    return -1;
}

///----------------------
// Expression Parsing
///----------------------

If* parse_if(Parser& parser);
Call* parse_call(Parser& parser);
ExprPtr parse_expr(Parser& parser);
Block* parse_block(Parser& parser);
Switch* parse_switch(Parser& parser);
Assign* parse_assign(Parser& parser);
Return* parse_return(Parser& parser);
Const* parse_constant(Parser& parser);
ExprPtr parse_positional(Parser& parser);
Case* parse_case(Parser& parser, ExprPtr value);
Function* parse_func(Parser& parser, bool has_name = true);
ExprPtr parse_statement(Parser& parser, int precedence = 0);
CaseCondition* parse_case_condition(Parser& parser, ExprPtr value);

static inline void consume_end(Parser& p, bool maybe = false) {
    p.consume(Semicolon, maybe);
}

ExprPtr Parser::parse(const std::string& code) {
    current = lexer.feed(code).next();
    ExprPtr expr = parse_expr(*this);

    if (!current.is(Eof)) {
        consume_end(*this);
        Block* block = parse_block(*this);
        block->body.insert(block->body.begin(), expr);
        expr = block;
    }

    return expr;
}

static inline bool expects_end(ExprPtr expr) {
    if (expr == nullptr)
        return false;
    switch (expr->type) {
        case ESwitch:
        case EBlock:
            return false;
        case EIf:
            return expects_end(expr->as<If>()->body);
        case EBinop:
            return expects_end(expr->as<Binop>()->right);
        case EFunction:
            return expects_end(expr->as<Function>()->body);
        default:
            return true;
    }
}

Block* parse_block(Parser& p) {
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
            consume_end(p);
    }

    return block;
}

ExprPtr parse_expr(Parser& p) {
    const Token& token = p.current;

    if (token.is(LCurly))
        return parse_block(p);

    if (token.is(Keyword)) {
        if (token.is(KeywordDeclare))
            return parse_assign(p);
        if (token.is(KeywordFunction))
            return parse_func(p);
        if (token.is(KeywordIf))
            return parse_if(p);
        if (token.is(KeywordSwitch))
            return parse_switch(p);
        if (token.is(KeywordReturn))
            return parse_return(p);
    }

    return parse_statement(p);
}

Return* parse_return(Parser& p) {
    Token token = p.consume(Keyword, KeywordReturn);
    return new Return(p.consume(), parse_statement(p));
}

ExprPtr parse_statement(Parser& p, int precedence) {
    int next_precedence;
    ExprPtr rhs = nullptr;
    ExprPtr lhs = parse_positional(p);

    while (p.current.is(Operator) && op_prec(p.current.text) >= precedence) {
        Token token = p.consume();

        next_precedence = op_prec(token.text);
        if (op_assoc(token.text) == OpLeft)
            next_precedence++;

        if (token.text == "=")
            p.error(token, "'=' only allowed in variable declaration %s", "");
        if (token.text == "...")
            p.error(token, "Illegal varargs '...' operator%s", "");

        rhs = parse_statement(p, next_precedence);
        lhs = new Binop(token, lhs, rhs);
    }

    return lhs;
}

ExprPtr parse_positional(Parser& p) {
    const Token& token = p.current;

    if (op_unary(token.text)) {
        Token unop_token = p.consume();
        int next_precedence = op_prec(unop_token.text);
        Unop* value = new Unop(unop_token, parse_statement(p, next_precedence));
        return value;
    }

    switch (token.type) {
        case Ident:
            if (p.peek().is(LParen))
                return parse_call(p);
        case Number:
        case String:
            return parse_constant(p);

        case LParen: {
            Token paren_token = p.consume(LParen);
            ExprPtr value = parse_statement(p);
            p.consume(RParen);
            return value;
        }

        case Keyword:
            if (token.is(KeywordFunction))
                return parse_func(p, false);
            if (token.is(KeywordSwitch))
                return parse_switch(p);
            if (token.is(KeywordIf))
                return parse_if(p);
            throw p.error(token,
                "Unexpected keyword '%s'", token.text.c_str());
            return nullptr;

        default:
            return nullptr;
    }
}

Const* parse_constant(Parser& p) {
    Token token = p.current;

    switch (token.type) {
        case String:
            return new ConstString(p.consume(), token.text);

        case Ident:
            if (token.text == KeywordNull)
                return new Const(p.consume(), EConstNull);
            else if (token.text == KeywordThis)
                return new Const(p.consume(), EConstThis);
            else
                return new ConstIdent(p.consume(), false, token.text);
            return nullptr;

        case Number:
            if (strcount(token.text, '.') > 0)
                return new ConstFloat(p.consume(), std::stod(token.text));
            else
                return new ConstInt(p.consume(), std::stoull(token.text));
            return nullptr;

        default:
            return nullptr;
    }
}

Call* parse_call(Parser& p) {
    Call* call = new Call(p.current, p.current.text);
    p.consume(Ident);
    p.consume(LParen);

    while (true) {
        if (p.consume(RParen, true)) break;
        call->args.push_back(parse_statement(p));
        if (p.consume(RParen, true)) break;
        p.consume(Comma);
    }

    return call;
}

Assign* parse_assign(Parser& p) {
    Token token = p.consume(Keyword, KeywordDeclare);
    bool is_ref = p.consume(Keyword, KeywordRef, true);
    Assign* assign = new Assign(token, is_ref, nullptr);

    bool is_pack;
    Token var_name;

    while (true) {
        if (p.consume(Operator, "=", true)) break;
        is_pack = p.current.is(Ident) ? false : p.consume(Operator, "...");
        var_name = p.consume(Ident);
        assign->vars.push_back(new ConstIdent(var_name, is_pack, var_name.text));
        if (p.consume(Operator, "=", true)) break;
        p.consume(Comma);
    }

    if (assign->vars.size() == 0)
        throw p.error(assign->token, "No variable name provided%s", "");
    if (assign->vars[0]->is_pack)
        throw p.error(assign->token, "single variable declaraction does not need to be packed%s", "");
    
    assign->value = parse_statement(p);
    return assign;
}

Function* parse_func(Parser& p, bool has_name) {
    Token token = p.consume(Keyword, KeywordFunction);
    std::string name = has_name ? p.consume(Ident).text : "";
    Function* func = new Function(token, name);

    bool is_pack;
    Token arg_name;
    bool has_paren = p.consume(LParen, true);

    while (true) {
        if (p.consume(has_paren ? RParen : Arrow, true)) break;
        is_pack = p.current.is(Ident) ? false : p.consume(Operator, "...");
        arg_name = p.consume(Ident);
        func->args.push_back(new ConstIdent(arg_name, is_pack, arg_name.text));
        if (p.consume(has_paren ? RParen : Arrow, true)) break;
        p.consume(Comma);
    }

    p.consume(Arrow, true);
    func->body = parse_expr(p);
    return func;
}

If* parse_if(Parser& p) {
    Token token = p.consume(Keyword, KeywordIf);
    Token paren = p.consume(LParen, true);
    ExprPtr condition = parse_statement(p);

    if (paren) {
        p.consume(RParen);
        paren.type = None;
    }

    if (!p.consume(Keyword, KeywordThen, true))
        p.consume(Arrow, paren ? true : false);

    ExprPtr body = parse_expr(p);
    ExprPtr else_expr = p.consume(Keyword, KeywordElse, true) ?
        parse_expr(p) : nullptr;

    return new If(token, body, else_expr, condition);
}

Switch* parse_switch(Parser& p) {
    Token token = p.consume(Keyword, KeywordSwitch);
    ExprPtr value = parse_statement(p);
    Switch* switch_expr = new Switch(token);

    p.consume(Arrow, true);
    p.consume(LCurly);
    while (true) {
        if (p.consume(RCurly, true)) break;
        switch_expr->cases.push_back(parse_case(p, value));
        if (p.consume(RCurly, true)) break;
    }

    return switch_expr;
}

Case* parse_case(Parser& p, ExprPtr value) {
    Token token = p.consume(Keyword, KeywordCase);
    CaseCondition* cond = parse_case_condition(p, value);
    CaseCondition* and_cond = nullptr;

    while (p.consume(Keyword, KeywordCase, true)) {
        and_cond = parse_case_condition(p, value);
        cond->condition = new Binop(and_cond->token,
            cond->condition, and_cond->condition);
        cond->condition->token.text = "||";
        cond->value = and_cond->value;
    }

    p.consume(Arrow);
    ExprPtr body = parse_expr(p);
    return new Case(token, body, cond);
}

CaseCondition* parse_case_condition(Parser& p, ExprPtr value) {
    ExprPtr cond = nullptr;
    ExprPtr set_value = parse_statement(p);

    if (p.consume(Keyword, KeywordWhen, true)) {
        cond = parse_statement(p);
    } else {
        cond = new Binop(value->token, value, set_value);
        cond->token.text = "==";
    }

    return new CaseCondition(set_value->token, set_value, cond);
}