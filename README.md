# 🚀 StandX C++ Trading Client

> ⚠️ **开发中项目 / Work In Progress**
> 本项目正在积极开发中，API 接口可能随时变更。生产环境使用前请充分测试。
> This project is under active development. APIs may change. Test thoroughly before production use.

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

🛠️ **Developer Friendly**
- Modular architecture with clean separation
- Custom logging system with tracer macros
- Safe float/string conversion utilities
- Modern C++17 codebase

### 🔧 Dependencies

- **OpenSSL 3.x** - For cryptographic operations
- **libsodium** - For Ed25519 signatures
- **libsecp256k1** - For Ethereum key operations
- **libcurl** - For HTTP requests
- **Poco** - For threading, datetime, and logging
- **nlohmann/json** - For JSON parsing (header-only)

#### 📦 Installation (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libssl-dev \
    libsodium-dev \
    libsecp256k1-dev \
    libcurl4-openssl-dev \
    libpoco-dev
```

#### 🍎 Installation (macOS)

```bash
brew install openssl@3 libsodium secp256k1 curl poco
```

### ⚙️ Configuration

Create a `.env` file in the project root:

```bash
CHAIN=bsc
WALLET_PRIVATE_KEY_HEX=your_private_key_here_without_0x_prefix
```

⚠️ **Never commit your `.env` file!** Add it to `.gitignore`.

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
    libcurl4-openssl-dev
```

#### 🍎 安装依赖 (macOS)

```bash
brew install openssl@3 libsodium secp256k1 curl
```

### ⚙️ 配置

在项目根目录创建 `.env` 文件：

```bash
CHAIN=bsc
WALLET_PRIVATE_KEY_HEX=你的私钥_不带0x前缀
```

⚠️ **切勿提交 `.env` 文件！** 请将其加入 `.gitignore`。

### 🔨 编译

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 🎯 快速开始

```cpp
#include "standx_client.h"

int main() {
    // 初始化客户端
    standx::StandXClient client("bsc", "你的私钥");

    // 登录
    std::string token = client.login();

    // 查询余额
    std::string balance = client.query_balance();

    // 创建限价单
    std::string result = client.new_order(
        "ETH-USD",    // 交易对
        "buy",        // 方向
        "limit",      // 订单类型
        "0.01",       // 数量
        "gtc",        // 有效期类型
        false,        // 是否只减仓
        "3000"        // 价格
    );

    // 取消订单
    client.cancel_order(792209018);

    return 0;
}
```

### 📚 API 参考

#### 身份认证

```cpp
std::string login();                    // 登录并获取访问令牌
std::string get_address() const;        // 获取钱包地址
```

#### 行情数据

```cpp
std::string query_symbol_price(const std::string& symbol);  // 获取行情价格
std::string query_balance();                                 // 获取账户余额
std::string query_positions(const std::string& symbol = ""); // 获取持仓
```

#### 订单操作

```cpp
// 创建订单
std::string new_order(
    const std::string& symbol,
    const std::string& side,           // "buy" 或 "sell"
    const std::string& order_type,     // "limit", "market" 等
    const std::string& qty,
    const std::string& time_in_force,  // "gtc", "ioc", "fok", "alo"
    bool reduce_only,
    const std::string& price = ""      // 限价单必填
);

// 取消订单（提供 order_id 或 cl_ord_id）
std::string cancel_order(int order_id = -1, const std::string& cl_ord_id = "");

// 查询订单
std::string query_order(int order_id = -1, const std::string& cl_ord_id = "");

// 查询未成交订单
std::string query_open_orders(const std::string& symbol = "");
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
├── deps/                     # 📦 内嵌依赖（tiny_keccak）
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
