# 🚀 StandX C++ Trading Client

[English](#english) | [中文](#chinese)

---

<a name="english"></a>
## 🌟 English Version

### 📖 What is this?

A **production-ready** C++ client for [StandX](https://standx.com) perpetual trading! 💪 This client handles everything from authentication to order management with **automatic token refresh** and **body signature verification**.

### ✨ Features

🔐 **Authentication**
- SIWE (Sign-In with Ethereum) login flow
- Ed25519 request signature verification
- Automatic token refresh on 401 errors

📊 **Market Data**
- Real-time symbol price queries (no auth required)
- Account balance checking
- Position monitoring

📈 **Order Management**
- Create orders (Market, Limit, etc.)
- Cancel orders by ID or client order ID
- Query order status
- Query open orders

🛠️ **Developer Friendly**
- Modular architecture (crypto, http, auth, client)
- CURL optimization with connection reuse
- Detailed debug logging
- Clean C++17 codebase

### 🔧 Dependencies

- **OpenSSL 3.x** - For cryptographic operations
- **libsodium** - For Ed25519 signatures
- **libsecp256k1** - For Ethereum key operations
- **libcurl** - For HTTP requests
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
    libcurl4-openssl-dev
```

#### 🍎 Installation (macOS)

```bash
brew install openssl@3 libsodium secp256k1 curl
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
    // Initialize client
    standx::StandXClient client("bsc", "your_private_key");

    // Login
    std::string token = client.login();

    // Query balance
    std::string balance = client.query_balance();

    // Create a limit order
    std::string result = client.new_order(
        "ETH-USD",    // symbol
        "buy",        // side
        "limit",      // order_type
        "0.01",       // qty
        "gtc",        // time_in_force
        false,        // reduce_only
        "3000"        // price
    );

    // Cancel order
    client.cancel_order(792209018);

    return 0;
}
```

### 📚 API Reference

#### Authentication

```cpp
std::string login();                    // Login and get access token
std::string get_address() const;        // Get your wallet address
```

#### Market Data

```cpp
std::string query_symbol_price(const std::string& symbol);  // Get symbol price
std::string query_balance();                                 // Get account balance
std::string query_positions(const std::string& symbol = ""); // Get positions
```

#### Order Operations

```cpp
// Create order
std::string new_order(
    const std::string& symbol,
    const std::string& side,           // "buy" or "sell"
    const std::string& order_type,     // "limit", "market", etc.
    const std::string& qty,
    const std::string& time_in_force,  // "gtc", "ioc", "fok", "alo"
    bool reduce_only,
    const std::string& price = ""      // Required for limit orders
);

// Cancel order (provide order_id or cl_ord_id)
std::string cancel_order(int order_id = -1, const std::string& cl_ord_id = "");

// Query order
std::string query_order(int order_id = -1, const std::string& cl_ord_id = "");

// Query open orders
std::string query_open_orders(const std::string& symbol = "");
```

### 🏗️ Architecture

```
cpp_standx_client/
├── src/
│   ├── crypto_utils.cpp/h    # 🔐 Crypto utilities (keccak256, base58, etc.)
│   ├── http_client.cpp/h     # 🌐 HTTP client with CURL optimization
│   ├── auth.cpp/h            # 🔑 SIWE authentication & Ed25519 signing
│   ├── standx_client.cpp/h   # 📊 Main trading client
│   └── main.cpp              # 🎯 Example usage
├── deps/                     # 📦 Embedded dependencies (tiny_keccak)
└── CMakeLists.txt           # 🔧 Build configuration
```

### 🔒 Security Notes

- ✅ Uses EIP-191 personal_sign for SIWE authentication
- ✅ Ed25519 signatures for API request verification
- ✅ Independent Ed25519 keypair generated per session
- ⚠️ Keep your private key secure - never expose it in logs
- ⚠️ Use environment variables for sensitive data

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

一个**生产可用**的 C++ 版 [StandX](https://standx.com) 永续合约交易客户端！💪 从身份验证到订单管理，支持**自动刷新 Token** 和**请求体签名验证**。

### ✨ 功能特性

🔐 **身份认证**
- SIWE (以太坊登录) 流程
- Ed25519 请求签名验证
- 401 错误自动刷新 Token

📊 **行情数据**
- 实时行情查询（无需认证）
- 账户余额查询
- 持仓监控

📈 **订单管理**
- 创建订单（市价、限价等）
- 按 ID 或自定义 ID 取消订单
- 查询订单状态
- 查询未成交订单

🛠️ **开发者友好**
- 模块化架构（加密、HTTP、认证、客户端）
- CURL 连接复用优化
- 详细的调试日志
- 简洁的 C++17 代码

### 🔧 依赖项

- **OpenSSL 3.x** - 加密操作
- **libsodium** - Ed25519 签名
- **libsecp256k1** - 以太坊密钥操作
- **libcurl** - HTTP 请求
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
