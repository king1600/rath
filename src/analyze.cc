#include "compiler.hh"

#include <map>

struct Scope {
    Scope *previous;
    std::map<std::string, Var*> vars;
    Scope(Scope* last = nullptr) : previous(last) {}

    Var* find(const std::string& name) {
        auto v = vars.find(name);
        if (v == vars.end())
            return previous ? previous->find(name) : nullptr;
        return v->second;
    }
};

bool Compiler::analyze(ExprPtr* tree) {
    *tree = optimize(*tree);
    return true;
}