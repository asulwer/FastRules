// xml_example.cpp
// Demonstrates the fastrules-xml extension: loading workflows from XML files
// and using XML repository for persistence.

#include <fastrules.hpp>
#include <fastrules/xml_repository.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace fastrules;
using namespace fastrules::ext;

static std::string resolveDataPath(const std::string& filename) {
    auto exePath = std::filesystem::current_path();
    for (int i = 0; i < 6; ++i) {
        auto p = exePath / "data" / "xml" / filename;
        if (std::filesystem::exists(p)) return p.string();
        auto parent = exePath.parent_path();
        if (parent == exePath) break;
        exePath = parent;
    }
    auto fallback = std::filesystem::current_path() / filename;
    return fallback.string();
}

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open: " + path);
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

int main() {
    try {
        LuaEngine engine;

        // ================================================================
        // 1. Load rules from XML data file
        // ================================================================
        std::string xmlPath = resolveDataPath("rules.xml");
        std::string xmlContent = readFile(xmlPath);
        std::cout << "Loaded XML from: " << xmlPath << "\n";
        std::cout << "Content:\n" << xmlContent << "\n";

        // ================================================================
        // 2. Create workflow from loaded rules
        // ================================================================
        Workflow workflow;
        workflow.id = 1;
        workflow.description = "Process customer orders";

        auto inventoryCheck = std::make_shared<Rule>();
        inventoryCheck->id = 1;
        inventoryCheck->expression = "quantity <= stock";
        inventoryCheck->action = "inStock = true";

        auto shippingRule = std::make_shared<Rule>();
        shippingRule->id = 2;
        shippingRule->expression = "addressValid == true";
        shippingRule->action = "canShip = true";
        shippingRule->dependsOnRuleId = 1;

        workflow.rules.push_back(inventoryCheck);
        workflow.rules.push_back(shippingRule);

        // ================================================================
        // 3. Save rules to XML repository
        // ================================================================
        XmlRuleRepository repo("rules.xml");
        repo.save(*inventoryCheck);
        repo.save(*shippingRule);
        repo.flush();

        std::cout << "Saved " << repo.count() << " rules to rules.xml\n";

        // ================================================================
        // 4. Read rules back from XML
        // ================================================================
        auto loadedRule = repo.findById(1);
        if (loadedRule) {
            std::cout << "Loaded rule: " << loadedRule->id << "\n";
            std::cout << "  Expression: " << loadedRule->expression << "\n";
        }

        auto allRules = repo.findAll();
        std::cout << "Total rules in repository: " << allRules.size() << "\n";

        // ================================================================
        // 5. View raw XML output
        // ================================================================
        std::cout << "\n--- Raw XML ---\n";
        std::cout << repo.toString() << "\n";

        // ================================================================
        // 6. Execute using loaded rules
        // ================================================================
        workflow.compile(engine);

        std::vector<RuleParameter> params;
        int quantity = 5, stock = 10;
        bool addressValid = true;
        params.emplace_back("quantity", quantity);
        params.emplace_back("stock", stock);
        params.emplace_back("addressValid", addressValid);

        auto results = workflow.execute(engine, params);
        for (const auto& result : results) {
            std::cout << "Rule " << result.ruleId << ": "
                      << (result.isSuccess() ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "\nXML extension example complete.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
