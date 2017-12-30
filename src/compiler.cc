#include "compiler.hh"

int Compiler::compile(const std::string& code) {
    try {
        ExprPtr tree = parser.parse("test.rath", code);
        if (!analyze(&tree)) return 1;

        if (tree) {
            tree->print();
            std::printf("\n");
        }

        delete tree;

    } catch (const std::exception& err) {
        std::fprintf(stderr, "%s\n", err.what());
        return 1;
    }

    return 0;
}