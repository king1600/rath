#include "ast.hh"

std::size_t strcount(const std::string& str, const char c) {
    register std::size_t pos = 0, count = 0;
    while ((pos = str.find(c, pos)) < str.size()) {
        count++;
        pos++;
    }
    return count;
}

Token::operator bool() const {
    switch (type) {
        case Eof: case None: return false;
        default: return true;
    }
}

bool Token::is(TokenType type) const {
    return this->type == type;
}

bool Token::is(const std::string& text) const {
    return this->text == text;
}

static const char* token_type_map[] = {
    "None", "Eof", "Ident", "String", "Number",
    "Keyword", "Operator", "LParen", "RParen",
    "LCurly", "RCurly", "LBracket", "RBracket",
    "Comma", "Arrow", "Semicolon", "Newline"
};

const char* Token::type_str(TokenType type) {
    return token_type_map[type];
}

std::string Token::debug() const {
    return std::move(sformat("[%s %s]",
        token_type_map[type], text.c_str()));
}

///////////////////////////////////////////////////////////////

static const char* expr_const_type_map[] = {
    "Int", "Float", "String", "Ident", 
    "Null", "This"
};

const char* Const::type_str(ConstExprType type) {
    return expr_const_type_map[type];
}

static const char* expr_type_map[] = {
    "Unop", "Binop", "Const", "Call",
    "Function", "Return", "Block", "If",
    "Switch", "Case", "CaseCond", "Assign"
};

const char* Expr::type_str(ExprType type) {
    return expr_type_map[type];
}

void Expr::print() const {
    std::printf("[Expr]");
}

void Const::print() const {
    std::printf("[Const %s]", Const::type_str(const_type));
}

void ConstInt::print() const {
    std::printf("[%s %lu]", Const::type_str(const_type), value);
}

void ConstFloat::print() const {
    std::printf("[%s %g]", Const::type_str(const_type), value);
}

void ConstString::print() const {
    std::printf("[%s %s]", Const::type_str(const_type), value.c_str());
}

void ConstIdent::print() const {
    std::printf("[%s %s%s]", Const::type_str(const_type),
        is_pack ? "..." : "", value.c_str());
}

void Unop::print() const {
    std::printf("[Unop(%s) ", token.text.c_str());
    if (value) value->print();
    std::printf("]");
}

void Binop::print() const {
    std::printf("[Binop(%s) ", token.text.c_str());
    std::printf("left="); if (left) left->print(); else std::printf("null");
    std::printf(" right="); if (right) right->print(); else std::printf("null");
    std::printf("]");
}

void Return::print() const {
    std::printf("[Return ");
    if (value) value->print();
    std::printf("]");
}

void Call::print() const {
    std::printf("[Call%s%s args={", name.size() > 0 ? " " : "", name.c_str());
    for (std::size_t i = 0; i < args.size(); i++) {
        if (args[i]) args[i]->print();
        if (i < args.size() - 1) std::printf(", ");
    }
    std::printf("}]");
}

void Block::print() const {
    std::printf("[Block body={");
    for (std::size_t i = 0; i < body.size(); i++) {
        if (body[i]) body[i]->print();
        if (i < body.size() - 1) std::printf(", ");
    }
    std::printf("}]");
}

void CaseCondition::print() const {
    std::printf("[Cond ");
    if (condition) condition->print();
    std::printf("]");
}

void Case::print() const {
    std::printf("[Case ");
    if (condition) condition->print();
    std::printf(" body="); if (body) body->print();
    std::printf("]");
}

void Switch::print() const {
    std::printf("[Switch cases={");
    for (std::size_t i = 0; i < cases.size(); i++) {
        if (cases[i]) cases[i]->print();
        if (i < cases.size() - 1) std::printf(", ");
    }
    std::printf("}]");
}

void Function::print() const {
    const char* space = name.size() > 0 ? " " : "";
    std::printf("[Func%s%s%sargs={", space, name.c_str(), space);
    for (std::size_t i = 0; i < args.size(); i++) {
        if (args[i]) args[i]->print();
        if (i < args.size() - 1) std::printf(", ");
    }
    std::printf("} body=");
    if (body) body->print(); else std::printf("null");
    std::printf("]");
}

void Assign::print() const {
    std::printf("[Assign%svars={", is_ref ? " ref " : " ");
    for (std::size_t i = 0; i < vars.size(); i++) {
        if (vars[i]) vars[i]->print();
        if (i < vars.size() - 1) std::printf(", ");
    }
    std::printf("} value=");
    if (value) value->print(); else std::printf("null");
    std::printf("]");
}

void If::print() const {
    std::printf("[If ");
    if (condition) condition->print(); else std::printf("null");
    std::printf(" ");
    if (body) body->print(); else std::printf("null");
    std::printf(" Else ");
    if (else_body) else_body->print(); else std::printf("null");
    std::printf("]");
}