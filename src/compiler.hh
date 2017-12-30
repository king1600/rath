#pragma once

#include "ast.hh"

class Compiler {
private:
    Parser parser;

public:
    Compiler() = default;

    ExprPtr optimize(ExprPtr tree);

    bool analyze(ExprPtr* tree);

    int compile(const std::string& code);
};