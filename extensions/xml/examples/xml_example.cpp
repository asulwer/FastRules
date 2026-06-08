// xml_example.cpp
// Demonstrates the fastrules-xml extension: XML file-based persistence
// for rules, workflows, and version history.

#include <fastrules.hpp>
#include <fastrules/xml_repository.hpp>
#include <iostream>

using namespace fastrules;
using namespace fastrules::ext;

int main() {
    try {
        LuaEngine engine;

        // ================================================================
        // 1. Create a workflow and rules programmatically
        // ================================================================
        Workflow workflow;
        workflow.id = "order-processing";
        workflow.description = "Process customer orders";

        auto inventoryCheck = std::make_shared<Rule>();
        inventoryCheck->id = "inventory-check";
        inventoryCheck->expression = "quantity <= stock";
        inventoryCheck->action = "inStock = true";

        auto shippingRule = std::make_shared<Rule>();
        shippingRule->id = "shipping-check";
        shippingRule->expression = "addressValid == true";
        shippingRule->action = "canShip = true";
        shippingRule->dependsOnRuleId = "inventory-check";

        workflow.rules.push_back(inventoryCheck);
        workflow.rules.push_back(shippingRule);

        // ================================================================
        // 2. Save rules to XML repository
        // ================================================================
        XmlRuleRepository repo("rules.xml");
        repo.save(*inventoryCheck);
        repo.save(*shippingRule);
        repo.flush();

        std::cout << "Saved " << repo.count() << " rules to rules.xml\n";

        // ================================================================
        // 3. Read rules back from XML
        // ================================================================
        auto loadedRule = repo.findById("inventory-check");
        if (loadedRule) {
            std::cout << "Loaded rule: " << loadedRule->id << "\n";
            std::cout << "  Expression: " << loadedRule->expression << "\n";
        }

        auto allRules = repo.findAll();
        std::cout << "Total rules in repository: " << allRules.size() << "\n";

        // ================================================================
        // 4. View raw XML output
        // ================================================================
        std::cout << "\n--- Raw XML ---\n";
        std::cout << repo.toString() << "\n";

        // ================================================================
        // 5. Execute using loaded rules
        // ================================================================
        workflow.compile(engine);

        std::vector<RuleParameter> params;
        int quantity = 5, stock = 10;
        bool addressValid = true;
        params.emplace_back("quantity", &quantity);
        params.emplace_back("stock", &stock);
        params.emplace_back("addressValid", &addressValid);

        auto results = workflow.execute(engine, params);
        for (const auto& result : results) {
            std::cout << "  Rule " << result.ruleId << ": " << (result.isSuccess() ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "\nXML extension example complete.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
