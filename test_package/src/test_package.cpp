#include <fastrules.hpp>
#include <iostream>

using namespace fastrules;

int main() {
    LuaEngine engine;
    
    auto rule = Rule::builder("test")
        .withExpression("true")
        .withAction("result = 42")
        .build();
    
    std::cout << "FastRules test package OK" << std::endl;
    return 0;
}
