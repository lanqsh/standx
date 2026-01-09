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

        // Query positions (optional)
        // std::string positions = client.query_positions("ETH-USD");

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
