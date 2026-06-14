#include <fastrules.hpp>
#include <iostream>
#include <string>
#include <limits>

int main() {
    fastrules::LuaEngine engine;
    std::string line;

    std::cout << "FastRules REPL -- type Lua expressions (exit to quit)\n";
    std::cout << "Examples: 1 + 1, true and false, string.len(\"hello\")\n\n";

    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            if (std::cin.eof()) {
                std::cout << "\nEOF detected. Exiting.\n";
                break;
            }
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        try {
            auto ref = engine.compileExpression(line);
            if (!ref.has_value()) {
                std::cout << "Error: compilation failed\n";
                continue;
            }

            fastrules::RuleContext ctx;
            bool result = engine.evaluateExpression(ref.value(), {}, ctx);
            std::cout << "Result: " << (result ? "true" : "false") << "\n";
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
    }

    return 0;
}
