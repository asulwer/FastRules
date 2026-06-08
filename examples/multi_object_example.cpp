// multi_object_example.cpp
// Demonstrates multiple C++ objects in a single rule expression

#include <fastrules.hpp>
#include <iostream>
#include <vector>

struct Order {
    std::string id;
    double minTotal = 0.0;
    std::string status;

    bool operator==(const Order& o) const { return id == o.id && minTotal == o.minTotal; }
    bool operator<=(const Order& o) const { return minTotal <= o.minTotal; }
    bool operator<(const Order& o) const { return minTotal < o.minTotal; }
};

struct Customer {
    std::string name;
    double total = 0.0;
    bool vip = false;

    bool operator==(const Customer& o) const { return name == o.name && total == o.total; }
    bool operator<=(const Customer& o) const { return total <= o.total; }
    bool operator<(const Customer& o) const { return total < o.total; }
};

struct TestCase {
    std::string label;
    Customer* customer;
    Order* order;
};

int main() {
    try {
        fastrules::LuaEngine engine;

        // Register both types
        engine.registerType<Customer>("Customer", [](auto& ut) {
            ut["name"] = &Customer::name;
            ut["total"] = &Customer::total;
            ut["vip"] = &Customer::vip;
        });

        engine.registerType<Order>("Order", [](auto& ut) {
            ut["id"] = &Order::id;
            ut["minTotal"] = &Order::minTotal;
            ut["status"] = &Order::status;
        });

        // Rule that compares two different objects
        auto rule = std::make_shared<fastrules::Rule>();
        rule->id = "minimum-order";
        rule->expression = "customer.total >= order.minTotal";
        rule->isActive = true;

        // Rule that checks VIP status
        auto vipRule = std::make_shared<fastrules::Rule>();
        vipRule->id = "vip-check";
        vipRule->expression = "customer.vip == true";
        vipRule->isActive = true;

        // Rule that mutates based on both objects
        auto actionRule = std::make_shared<fastrules::Rule>();
        actionRule->id = "update-status";
        actionRule->expression = "order.minTotal > 100";
        actionRule->action = "order.status = 'priority'";
        actionRule->isActive = true;

        fastrules::Workflow workflow;
        workflow.rules = {rule, vipRule, actionRule};
        workflow.compile(engine);

        // Test data
        Customer customer{"Alice", 150.0, true};
        Order order{"ORD-123", 100.0, "pending"};

        Customer customer2{"Bob", 50.0, false};
        Order order2{"ORD-456", 200.0, "pending"};

        std::vector<TestCase> tests = {
            {"Alice + Order 123", &customer, &order},
            {"Bob + Order 456", &customer2, &order2}
        };

        for (const auto& test : tests) {
            std::cout << "\n=== " << test.label << " ===" << std::endl;
            std::cout << "  Customer total: " << test.customer->total << ", VIP: " << (test.customer->vip ? "Yes" : "No") << std::endl;
            std::cout << "  Order minTotal: " << test.order->minTotal << ", Status: " << test.order->status << std::endl;

            std::vector<fastrules::RuleParameter> params;
            params.emplace_back("customer", "Customer", std::any(test.customer));
            params.emplace_back("order", "Order", std::any(test.order));

            auto results = workflow.execute(engine, params);

            for (const auto& result : results) {
                std::cout << "  Rule: " << result.ruleId
                          << " - Success: " << (result.isSuccess() ? "Yes" : "No")
                          << std::endl;
            }

            std::cout << "  Final status: " << test.order->status << std::endl;
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
