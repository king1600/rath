#include "ast.hh"

int main() {
    const char* code = "hi(5, 6);";

    Parser parser;
    ExprPtr tree = parser.parse(code);

    if (tree) {
        tree->print();
        std::printf("\n");
    }
    delete tree;

}