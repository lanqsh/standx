#include <fstream>
#include <map>

#include "data.h"
#include "standx_client.h"
#include "tracer.h"

Config kConfig;

std::map<std::string, std::string> load_env(const std::string& path = ".env") {
  std::map<std::string, std::string> env;
  std::ifstream ifs(path);
  if (!ifs.is_open()) return env;

  std::string line;
  while (std::getline(ifs, line)) {
    size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos || line[start] == '#') continue;

    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;

    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);

    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    val.erase(0, val.find_first_not_of(" \t"));
    val.erase(val.find_last_not_of(" \t") + 1);

    if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') ||
                            (val.front() == '\'' && val.back() == '\''))) {
      val = val.substr(1, val.size() - 2);
    }

    env[key] = val;
  }
  return env;
}

int main() {
  logger::Tracer::Init();

  try {
    auto env = load_env(".env");
    std::string chain = env["CHAIN"];
    std::string private_key = env["WALLET_PRIVATE_KEY_HEX"];

    if (chain.empty() || private_key.empty()) {
      ERROR("Missing CHAIN or WALLET_PRIVATE_KEY_HEX in .env file");
      return 1;
    }

    standx::StandXClient client(chain, private_key, "ETH-USD");
    INFO("Address: " << client.get_address());

    INFO("Querying ETH-USD price...");
    Ticker ticker;
    ticker.contract = "ETH-USD";
    if (client.tickers(ticker)) {
      INFO("Symbol price: " << ticker.last);
    }

    INFO("Logging in...");
    std::string token = client.login();
    INFO("Access token: " << token);

    INFO("Querying balance...");
    float availBal = 0.0f, totalBal = 0.0f;
    if (client.balance(availBal, totalBal)) {
      INFO("Balance - Available: " << availBal << ", Total: " << totalBal);
    }

    INFO("Querying order 784080731...");
    Order test_order;
    test_order.id = "784080731";
    if (client.detail(test_order)) {
        INFO("Order Detail: ID=" << test_order.id << ", Status=" << test_order.status
                  << ", Price=" << test_order.price << ", Size=" << test_order.size);
    } else {
        ERROR("Failed to query order detail");
    }

    INFO("Querying open orders...");
    std::list<Order> unfilled_orders;
    if (client.unfilledOrders(unfilled_orders)) {
        INFO("Unfilled Orders (" << unfilled_orders.size() << " found)");
        for (const auto& order : unfilled_orders) {
            INFO("  - ID: " << order.id << ", Side: " << order.side
                      << ", Price: " << order.price << ", Size: " << order.size);
        }
    } else {
        ERROR("Failed to query unfilled orders");
    }

    INFO("Querying positions for ETH-USD...");
    std::vector<Position> positions_list;
    if (client.positions(positions_list)) {
      INFO("Positions (" << positions_list.size() << " found)");
      for (const auto& pos : positions_list) {
        INFO("  - Side: " << pos.positionSide
                          << ", Amount: " << pos.positionAmt);
      }
    } else {
      ERROR("Failed to query positions");
    }

  } catch (const std::exception& e) {
    ERROR("Exception: " << e.what());
    return 1;
  }

  return 0;
}
