# 中国象棋 Chinese Chess

在浏览器里下象棋。规则引擎用 C++ 编写并编译为 WebAssembly，棋盘渲染使用 Phaser，界面框架使用 Vue 3 + TypeScript + Tailwind CSS。

**🎮 [立即在线游玩 →](https://alexiachen.github.io/chinese-chess/)**

---

## 功能特性

- ✅ 完整象棋规则：将帅、士象、车马炮兵全部兵种，将军检测，将帅照面限制
- ✅ C++ 规则引擎编译为 WASM，在浏览器中零服务端运行
- ✅ Phaser 渲染棋盘，点击选子 + 高亮合法落点
- ✅ 支持 Pikafish 引擎（本地原生模式）
- ✅ 走棋历史记录，FEN 实时显示

---

## 本地运行（最快路径）

### 前提条件

| 工具 | 版本要求 |
|------|---------|
| CMake | ≥ 3.20 |
| C++ 编译器 | 支持 C++20（GCC 11+、Clang 13+） |
| Node.js | ≥ 22 |
| Emscripten | 任意新版本（仅构建 WASM 时需要） |
| Python 3 | 任意版本（仅运行测试 fixture 时需要） |

### 1. 克隆仓库

```bash
git clone --recursive https://github.com/AlexiaChen/chinese-chess.git
cd chinese-chess
```

如果已经克隆但忘了 `--recursive`：

```bash
git submodule update --init --depth 1 third_party/pikafish
```

### 2. 构建 WASM 桥接层

浏览器版本依赖 WASM 来运行 C++ 规则引擎。请先安装 [Emscripten](https://emscripten.org/docs/getting_started/downloads.html)，然后：

```bash
make wasm
```

这会生成 `web/public/wasm/chinese_chess_wasm.js` 和 `.wasm`。

### 3. 启动前端开发服务器

```bash
make web-install   # 首次运行，安装 npm 依赖
make web-dev       # 启动 Vite 开发服务器
```

打开浏览器访问 **http://localhost:5173**，即可开始下棋。

---

## 所有构建命令

```
make help          # 查看所有可用命令
make test          # 构建 C++ 并运行全套测试
make wasm          # 构建 WebAssembly 桥接层
make web-install   # 安装前端 npm 依赖
make web-dev       # 启动前端开发服务器（热重载）
make web-build     # 构建生产版前端包
make web-preview   # 本地预览生产包（http://localhost:4173）
make pages-build   # 构建 GitHub Pages 版（带 /chinese-chess/ 前缀）
make pikafish      # 构建 Pikafish 引擎（可选，仅本地原生模式）
make engine-smoke  # 用 Pikafish 引擎做快速冒烟测试
make clean         # 清除所有构建产物
```

---

## 运行测试

```bash
make test
```

这会用 CMake 配置项目、编译 C++ 核心和测试，并通过 CTest 运行全套测试。
当前测试覆盖：规则引擎走法合法性、将军检测、UCI 编解码、WASM 桥接层。

---

## 使用 Pikafish 引擎（可选）

Pikafish 是专为中国象棋设计的强力引擎（基于 Stockfish 改造）。它以 git 子模块的形式包含在本仓库中。

```bash
make pikafish      # 编译引擎（约需 2-5 分钟）
make engine-smoke  # 让引擎分析开局并返回最佳走法
```

注意：Pikafish 当前仅用于本地原生模式（CLI 和未来的人机对战功能）。浏览器版本使用纯 WASM 规则引擎，不调用 Pikafish。

---

## 项目架构

```
chinese-chess/
├── src/
│   ├── core/          # C++ 象棋规则引擎（走法生成、将军检测、FEN 解析）
│   ├── engine/        # UCI 进程适配器 + Pikafish 坐标编解码
│   ├── bridge/        # 浏览器会话层 + WASM C ABI 导出
│   └── apps/cli/      # 本地命令行界面（测试/调试用）
├── web/               # Vue 3 + Vite + TypeScript + Tailwind + Phaser 前端
│   ├── src/
│   │   ├── bridge/    # 动态加载 WASM 模块的 TypeScript 桥接层
│   │   ├── game/      # Phaser 场景 + FEN 工具函数
│   │   └── components/# Vue 组件（棋盘、布局）
│   └── public/wasm/   # WASM 构建产物（由 make wasm 生成，已加入 .gitignore）
├── tests/             # C++ 测试 + fake UCI 引擎 fixture
├── scripts/           # build_pikafish.sh、build_wasm.sh
├── third_party/
│   └── pikafish/      # Pikafish 引擎（git 子模块）
└── .github/workflows/
    ├── ci.yml         # 原生构建 + 测试 CI
    └── pages.yml      # GitHub Pages 自动部署
```

### 技术栈

| 层次 | 技术 |
|------|------|
| 规则引擎 | C++20，CMake |
| 浏览器运行时 | Emscripten → WebAssembly |
| 前端框架 | Vue 3 + TypeScript |
| 构建工具 | Vite 8 |
| 棋盘渲染 | Phaser 3 |
| 样式 | Tailwind CSS 4 |
| 引擎集成 | Pikafish（UCI 协议，git 子模块） |
| CI/CD | GitHub Actions + GitHub Pages |

---

## FEN 格式说明

本项目使用与 Pikafish 兼容的 FEN：

- 马 = `n/N`，象 = `b/B`（与国际象棋表示一致）
- 完整六字段格式，例如：

```
rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1
```

- UCI 坐标：`a0`（红方左下角）到 `i9`（黑方右上角），走法示例：`a0a1`

---

## 贡献

欢迎 PR 和 Issue！开发前请先运行 `make test` 确认基准测试通过。
