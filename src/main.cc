#include "ast.hh"

int main() {
    const char* code = R"(
        switch x {
            case 5
            case 6
                -> 10
            case _ -> 20
        }
        x := 5
    )";

    Parser parser;
    ExprPtr tree = parser.parse(code);

    if (tree) {
        tree->print();
        std::printf("\n");
    }
    delete tree;

}