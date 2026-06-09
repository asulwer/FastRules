// custom_methods_example.cpp
// Demonstrates injecting user-defined C++ methods into Lua expressions and actions.
//
// This is the FastRules equivalent of RoslynRules' ability to inject C# methods.
//
// Two mechanisms:
//   1. Type Registration (registerType<T>) — bind C++ struct methods/fields to Lua
//   2. Action Callbacks (registerAction) — bind standalone C++ functions to Lua actions
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

    // Comparison operators required by sol2 automagic
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
void sendEmail(const std::string& to, const std::string& subject, const std::string& body) {
    std::cout << "  [EMAIL] To: " << to << ", Subject: " << subject << std::endl;
}

// Standalone C++ function for logging
void logAction(const std::string& ruleId, const std::string& customerName, bool success) {
    std::cout << "  [LOG] Rule '" << ruleId << "' for customer '" << customerName
              << "' — " << (success ? "PASSED" : "FAILED") << std::endl;
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
        // 1. TYPE REGISTRATION — bind C++ struct methods/fields to Lua
        // =====================================================================
        // After this, Lua expressions can:
        //   - Read fields: customer.name, customer.age, customer.balance
        //   - Call methods: customer:isPremium(), customer:getTier(), customer:addBalance(100)
        //   - Use in expressions: customer.age >= 18, customer:isPremium(), customer.balance > 500
        //
        engine.registerType<Customer>("Customer", [](auto& ut) {
            // Fields (read/write in both expressions and actions)
            ut["name"]      = &Customer::name;
            ut["age"]       = &Customer::age;
            ut["processed"] = &Customer::processed;
            ut["isActive"]  = &Customer::isActive;
            ut["balance"]   = &Customer::balance;

            // Methods (call from expressions and actions)
            ut["isPremium"]  = &Customer::isPremium;   // no args, returns bool
            ut["getTier"]    = &Customer::getTier;     // no args, returns string
            ut["addBalance"] = &Customer::addBalance;  // one arg (double), void return
        });

        // =====================================================================
        // 2. ACTION CALLBACKS — bind standalone C++ functions to Lua actions
        // =====================================================================
        // After this, Lua actions can call:
        //   callbacks.sendEmail("alice@example.com", "Welcome", "...")
        //   callbacks.logAction("rule-id", customer.name, true)
        //   callbacks.formatCurrency(123.45) → returns "$123.45"
        //

        // Callback: sendEmail(target, args...) — target is first arg, rest are in args vector
        engine.registerAction("sendEmail", [](sol::object target, const std::vector<sol::object>& args) {
            // args[0] = to, args[1] = subject, args[2] = body
            if (args.size() >= 3) {
                sendEmail(
                    args[0].as<std::string>(),
                    args[1].as<std::string>(),
                    args[2].as<std::string>()
                );
            }
        });

        // Callback: logAction(target, args...)
        engine.registerAction("logAction", [](sol::object target, const std::vector<sol::object>& args) {
            if (args.size() >= 3) {
                logAction(
                    args[0].as<std::string>(),
                    args[1].as<std::string>(),
                    args[2].as<bool>()
                );
            }
        });

        // Callback: formatCurrency — this one returns a value via target
        engine.registerAction("formatCurrency", [](sol::object target, const std::vector<sol::object>& args) {
            if (!args.empty()) {
                double amount = args[0].as<double>();
                std::string formatted = formatCurrency(amount);
                // Note: Action callbacks can't directly return values to Lua assignments.
                // For value-returning functions, use TypeRegistry methods instead.
                std::cout << "  [FORMAT] " << amount << " → " << formatted << std::endl;
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
        adultCheck->description = "Customer must be 18+";
        adultCheck->expression = "customer.age >= 18";
        // Action uses both field mutation AND callback
        adultCheck->action = R"(
            customer.processed = true
            callbacks.logAction("adult-check", customer.name, true)
        )";
        workflow.rules.push_back(adultCheck);

        // Rule 2: Check premium status using C++ method in expression
        auto premiumCheck = std::make_shared<fastrules::Rule>();
        premiumCheck->id = 2;
        premiumCheck->description = "Premium customers get special handling";
        // Use : syntax for method calls on userdata
        premiumCheck->expression = "customer:isPremium()";
        premiumCheck->action = R"(
            callbacks.sendEmail("premium@corp.com", "Premium Alert", "Customer " .. customer.name .. " is premium!")
            callbacks.logAction("premium-check", customer.name, true)
        )";
        workflow.rules.push_back(premiumCheck);

        // Rule 3: Check tier using C++ method that returns string
        auto tierCheck = std::make_shared<fastrules::Rule>();
        tierCheck->id = 3;
        tierCheck->description = "Check customer tier";
        // getTier() returns "standard", "gold", or "platinum"
        tierCheck->expression = "customer:getTier() ~= 'standard'";
        tierCheck->action = R"(
            callbacks.logAction("tier-check", customer.name, true)
        )";
        workflow.rules.push_back(tierCheck);

        // Rule 4: Balance check with arithmetic
        auto balanceCheck = std::make_shared<fastrules::Rule>();
        balanceCheck->id = 4;
        balanceCheck->description = "Check if balance is positive";
        balanceCheck->expression = "customer.balance > 0";
        balanceCheck->action = R"(
            callbacks.formatCurrency(customer.balance)
            customer:addBalance(10.0)  -- call C++ method with argument
            callbacks.logAction("balance-check", customer.name, true)
        )";
        workflow.rules.push_back(balanceCheck);

        // Rule 5: Complex expression combining fields and methods
        auto complexCheck = std::make_shared<fastrules::Rule>();
        complexCheck->id = 5;
        complexCheck->description = "Active adult with positive balance";
        complexCheck->expression = "customer.isActive and customer.age >= 18 and customer.balance > 0";
        complexCheck->action = "callbacks.logAction('complex-check', customer.name, true)";
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
            // Pass pointer — allows Lua to mutate the original C++ object
            params.emplace_back("customer", &customer);

            auto results = workflow.execute(engine, params);

            std::cout << "  Results:" << std::endl;
            for (const auto& result : results) {
                std::cout << "    " << result.ruleId
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


