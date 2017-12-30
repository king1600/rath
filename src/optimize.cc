#include "compiler.hh"

#define expr_is_const(e) ((e) && ((e)->type == EConst))
#define is_ctype(e, type) ((e)->const_type == type)
#define e_to_val(e, type) ((e)->as<type>()->value)

#define const_cross_operators \
    if (op == "+") return left + right; \
    if (op == "-") return left - right; \
    if (op == "*") return left * right; \
    if (op == "/") return left / right

#define const_int_operators \
    if (op == "&") return left & right; \
    if (op == "^") return left ^ right; \
    if (op == "|") return left | right; \
    if (op == "%") return left % right; \
    if (op == ">>") return left >> right; \
    if (op == "<<") return left << right
    
#define binop_combinator(name, body)     \
    template <typename L, typename R>    \
    static inline L name(                \
        Parser& p, const Token& token,   \
        const L& left, const R& right) { \
        const std::string& op = token.text; \
        body;                               \
        p.error(token,                      \
            "Invalid operator %s on constant expressions", op.c_str()); \
        return left; \
    }

binop_combinator(const_combine, const_cross_operators)
binop_combinator(const_combine_int, const_cross_operators; const_int_operators)

static inline Const* binop_resolve(Parser &p, const Token& token, Const* left, Const* right) {
    if (is_ctype(left, EConstIdent) || is_ctype(right, EConstIdent))
        return nullptr;
    Const* resolved = nullptr;

    // int op int
    if (is_ctype(left, EConstInt) && is_ctype(right, EConstInt))
        resolved = new ConstInt(left->token, const_combine_int(p, token,
            e_to_val(left, ConstInt), e_to_val(right, ConstInt)));

    // int op float
    if (is_ctype(left, EConstInt) && is_ctype(right, EConstFloat))
        resolved = new ConstInt(left->token, const_combine(p, token,
            e_to_val(left, ConstInt), e_to_val(right, ConstFloat)));

    // float op int
    if (is_ctype(left, EConstFloat) && is_ctype(right, EConstInt))
        resolved = new ConstInt(left->token, const_combine(p, token,
            e_to_val(left, ConstInt), e_to_val(right, ConstInt)));

    // float op float
    if (is_ctype(left, EConstFloat) && is_ctype(right, EConstFloat))
        resolved = new ConstInt(left->token, const_combine(p, token,
            e_to_val(left, ConstFloat), e_to_val(right, ConstFloat)));

    // string op string
    if (is_ctype(left, EConstString) && is_ctype(right, EConstString) && token.text == "+")
        resolved = new ConstString(left->token,
            e_to_val(left, ConstString) + e_to_val(right, ConstString));
    
    return resolved;
}

static inline Const* unary_resolve(Parser& p, const Token& token, Const* value) {
    switch (value->const_type) {
        case EConstInt:
            return new ConstInt(token, const_combine(
                p, token, 0, e_to_val(value, ConstInt)));
        case EConstFloat:
            return new ConstFloat(token, const_combine(
                p, token, 0.0, e_to_val(value, ConstFloat)));
        default:
            p.error(token, "Invalid unary operator %s on constant expression",
                token.text.c_str());
            return nullptr;
    }
}

#define const_fold_list(list, type) \
    for (std::size_t i = 0; i < list.size(); i++) \
        list[i] = static_cast<type>(constant_fold(p, list[i]))

static inline ExprPtr constant_fold(Parser& p, ExprPtr expr) {
    if (!expr) return expr;
    switch (expr->type) {

        case EUnop: {
            Unop* op = expr->as<Unop>();
            op->value = constant_fold(p, op->value);
            if (expr_is_const(op->value)) {
                Const* combined = unary_resolve(p, op->token, op->value->as<Const>());
                if (combined) {
                    delete op;
                    return combined;
                }
            }
            return op;
        }

        case EBinop: {
            Binop* op = expr->as<Binop>();
            op->left = constant_fold(p, op->left);
            op->right = constant_fold(p, op->right);
            if (expr_is_const(op->left) && expr_is_const(op->right)) {
                Const* combined = binop_resolve(p, op->token,
                    op->left->as<Const>(), op->right->as<Const>());
                if (combined) {
                    delete op;
                    return combined;
                }
            }
            return op;
        }

        case EReturn: {
            Return* e = expr->as<Return>();
            e->value = constant_fold(p, e->value);
            return e;
        }

        case EFunction: {
            Function* e = expr->as<Function>();
            e->body = constant_fold(p, e->body);
            return e;
        }

        case EAssign: {
            Assign* e = expr->as<Assign>();
            e->value = constant_fold(p, e->value);
            return e;
        }

        case ECall: {
            std::vector<ExprPtr>& args = expr->as<Call>()->args;
            const_fold_list(args, ExprPtr);
            return expr;
        }

        case EBlock: {
            std::vector<ExprPtr>& body = expr->as<Block>()->body;
            const_fold_list(body, ExprPtr);
            return expr;
        }

        case ESwitch: {
            std::vector<Case*>& cases = expr->as<Switch>()->cases;
            const_fold_list(cases, Case*);
            return expr;
        }

        case ECaseCond: {
            CaseCondition* e = expr->as<CaseCondition>();
            e->value = constant_fold(p, e->value);
            e->condition = constant_fold(p, e->condition);
            return e;
        }

        case ECase: {
            Case* e = expr->as<Case>();
            e->body = constant_fold(p, e->body);
            e->condition = static_cast<CaseCondition*>(
                constant_fold(p, e->condition));
            return e;
        }

        default:
            return expr;
    }
}

ExprPtr Compiler::optimize(ExprPtr tree) {
    tree = constant_fold(parser, tree);
    return tree;
}