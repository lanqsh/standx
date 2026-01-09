# StandX C++ Example Client

这个示例展示如何用 C++ 实现 StandX 离线登录流程并查询持仓。示例主要工作流：

- 生成临时 ed25519 密钥对并用 base58 编码作为 `requestId`。
- 调用 `POST /v1/offchain/prepare-signin?chain=<chain>` 获取 `signedData` (JWT)。
- 解码 JWT payload 并读取 `message` 字段。
- 使用以太坊私钥对 `message` 做 `personal_sign`；示例用辅助的 Node.js 脚本（ethers）做签名，以避免在 C++ 中实现 secp256k1/keccak。
- 提交 `POST /v1/offchain/login?chain=<chain>` 获取 `accessToken`。
- 使用 `accessToken` 调用用户指定的持仓查询接口。

依赖
- CMake
- libsodium (生成 ed25519)
- libcurl
- nlohmann/json (开发包，头文件 `nlohmann/json.hpp`)
- Node.js + npm
- 在项目目录安装 ethers: `npm install ethers`

如果使用纯 C++ 签名（默认已实现），需要额外依赖：
- `libsecp256k1`（用于 secp256k1 签名/恢复）

在 Linux 上可以用包管理器安装或从源码编译 `libsecp256k1`。示例：

```bash
# on Ubuntu
sudo apt install libsecp256k1-dev
```

如果你不想安装 `libsecp256k1`，可以继续使用 Node/ethers 签名（示例中的旧实现已移除）。

构建示例

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

运行示例

```bash
./standx_client <chain> <wallet_private_key_hex> [positions_url]

# 例子：
# chain = 56 (BSC)
# positions_url 可以是 StandX 的持仓查询 REST 路径，例如 https://api.standx.com/v1/perps/positions （请按实际 API 调整）
```

注意
- Node 脚本用于签名：请确保不要在不安全环境下明文传递私钥。生产环境请把私钥隔离并安全调用签名服务。
- 示例未实现对 `signedData`（JWT）的完整公钥验证（需用 StandX 公钥验证签名），建议生产环境实现 JWT 验证。
