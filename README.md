# 🚀 StandX C++ Trading Client

> ✅ Project Status: Completed (maintenance mode). APIs are stable for current release.

[English](#english) | [中文](#chinese)

---

<a name="english"></a>
## 🌟 English

### 📖 Overview

A C++17 trading client for [StandX](https://standx.com) perpetual trading, including authentication, account/position queries, order operations, and grid strategy execution.

### ✨ Features

- SIWE authentication + Ed25519 request signing
- Auto token refresh on 401 responses
- Ticker, balance, and position queries
- Place/cancel/detail/unfilled order operations
- Long/short grid strategy support (BTC/ETH/SOL configurable)

### 🔧 Dependencies

- OpenSSL 3.x
- libsodium
- libsecp256k1
- libcurl
- Poco (Foundation, Util, Net, Data)
- nlohmann/json (header-only)

### 📦 Install Dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libssl-dev \
    libsodium-dev \
    libsecp256k1-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libpoco-dev
```

### ⚙️ Configuration

Use `config.properties` in project root.

```properties
uid = main
secretKey = YOUR_SECRET_KEY_HERE
chain = bsc
grid.long = false
grid.short = true
grid.size = 0.001
grid.step = 5

order.lever = 10
order.minAvailBal = 20
order.blackList =
order.whiteList = ETH-USD

log.logName = log/default.log
log.logSize = 100M
log.logLevel = debug

bark.server =
```

Key fields:
- `uid`: user identifier for notifications
- `secretKey`: optional integration secret
- `chain`: network name, e.g. `bsc`
- `grid.long` / `grid.short`: enable long/short grid
- `grid.size` / `grid.step`: grid order quantity and price step
- `order.*`: leverage, min balance, whitelist/blacklist settings
- `order.whiteList`: target symbol used by strategy startup (e.g. `ETH-USD`)
- `log.*`: logger output settings

### 🔨 Build

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 🎯 Quick Start

```cpp
#include "standx_client.h"
#include "data.h"

int main() {
    standx::StandXClient client("bsc", "your_private_key", "ETH-USD");

    std::string token = client.login();

    float availBal = 0.0f, totalBal = 0.0f;
    client.balance(availBal, totalBal);

    standx::Order order;
    order.side = "BUY";
    order.type = "LIMIT";
    order.size = 0.01f;
    order.price = 3000.0f;
    order.cl_ord_id = "demo-order-001";

    if (client.placeOrder(order)) {
        client.cancelOrder(order.cl_ord_id);
    }

    return 0;
}
```

Notes:
- `placeOrder` requires non-empty `order.cl_ord_id`.
- `tpOrder` requires non-empty `order.tp_cl_ord_id`.

### 📚 API Reference

```cpp
// Auth
std::string login();
std::string get_address() const;
std::string getInstId() const;

// Market / Account / Position
bool tickers(Ticker& tk);
bool balance(float& availBal, float& totalBal);
bool positions(std::vector<Position>& positions_list);

// Orders
bool placeOrder(Order& order);
bool tpOrder(Order& order);
void cancelOrder(const std::string& cl_ord_id);
bool detail(Order& order);
bool unfilledOrders(std::list<Order>& order_list);
```

### 🏗️ Architecture

```text
cpp_standx_client/
├── src/
│   ├── auth.cpp/h
│   ├── crypto_utils.cpp/h
│   ├── http_client.cpp/h
│   ├── standx_client.cpp/h
│   ├── strategy.cpp/h
│   ├── tracer.cpp/h
│   ├── util.cpp/h
│   ├── data.h
│   ├── defines.h
│   └── main.cpp
└── CMakeLists.txt
```

### 🔒 Security Notes

- Uses EIP-191 `personal_sign` for SIWE
- Uses Ed25519 signatures for API requests
- Generates an independent Ed25519 keypair per session
- Never expose private keys in logs

### 📄 License

MIT

---

<a name="chinese"></a>
## 🌟 中文

### 📖 项目简介

这是一个基于 C++17 的 [StandX](https://standx.com) 永续合约交易客户端，覆盖认证、账户/持仓查询、订单操作与网格策略。

### ✨ 功能特性

- SIWE 登录 + Ed25519 请求签名
- 401 自动刷新 Token
- 行情、余额、持仓查询
- 下单、止盈单、撤单、订单详情、未成交订单查询
- 支持多空网格策略（可配置 BTC/ETH/SOL）

### 🔧 依赖项

- OpenSSL 3.x
- libsodium
- libsecp256k1
- libcurl
- Poco（Foundation / Util / Net / Data）
- nlohmann/json（头文件）

### 📦 安装依赖（Ubuntu/Debian）

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libssl-dev \
    libsodium-dev \
    libsecp256k1-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libpoco-dev
```

### ⚙️ 配置说明

在项目根目录使用 `config.properties`：

```properties
uid = main
secretKey = YOUR_SECRET_KEY_HERE
chain = bsc
grid.long = false
grid.short = true
grid.size = 0.001
grid.step = 5

order.lever = 10
order.minAvailBal = 20
order.blackList =
order.whiteList = ETH-USD

log.logName = log/default.log
log.logSize = 100M
log.logLevel = debug

bark.server =
```

主要字段：
- `uid`：通知标识
- `secretKey`：可选集成密钥
- `chain`：链/网络（如 `bsc`）
- `grid.long` / `grid.short`：是否启用多/空网格
- `grid.size` / `grid.step`：网格下单数量与价格步长
- `order.*`：杠杆、最小余额、黑白名单
- `order.whiteList`：策略启动时使用的交易对（如 `ETH-USD`）
- `log.*`：日志配置

### 🔨 编译

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 📚 API 参考

```cpp
// 认证
std::string login();
std::string get_address() const;
std::string getInstId() const;

// 行情 / 账户 / 持仓
bool tickers(Ticker& tk);
bool balance(float& availBal, float& totalBal);
bool positions(std::vector<Position>& positions_list);

// 订单
bool placeOrder(Order& order);
bool tpOrder(Order& order);
void cancelOrder(const std::string& cl_ord_id);
bool detail(Order& order);
bool unfilledOrders(std::list<Order>& order_list);
```

说明：
- `placeOrder` 要求 `order.cl_ord_id` 非空。
- `tpOrder` 要求 `order.tp_cl_ord_id` 非空。

### 🏗️ 架构设计

```text
cpp_standx_client/
├── src/
│   ├── auth.cpp/h
│   ├── crypto_utils.cpp/h
│   ├── http_client.cpp/h
│   ├── standx_client.cpp/h
│   ├── strategy.cpp/h
│   ├── tracer.cpp/h
│   ├── util.cpp/h
│   ├── data.h
│   ├── defines.h
│   └── main.cpp
└── CMakeLists.txt
```

### 🔒 安全提示

- 使用 EIP-191 `personal_sign` 进行 SIWE 认证
- 使用 Ed25519 对 API 请求签名
- 每次会话独立生成 Ed25519 密钥对
- 请勿在日志中输出私钥

### 📄 开源协议

MIT

---

**Made with ❤️ for the StandX community**
