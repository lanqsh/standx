#include "standx_client.h"
#include <iostream>
#include <fstream>
#include <map>

// Load .env file
std::map<std::string, std::string> load_env(const std::string& path = ".env") {
    std::map<std::string, std::string> env;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return env;

    std::string line;
    while (std::getline(ifs, line)) {
        // Trim and skip comments
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos || line[start] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Trim
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);

        // Remove quotes
        if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') ||
                                 (val.front() == '\'' && val.back() == '\''))) {
            val = val.substr(1, val.size() - 2);
        }

        env[key] = val;
    }
    return env;
}

int main() {
    try {
        // Load config
        auto env = load_env(".env");
        std::string chain = env["CHAIN"];
        std::string private_key = env["WALLET_PRIVATE_KEY_HEX"];

        if (chain.empty() || private_key.empty()) {
            std::cerr << "Error: Missing CHAIN or WALLET_PRIVATE_KEY_HEX in .env file\n";
            return 1;
        }

        // Create client
        standx::StandXClient client(chain, private_key);
        std::cout << "Address: " << client.get_address() << "\n\n";

        // Query symbol price (no auth required)
        std::cout << "Querying ETH-USD price...\n";
        std::string price = client.query_symbol_price("ETH-USD");
        std::cout << "Symbol Price:\n" << price << "\n\n";

        // Login
        std::cout << "Logging in...\n";
        std::string token = client.login();
        std::cout << "Access token: " << token << "\n\n";

        // Query balance
        std::cout << "Querying balance...\n";
        std::string balance = client.query_balance();
        std::cout << "Balance:\n" << balance << "\n\n";

        // Query order
        std::cout << "Querying order 784080731...\n";
        std::string order = client.query_order(784080731);
        std::cout << "Order:\n" << order << "\n\n";

        // Query open orders
        std::cout << "Querying open orders...\n";
        std::string open_orders = client.query_open_orders();
        std::cout << "Open Orders:\n" << open_orders << "\n\n";

        // Query positions
        std::cout << "Querying positions for ETH-USD...\n";
        std::string positions = client.query_positions("ETH-USD");
        std::cout << "Positions:\n" << positions << "\n\n";

        // Create new order
        // std::cout << "Creating new order: ETH-USD limit buy @ 3000, qty=0.001...\n";
        // std::string new_order_result = client.new_order(
        //     "ETH-USD",      // symbol
        //     "buy",          // side (多单)
        //     "limit",        // order_type
        //     "0.001",        // qty
        //     "alo",          // time_in_force
        //     false,          // reduce_only
        //     "3000"          // price
        // );
        // std::cout << "New Order Result:\n" << new_order_result << "\n\n";

        // Cancel order
        std::cout << "Canceling order 792209018...\n";
        std::string cancel_result = client.cancel_order(792209018);
        std::cout << "Cancel Order Result:\n" << cancel_result << "\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
