#include <cstdio>
#include "ast.hh"

int main() {
    const char* code = "let x = if (5 == 6) -> 5";

    Parser parser;
    ExprPtr tree = parser.parse(code);
    delete tree;

}