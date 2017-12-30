#include "compiler.hh"

int main() {
    const char* code = R"(
        "hello " + "world"
    )";

    Compiler compiler;
    return compiler.compile(code);
}