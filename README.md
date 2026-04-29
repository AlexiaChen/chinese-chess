# 中国象棋 / Chinese Chess

一款运行在浏览器里的中国象棋游戏，用 C++ 规则引擎（编译为 WebAssembly）驱动棋局逻辑，配合 [Pikafish](https://github.com/official-pikafish/Pikafish) 象棋 AI 引擎，前端使用 Vue 3 + Phaser 渲染。

**🌐 在线体验：[alexiachen.github.io/chinese-chess](https://alexiachen.github.io/chinese-chess)**

---

## 功能特性

- 完整的象棋规则核心（C++ 实现）
  - FEN 解析与序列化（兼容 Pikafish）
  - 全棋种合法走法生成
  - 将军检测、将帅对面规则、蹩腿马、塞象眼、隔山炮等
- 浏览器内 WebAssembly 规则引擎，无需后端
- Pikafish AI 引擎（官方象棋 AI，基于 Stockfish 改造）
- Vue 3 + Phaser 5 渲染棋盘，Tailwind CSS 布局
- 可直接在浏览器 DevTools 中测试的 JS API
- GitHub Actions 自动构建 + 部署到 GitHub Pages

---

## 技术栈

| 层 | 技术 |
|---|---|
| 规则引擎 | C++17，CMake |
| AI 引擎 | [Pikafish](https://github.com/official-pikafish/Pikafish)（git 子模块）|
| WASM 构建 | Emscripten |
| 前端框架 | Vue 3 + TypeScript |
| 打包工具 | Vite |
| CSS | Tailwind CSS |
| 棋盘渲染 | Phaser 3 |
| CI/CD | GitHub Actions |

---

## 前置依赖

在开始之前，请确认你的环境里已有：

| 工具 | 说明 |
|---|---|
| `git` | 拉取代码和子模块 |
| `cmake` ≥ 3.20 | 构建 C++ 核心 |
| `make` | 便捷命令入口 |
| `g++` / `clang++` | C++17 编译器 |
| `python3` | 运行测试夹具（`fake_uci_engine.py`）|
| `node` ≥ 18 + `npm` | 前端构建 |
| [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) | 编译 WASM（仅 WASM 构建需要）|

---

## 快速开始：在本地跑起来

### 第一步：克隆仓库

```bash
git clone --recursive https://github.com/AlexiaChen/chinese-chess.git
cd chinese-chess
```

如果已经 clone 但忘了加 `--recursive`：

```bash
git submodule update --init --depth 1 third_party/pikafish
```

### 第二步：构建 WASM 规则引擎

> 首次构建需要 Emscripten。如果你还没装，参考 [官方安装说明](https://emscripten.org/docs/getting_started/downloads.html)。

```bash
make wasm
```

这会把 `chinese_chess_wasm.js` 和 `chinese_chess_wasm.wasm` 输出到 `web/public/wasm/`，前端会自动加载。

### 第三步：安装前端依赖

```bash
make web-install
```

### 第四步：启动开发服务器

```bash
make web-dev
```

浏览器打开 [http://localhost:5173](http://localhost:5173)，即可开始下棋。

---

## 完整构建指南

### 原生 C++ 测试（不需要 Emscripten）

```bash
make test
```

这会依次执行：CMake 配置 → 编译 → CTest 运行。如果你想分步执行：

```bash
make configure   # cmake -S . -B build
make build       # cmake --build build
make test        # ctest --test-dir build --output-on-failure
```

### 编译 Pikafish AI 引擎（可选，本地 AI 对弈用）

```bash
make pikafish
```

如需指定架构（默认自动检测）：

```bash
PIKAFISH_ARCH=x86-64 make pikafish
```

### 验证 AI 引擎是否工作

```bash
make engine-smoke
```

成功输出示例：`bestmove b0c2`

如果 Pikafish 还没编译，想先用假引擎快速验证适配层：

```bash
make fake-engine-smoke
```

### 构建生产前端包

```bash
make web-build
```

### 预览生产包（本地）

```bash
make web-preview
# 打开 http://127.0.0.1:4173
```

---

## 所有 make 目标一览

```
make help
```

| 目标 | 功能 |
|---|---|
| `make configure` | CMake 配置 |
| `make build` | 编译原生目标 |
| `make test` | 运行测试套件 |
| `make native-cli` | 运行原生 CLI |
| `make fake-engine-smoke` | 用假 UCI 引擎测试适配层 |
| `make pikafish` | 编译内置 Pikafish 子模块 |
| `make engine-smoke` | 用真实 Pikafish 验证 |
| `make wasm` | 编译 WebAssembly 桥接模块 |
| `make web-install` | 安装前端 npm 依赖 |
| `make web-dev` | 启动 Vite 开发服务器 |
| `make web-build` | 构建生产前端包 |
| `make web-preview` | 本地预览生产包 |
| `make web-bundle` | 通过 CMake 构建前端包 |
| `make pages-build` | 构建 GitHub Pages 部署包 |
| `make clean` | 清除构建产物 |

---

## 项目结构

```
chinese-chess/
├── src/
│   ├── core/              # C++ 象棋规则引擎（FEN、走法生成、将军检测）
│   ├── engine/            # UCI 协议编解码器、Pikafish 子进程适配器
│   ├── bridge/            # 浏览器/WASM C ABI 桥接层
│   └── apps/cli/          # 原生 CLI 入口
├── tests/
│   ├── game_tests.cpp     # 规则引擎单元测试
│   └── fixtures/          # 测试夹具（fake UCI 引擎）
├── web/                   # Vue 3 + Vite + Phaser 前端
│   ├── src/
│   │   ├── App.vue        # 主页面组件（状态管理、WASM 加载）
│   │   ├── components/    # Phaser 棋盘组件
│   │   ├── game/          # 棋盘场景、FEN 工具
│   │   └── bridge/        # WASM 运行时桥接
│   └── public/wasm/       # 编译好的 WASM 产物（构建后生成，已 gitignore）
├── third_party/pikafish/  # Pikafish AI 引擎（git 子模块）
├── scripts/               # 构建辅助脚本
├── .github/workflows/     # CI 和 Pages 部署 workflow
└── Makefile               # 统一构建入口
```

---

## 在浏览器里测试规则引擎

页面加载后，DevTools Console 中可以直接调用：

```js
// 获取当前 FEN
window.__CHINESE_CHESS_TEST_API__.currentFen()

// 查询某格的合法走法
window.__CHINESE_CHESS_TEST_API__.legalMovesFrom('a3')
// => ['a3a4']

// 执行一步棋
window.__CHINESE_CHESS_TEST_API__.applyMove('a3a4')
// => true

// 重置棋局
window.__CHINESE_CHESS_TEST_API__.reset()
```

---

## CI / 部署

- **CI**（`.github/workflows/ci.yml`）：每次推送到 `master` 时，自动构建原生 C++ + 运行测试，并完整构建 WASM + 前端包。
- **Pages 部署**（`.github/workflows/pages.yml`）：构建成功后自动部署到 `https://alexiachen.github.io/chinese-chess/`。

> 首次部署前需要在仓库 **Settings → Pages → Source** 中选择 **GitHub Actions**。
