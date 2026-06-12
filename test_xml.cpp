#include <pugixml.hpp>
#include <iostream>
#include <fstream>

int main() {
    std::ifstream file("customer_validation.xml");
    if (!file.is_open()) {
        std::cerr << "Cannot open file" << std::endl;
        return 1;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    std::cout << "File size: " << content.size() << std::endl;
    std::cout << "First 100 chars: " << content.substr(0, 100) << std::endl;
    
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(content.c_str());
    
    if (!result) {
        std::cerr << "Parse error: " << result.description() << std::endl;
        std::cerr << "At offset: " << result.offset << std::endl;
        return 1;
    }
    
    std::cout << "Parse successful!" << std::endl;
    return 0;
}
