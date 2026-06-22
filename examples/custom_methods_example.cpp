// custom_methods_example.cpp
// Demonstrates injecting user-defined C++ methods into Lua expressions and actions.
//
// This is the FastRules equivalent of RoslynRules' ability to inject C# methods.
//
// Two mechanisms:
//   1. Type Registration (registerType<T>) -- bind C++ struct methods/fields to Lua
//   2. Action Callbacks (registerAction) -- bind standalone C++ functions to Lua actions
//
// Build: cmake --build build --target custom_methods_example

#include <fastrules.hpp>
#include <iostream>
#include <vector>
#include <chrono>

// ============================================================================
// User-defined C++ types and functions
// ============================================================================

struct Customer {
    std::string name;
    int age = 0;
    bool processed = false;
    bool isActive = true;
    double balance = 0.0;

    // Comparison operators required by LuaBridge3 automagic
    bool operator==(const Customer& other) const {
        return name == other.name && age == other.age &&
               processed == other.processed && isActive == other.isActive &&
               balance == other.balance;
    }
    bool operator<=(const Customer& other) const { return age <= other.age; }
    bool operator<(const Customer& other) const { return age < other.age; }

    // C++ method we want to call from Lua
    bool isPremium() const { return balance > 1000.0; }

    // C++ method with parameters
    void addBalance(double amount) { balance += amount; }

    // C++ method that returns a computed value
    std::string getTier() const {
        if (balance > 10000.0) return "platinum";
        if (balance > 1000.0) return "gold";
        return "standard";
    }
};

// Standalone C++ function we want to call from Lua actions
void sendEmail(const std::string& to, const std::string& subject, const std::string& /*body*/) {
    std::cout << "  [EMAIL] To: " << to << ", Subject: " << subject << std::endl;
}

// Standalone C++ function for logging
void logAction(const std::string& ruleId, const std::string& customerName, bool success) {
    std::cout << "  [LOG] Rule '" << ruleId << "' for customer '" << customerName
              << "' - " << (success ? "PASSED" : "FAILED") << std::endl;
}

// Standalone C++ function that returns a value
std::string formatCurrency(double amount) {
    return "$" + std::to_string(amount).substr(0, std::to_string(amount).find('.') + 3);
}

// ============================================================================
// Main example
// ============================================================================

int main() {
    try {
        // Create engine
        fastrules::LuaEngine engine;

        // =====================================================================
        // 1. TYPE REGISTRATION -- bind C++ struct methods/fields to Lua
        // =====================================================================
        // After this, Lua expressions can:
        //   - Read fields: customer.name, customer.age, customer.balance
        //   - Call methods: customer:isPremium(), customer:getTier(), customer:addBalance(100)
        //   - Use in expressions: customer.age >= 18, customer:isPremium(), customer.balance > 500
        //
        engine.registerType<Customer>("Customer", [](auto& reg) {
            // Fields (read/write in both expressions and actions)
            reg.bind("name",      &Customer::name);
            reg.bind("age",       &Customer::age);
            reg.bind("processed", &Customer::processed);
            reg.bind("isActive",  &Customer::isActive);
            reg.bind("balance",   &Customer::balance);

            // Methods (call from expressions and actions)
            // NOTE: TypeRegistrar::method() only supports zero-argument methods.
            // For methods with arguments, use direct Lua or action callbacks.
            reg.method("isPremium",  &Customer::isPremium);   // no args, returns bool
            reg.method("getTier",    &Customer::getTier);     // no args, returns string
            // addBalance(double) is not registered -- use field mutation instead: customer.balance = customer.balance + 10
        });

        // =====================================================================
        // 2. ACTION CALLBACKS -- bind standalone C++ functions to Lua actions
        // =====================================================================
        // After this, Lua actions can call:
        //   sendEmail("alice@example.com", "Welcome", "...")
        //   logAction("rule-id", customer.name, true)
        //   formatCurrency(123.45) => prints formatted value
        //
        // NOTE: Action callbacks use std::any, not sol::object or LuaRef.
        // The signature is: void(const std::any& target, const std::vector<std::any>& args)
        //

        // Callback: sendEmail(target, args...)
        engine.registerAction("sendEmail", [](const std::any& /*target*/, const std::vector<std::any>& args) {
            // args[0] = to, args[1] = subject, args[2] = body
            if (args.size() >= 3) {
                sendEmail(
                    std::any_cast<std::string>(args[0]),
                    std::any_cast<std::string>(args[1]),
                    std::any_cast<std::string>(args[2])
                );
            }
        });

        // Callback: logAction(target, args...)
        engine.registerAction("logAction", [](const std::any& /*target*/, const std::vector<std::any>& args) {
            if (args.size() >= 3) {
                logAction(
                    std::any_cast<std::string>(args[0]),
                    std::any_cast<std::string>(args[1]),
                    std::any_cast<bool>(args[2])
                );
            }
        });

        // Callback: formatCurrency -- prints a formatted value
        engine.registerAction("formatCurrency", [](const std::any& /*target*/, const std::vector<std::any>& args) {
            if (!args.empty()) {
                double amount = 0.0;
                // Try to extract as double first, then int
                try {
                    amount = std::any_cast<double>(args[0]);
                } catch (...) {
                    try {
                        amount = static_cast<double>(std::any_cast<int>(args[0]));
                    } catch (...) {
                        // fallback
                    }
                }
                std::string formatted = formatCurrency(amount);
                std::cout << "  [FORMAT] " << amount << " => " << formatted << std::endl;
            }
        });

        // =====================================================================
        // 3. CREATE RULES USING INJECTED METHODS
        // =====================================================================

        fastrules::Workflow workflow;
        workflow.description = "Customer validation with custom C++ methods";

        // Rule 1: Check if customer is an adult using field access
        auto adultCheck = std::make_shared<fastrules::Rule>();
        adultCheck->id = 1;
        adultCheck->name = "adultCheck";
        adultCheck->description = "Customer must be 18+";
        adultCheck->expression = "customer.age >= 18";
        // Action uses both field mutation AND callback
        adultCheck->action = R"(
            customer.processed = true
            logAction("adult-check", customer.name, true)
        )";
        workflow.rules.push_back(adultCheck);

        // Rule 2: Check premium status using C++ method in expression
        auto premiumCheck = std::make_shared<fastrules::Rule>();
        premiumCheck->id = 2;
        premiumCheck->name = "premiumCheck";
        premiumCheck->description = "Premium customers get special handling";
        // Use : syntax for method calls on userdata
        premiumCheck->expression = "customer:isPremium()";
        premiumCheck->action = R"(
            sendEmail("premium@corp.com", "Premium Alert", "Customer " .. customer.name .. " is premium!")
            logAction("premium-check", customer.name, true)
        )";
        workflow.rules.push_back(premiumCheck);

        // Rule 3: Check tier using C++ method that returns string
        auto tierCheck = std::make_shared<fastrules::Rule>();
        tierCheck->id = 3;
        tierCheck->name = "tierCheck";
        tierCheck->description = "Check customer tier";
        // getTier() returns "standard", "gold", or "platinum"
        tierCheck->expression = "customer:getTier() ~= 'standard'";
        tierCheck->action = R"(
            logAction("tier-check", customer.name, true)
        )";
        workflow.rules.push_back(tierCheck);

        // Rule 4: Balance check with arithmetic
        auto balanceCheck = std::make_shared<fastrules::Rule>();
        balanceCheck->id = 4;
        balanceCheck->name = "balanceCheck";
        balanceCheck->description = "Check if balance is positive";
        balanceCheck->expression = "customer.balance > 0";
        balanceCheck->action = R"(
            formatCurrency(customer.balance)
            customer.balance = customer.balance + 10.0  -- mutate field directly
            logAction("balance-check", customer.name, true)
        )";
        workflow.rules.push_back(balanceCheck);

        // Rule 5: Complex expression combining fields and methods
        auto complexCheck = std::make_shared<fastrules::Rule>();
        complexCheck->id = 5;
        complexCheck->name = "complexCheck";
        complexCheck->description = "Active adult with positive balance";
        complexCheck->expression = "customer.isActive and customer.age >= 18 and customer.balance > 0";
        complexCheck->action = "logAction('complex-check', customer.name, true)";
        workflow.rules.push_back(complexCheck);

        // Compile
        workflow.compile(engine);

        // =====================================================================
        // 4. EXECUTE WITH TEST DATA
        // =====================================================================

        std::vector<std::pair<std::string, Customer>> tests = {
            {"Standard Adult",     Customer{"Alice", 25, false, true, 100.0}},
            {"Premium Customer",   Customer{"Bob", 30, false, true, 5000.0}},
            {"Platinum Customer",  Customer{"Carol", 35, false, true, 25000.0}},
            {"Minor",              Customer{"Dave", 15, false, true, 50.0}},
            {"Inactive",           Customer{"Eve", 40, false, false, 500.0}},
        };

        for (const auto& [label, customerData] : tests) {
            // Copy because actions mutate the object
            Customer customer = customerData;

            std::cout << "\n========================================" << std::endl;
            std::cout << "Customer: " << label << " (" << customer.name << ")" << std::endl;
            std::cout << "  Age: " << customer.age
                      << ", Balance: " << customer.balance
                      << ", Tier: " << customer.getTier()
                      << ", Premium: " << (customer.isPremium() ? "Yes" : "No") << std::endl;

            std::vector<fastrules::RuleParameter> params;
            // Pass pointer -- allows Lua to mutate the original C++ object
            params.emplace_back("customer", &customer);

            auto results = workflow.execute(engine, params);

            std::cout << "  Results:" << std::endl;
            for (const auto& result : results) {
                std::cout << "    " << result.ruleName
                          << ": " << (result.isSuccess() ? "PASS" : "FAIL");
                if (result.exception.has_value()) {
                    std::cout << " [ERROR: " << result.exception.value().what() << "]";
                }
                std::cout << std::endl;
            }

            std::cout << "  After execution:" << std::endl;
            std::cout << "    Processed: " << (customer.processed ? "Yes" : "No")
                      << ", Balance: " << customer.balance
                      << ", Tier: " << customer.getTier() << std::endl;
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
