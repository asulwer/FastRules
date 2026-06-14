// macro_example.cpp
// Demonstrates the FASTRULES_REGISTER_TYPE and FASTRULES_REGISTER_METHODS macros
// as alternatives to the lambda-based registerType syntax.

#include <fastrules.hpp>
#include <fastrules/type_registration_macro.hpp>
#include <iostream>
#include <vector>

// ============================================================================
// User-defined C++ struct
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

    // Methods callable from Lua
    bool isPremium() const { return balance > 1000.0; }
    std::string getTier() const {
        if (balance > 10000.0) return "platinum";
        if (balance > 1000.0) return "gold";
        return "standard";
    }
};

// ============================================================================
// Another struct: Order
// ============================================================================
struct Order {
    std::string id;
    double total = 0.0;
    std::string status;

    bool operator==(const Order& other) const {
        return id == other.id && total == other.total && status == other.status;
    }
    bool operator<=(const Order& other) const { return total <= other.total; }
    bool operator<(const Order& other) const { return total < other.total; }

    bool isShipped() const { return status == "shipped"; }
};

int main() {
    try {
        fastrules::LuaEngine engine;

        std::cout << "=== Macro-Based Type Registration Example ===" << std::endl << std::endl;

        // =====================================================================
        // 1. FASTRULES_REGISTER_TYPE -- register fields only (explicit count)
        // =====================================================================
        std::cout << "1. FASTRULES_REGISTER_TYPE_5(engine, Customer, name, age, processed, isActive, balance);"
                  << std::endl;
        FASTRULES_REGISTER_TYPE_5(engine, Customer, name, age, processed, isActive, balance);
        std::cout << "   Customer type registered with 5 fields." << std::endl << std::endl;

        // =====================================================================
        // 2. FASTRULES_REGISTER_METHODS -- register methods on the same type
        // =====================================================================
        std::cout << "2. FASTRULES_REGISTER_METHODS_2(engine, Customer, isPremium, getTier);"
                  << std::endl;
        FASTRULES_REGISTER_METHODS_2(engine, Customer, isPremium, getTier);
        std::cout << "   Customer methods registered (isPremium, getTier)." << std::endl << std::endl;

        // =====================================================================
        // 3. Register Order type using a combined fields+methods macro
        // =====================================================================
        std::cout << "3. FASTRULES_REGISTER_TYPE_WITH_METHODS_3_1(engine, Order, id, total, status, isShipped);"
                  << std::endl;
        FASTRULES_REGISTER_TYPE_WITH_METHODS_3_1(engine, Order, id, total, status, isShipped);
        std::cout << "   Order type registered with 3 fields and 1 method." << std::endl << std::endl;

        // =====================================================================
        // 4. Create rules using the macro-registered types
        // =====================================================================
        fastrules::Workflow workflow;
        workflow.description = "Macro-registered type validation";

        auto adultCheck = std::make_shared<fastrules::Rule>();
        adultCheck->id = 1;
        adultCheck->description = "Customer must be adult";
        adultCheck->expression = "customer.age >= 18";
        adultCheck->action = "customer.processed = true";
        workflow.rules.push_back(adultCheck);

        auto premiumCheck = std::make_shared<fastrules::Rule>();
        premiumCheck->id = 2;
        premiumCheck->description = "Premium status check";
        premiumCheck->expression = "customer:isPremium()";
        workflow.rules.push_back(premiumCheck);

        auto tierCheck = std::make_shared<fastrules::Rule>();
        tierCheck->id = 3;
        tierCheck->description = "Must be gold or platinum";
        tierCheck->expression = "customer:getTier() ~= 'standard'";
        workflow.rules.push_back(tierCheck);

        workflow.compile(engine);

        // =====================================================================
        // 5. Execute with test data
        // =====================================================================
        std::vector<Customer> customers = {
            {"Alice", 25, false, true, 100.0},    // standard
            {"Bob", 30, false, true, 5000.0},     // gold
            {"Carol", 35, false, true, 25000.0},  // platinum
            {"Dave", 15, false, true, 50.0},      // minor
        };

        for (const auto& customerData : customers) {
            Customer customer = customerData;

            std::cout << "----------------------------------------" << std::endl;
            std::cout << "Customer: " << customer.name
                      << " (age=" << customer.age
                      << ", balance=" << customer.balance
                      << ", tier=" << customer.getTier()
                      << ", premium=" << (customer.isPremium() ? "Yes" : "No")
                      << ")" << std::endl;

            std::vector<fastrules::RuleParameter> params;
            params.emplace_back("customer", &customer);

            auto results = workflow.execute(engine, params);

            for (const auto& result : results) {
                std::cout << "  Rule " << result.ruleName
                          << ": " << (result.isSuccess() ? "PASS" : "FAIL");
                if (result.exception.has_value()) {
                    std::cout << " [ERROR: " << result.exception.value().what() << "]";
                }
                std::cout << std::endl;
            }
            std::cout << "  processed=" << (customer.processed ? "Yes" : "No") << std::endl;
        }

        std::cout << "\n=== Macro Example Complete ===" << std::endl;
        return 0;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
