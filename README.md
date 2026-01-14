# 🚀 StandX C++ Trading Client

> ⚠️ Work In Progress — This project is under active development; APIs may change. Test thoroughly before production use.

[English](#english) | [中文](#chinese)

---

<a name="english"></a>
## 🌟 English Version

### 📖 What is this?

A **C++ trading client** for [StandX](https://standx.com) perpetual trading with grid strategy support! 💪 This client handles authentication, order management, and implements automated grid trading strategies.

### ✨ Features

🔐 **Authentication**
- SIWE (Sign-In with Ethereum) login flow
- Ed25519 request signature verification
- Automatic token refresh on 401 errors

📊 **Market Data**
- Real-time ticker price queries
- Account balance checking (cross margin)
- Position monitoring with side detection

📈 **Order Management**
- Place orders with automatic side/type conversion
- Place TP (Take Profit) orders with qty sign handling
- Cancel orders by ID
- Query order details with status mapping
- Query unfilled orders

⚡ **Grid Trading Strategy**
- Long and short grid strategies
- Automatic position management
- TP order management
- Configurable grid size and intervals
- Multi-symbol support (BTC, ETH, SOL)

```cpp
#include "standx_client.h"

int main() {
    // Initialize client: chain, private_key_hex, symbol
    standx::StandXClient client("bsc", "your_private_key", "ETH-USD");

    // Login and get access token
    std::string token = client.login();

    // Query account balance
    float avail = 0.0f, total = 0.0f;
    if (client.balance(avail, total)) {
        // use avail/total
    }

    // Query positions
    std::vector<standx::Position> positions;
    if (client.positions(positions)) {
        // iterate positions
    }

    // Place a limit order example
    standx::Order order;
    order.side = "BUY";
    order.type = "LIMIT";
    order.size = 0.01f;
    order.price = 3000.0f;
    if (client.placeOrder(order)) {
        // order.id will be set after sync with unfilled orders
    }

    // Cancel order
    client.cancelOrder(order.id);

    return 0;
}
```

### 📚 API Reference

#### Authentication

```cpp
std::string login();                    // Login and get access token
std::string get_address() const;        // Get wallet address
```

#### Market Data / Account / Positions

```cpp
bool tickers(Ticker& tk);                     // Get latest ticker (returns true/false)
bool balance(float& availBal, float& totalBal);// Get account balance; writes to reference params
bool positions(std::vector<Position>& positions_list); // Get positions
```

#### Order Operations

```cpp
bool placeOrder(Order& order);                // Place order (LIMIT/MARKET). Order will be updated (id/status)
bool tpOrder(Order& order);                   // Place TP/reduce-only order. Order.tpId will be set
void cancelOrder(const std::string& id);      // Cancel order by ID
bool detail(Order& order);                    // Query order detail and update order.status
bool unfilledOrders(std::list<Order>& order_list); // Get unfilled orders list
```

### 🏗️ Architecture

```
cpp_standx_client/
├── src/
- `secretKey`: optional secret for integrations.
- `chain`: blockchain/network (e.g., `bsc`).
- `grid.long` / `grid.short`: enable long/short grid strategies.
- `order.*`: order-related defaults (leverage, min balance).
- `log.*`: logging configuration.
- `sub.*Size`: default contract sizes per symbol.

Alternatively, you can configure the client using `config.properties` in the project root. Example `config.properties`:

```properties
uid = main
secretKey = YOUR_SECRET_KEY_HERE
chain = bsc
grid.long = false
grid.short = true

order.lever = 10
order.minAvailBal = 20
order.blackList =
order.whiteList = ETH-USD

log.logName = log/default.log
log.logSize = 100M
log.logLevel = debug

bark.server =

sub.btcSize = 0.0001
sub.ethSize = 0.001
sub.solSize = 0.05
```

Key fields:
- `uid`: user identifier used for notifications.
- `secretKey`: optional secret for integrations.
- `chain`: blockchain/network (e.g., `bsc`).
- `grid.long` / `grid.short`: enable long/short grid strategies.
- `order.*`: order-related defaults (leverage, min balance).
- `log.*`: logging configuration.
- `sub.*Size`: default contract sizes per symbol.

### 🔨 Build

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 🎯 Quick Start

```cpp
#include "standx_client.h"

int main() {
    // Initialize logger
    logger::Tracer::Init();

    // Initialize client with symbol
    standx::StandXClient client("bsc", "your_private_key", "ETH-USD");

    // Login
    std::string token = client.login();

    // Query balance
    float availBal = 0.0f, totalBal = 0.0f;
    if (client.balance(availBal, totalBal)) {
        INFO("Available: " << availBal << ", Total: " << totalBal);
    }

    // Query positions
    std::vector<Position> positions;
    if (client.positions(positions)) {
        for (const auto& pos : positions) {
            INFO("Side: " << pos.positionSide << ", Amt: " << pos.positionAmt);
        }
    }

    // Place order
    Order order;
    order.side = "BUY";
    order.type = "LIMIT";
    order.size = 0.01f;
    order.price = 3000.0f;
    order.is_reduce_only = false;

    if (client.placeOrder(order)) {
        INFO("Order placed: " << order.id);
    }

    // Cancel order
    client.cancelOrder(order.id);

    return 0;
}
```

### 📚 API Reference

#### Authentication

```cpp
std::string login();                    // Login and get access token
std::string get_address() const;        // Get your wallet address
std::string getInstId() const;          // Get instrument ID (symbol)
```

#### Market Data

```cpp
bool tickers(Ticker& tk);                                    // Get ticker price
bool balance(float& availBal, float& totalBal);              // Get account balance
bool positions(std::vector<Position>& positions_list);       // Get positions
```

#### Order Operations

```cpp
// Place order
bool placeOrder(Order& order);

// Place TP order (with qty sign based on side)
bool tpOrder(Order& order);

// Cancel order by ID
void cancelOrder(const std::string& id);

// Query order detail
bool detail(Order& order);

// Query unfilled orders
bool unfilledOrders(std::list<Order>& order_list);
```

#### Order Structure

```cpp
struct Order {
    std::string id;              // Order ID (filled after placement)
    std::string contract;        // Symbol
    float size;                  // Quantity
    float price;                 // Price
    bool is_reduce_only;         // Reduce only flag
    std::string status;          // NEW, FILLED, CANCELED, FAILED
    std::string side;            // BUY, SELL
    std::string positionSide;    // LONG, SHORT
    std::string type;            // LIMIT, MARKET
    // ... other fields
};
```

### 🏗️ Architecture

```
cpp_standx_client/
├── src/
│   ├── crypto_utils.cpp/h    # 🔐 Crypto utilities (keccak256, base58, etc.)
│   ├── http_client.cpp/h     # 🌐 HTTP client with auto token refresh
│   ├── auth.cpp/h            # 🔑 SIWE authentication & Ed25519 signing
│   ├── standx_client.cpp/h   # 📊 Main trading client
│   ├── strategy.cpp/h        # ⚡ Grid trading strategy
│   ├── tracer.cpp/h          # 📝 Logging system
│   ├── util.cpp/h            # 🛠️ Utility functions
│   ├── data.h                # 📦 Data structures
│   ├── defines.h             # 🔧 Constants and macros
│   └── main.cpp              # 🎯 Example usage
└── CMakeLists.txt            # 🔧 Build configuration
```

### 🔒 Security Notes

- ✅ Uses EIP-191 personal_sign for SIWE authentication
- ✅ Ed25519 signatures for API request verification
- ✅ Independent Ed25519 keypair generated per session
- ✅ Integrated Keccak-256 implementation (no external deps)
- ⚠️ Keep your private key secure - never expose it in logs
- ⚠️ Use environment variables for sensitive data

### 🎮 Grid Strategy

The project includes a grid trading strategy implementation:

- **Long Grid**: Places buy orders below current price, sells above
- **Short Grid**: Places sell orders above current price, covers below
- **TP Management**: Automatic take-profit orders for filled positions
- **Position Monitoring**: Real-time position and order tracking
- **Multi-Symbol**: Supports BTC, ETH, SOL with custom parameters

Configure in `data.h` via `Config` struct:
```cpp
struct Config {
    float lever;                // Leverage
    float minAvailBal;         // Minimum available balance
    bool gridLong;             // Enable long grid
    bool gridShort;            // Enable short grid
    float subBtcSize;          // BTC order size
    float subEthSize;          // ETH order size
    float subSolSize;          // SOL order size
    // ... other fields
};
```

### 🤝 Contributing

Contributions are welcome! Feel free to:
- 🐛 Report bugs
- 💡 Suggest features
- 🔧 Submit pull requests

### 📄 License

MIT License - See LICENSE file for details

---

<a name="chinese"></a>
## 🌟 中文版本

### 📖 这是什么？

一个支持网格策略的 C++ 版 [StandX](https://standx.com) 永续合约交易客户端！💪 从身份验证到订单管理，并实现了自动化网格交易策略。

### ✨ 功能特性

🔐 **身份认证**
- SIWE (以太坊登录) 流程
- Ed25519 请求签名验证
- 401 错误自动刷新 Token

📊 **行情数据**
- 实时行情查询
- 账户余额查询（全仓模式）
- 持仓监控（自动识别方向）

📈 **订单管理**
- 下单（自动转换方向/类型为小写）
- 止盈单（根据方向自动处理数量正负）
- 按 ID 取消订单
- 查询订单详情（状态映射）
- 查询未成交订单

⚡ **网格交易策略**
- 多空网格策略
- 自动仓位管理
- 止盈单管理
- 可配置网格大小和间隔
- 多币种支持（BTC、ETH、SOL）

🛠️ **开发者友好**
- 模块化架构，清晰分离
- 自定义日志系统（tracer 宏）
- 安全的浮点/字符串转换工具
- 现代 C++17 代码

### 🔧 依赖项

- **OpenSSL 3.x** - 加密操作
- **libsodium** - Ed25519 签名
- **libsecp256k1** - 以太坊密钥操作
- **libcurl** - HTTP 请求
- **Poco** - 线程、时间、日志
- **nlohmann/json** - JSON 解析（仅头文件）

#### 📦 安装依赖 (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libssl-dev \
    libsodium-dev \
    libsecp256k1-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev
```

#### 🍎 安装依赖 (macOS)

```bash
brew install openssl@3 libsodium secp256k1 curl
```


### ⚙️ 配置

请在项目根目录使用 `config.properties` 进行配置（不再使用 `.env`）。示例 `config.properties`：

```properties
uid = main
secretKey = YOUR_SECRET_KEY_HERE
chain = bsc
grid.long = false
grid.short = true

order.lever = 10
order.minAvailBal = 20
order.blackList =
order.whiteList = ETH-USD

log.logName = log/default.log
log.logSize = 100M
log.logLevel = debug

bark.server =

sub.btcSize = 0.0001
sub.ethSize = 0.001
sub.solSize = 0.05
```

主要字段解释：
- `uid`：用于通知的用户标识。
- `secretKey`：可选的集成秘钥。
- `chain`：链/网络（例如 `bsc`）。
- `grid.long` / `grid.short`：启用多/空网格策略。
- `order.*`：下单相关默认值（杠杆，最小余额）。
- `log.*`：日志配置。
- `sub.*Size`：各合约的默认下单量。

或者，也可以使用项目根目录下的 `config.properties` 进行配置。示例 `config.properties`：

```properties
uid = main
secretKey = YOUR_SECRET_KEY_HERE
chain = bsc
grid.long = false
grid.short = true

order.lever = 10
order.minAvailBal = 20
order.blackList =
order.whiteList = ETH-USD

log.logName = log/default.log
log.logSize = 100M
log.logLevel = debug

bark.server =

sub.btcSize = 0.0001
sub.ethSize = 0.001
sub.solSize = 0.05
```

主要字段解释：
- `uid`：用于通知的用户标识。
- `secretKey`：可选的集成秘钥。
- `chain`：链/网络（例如 `bsc`）。
- `grid.long` / `grid.short`：启用多/空网格策略。
- `order.*`：下单相关默认值（杠杆，最小余额）。
- `log.*`：日志配置。
- `sub.*Size`：各合约的默认下单量。

### 🔨 编译

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 📚 API 参考

#### 身份认证

```cpp
std::string login();                    // 登录并获取访问令牌
std::string get_address() const;        // 获取钱包地址
```

#### 行情 / 账户 / 持仓

```cpp
bool tickers(Ticker& tk);                     // 获取最新行情（返回 true/false）
bool balance(float& availBal, float& totalBal);// 获取账户余额，结果写入引用参数
bool positions(std::vector<Position>& positions_list); // 获取持仓
```

#### 订单操作

```cpp
bool placeOrder(Order& order);                // 下单（limit/market），Order 会被更新（id/status）
bool tpOrder(Order& order);                   // 下止盈/减仓单（reduce-only），Order.tpId 会被填写
void cancelOrder(const std::string& id);      // 取消指定 ID 的订单
bool detail(Order& order);                    // 查询订单详情并更新 order.status
bool unfilledOrders(std::list<Order>& order_list); // 获取未成交订单列表
```

### 🏗️ 架构设计

```
cpp_standx_client/
├── src/
│   ├── crypto_utils.cpp/h    # 🔐 加密工具（keccak256、base58等）
│   ├── http_client.cpp/h     # 🌐 HTTP客户端（CURL优化）
│   ├── auth.cpp/h            # 🔑 SIWE认证 & Ed25519签名
│   ├── standx_client.cpp/h   # 📊 主交易客户端
│   └── main.cpp              # 🎯 使用示例
└── CMakeLists.txt           # 🔧 构建配置
```

### 🔒 安全提示

- ✅ 使用 EIP-191 personal_sign 进行 SIWE 认证
- ✅ Ed25519 签名验证 API 请求
- ✅ 每个会话独立生成 Ed25519 密钥对
- ⚠️ 妥善保管私钥 - 切勿暴露在日志中
- ⚠️ 使用环境变量存储敏感数据

### 🤝 参与贡献

欢迎贡献代码！你可以：
- 🐛 报告 Bug
- 💡 提出新功能建议
- 🔧 提交 Pull Request

### 📄 开源协议

MIT License - 详见 LICENSE 文件

---

**Made with ❤️ for the StandX community** 🚀
