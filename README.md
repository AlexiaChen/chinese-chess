# 中国象棋 Chinese Chess

在浏览器里下象棋。规则引擎用 C++ 编写并编译为 WebAssembly，AI 对手运行在 Web Worker 中，棋盘渲染使用 Phaser，界面框架使用 Vue 3 + TypeScript + Tailwind CSS。

**🎮 [立即在线游玩 →](https://alexiachen.github.io/chinese-chess/)**

---

## 功能特性

### 游戏功能
- ✅ 完整象棋规则：将帅、士象、车马炮兵全部兵种，将军检测，将帅照面限制
- ✅ 人机对战：可选择执红方或黑方，AI 自动应对
- ✅ 悔棋：支持撤销上一步走法
- ✅ 棋盘自动翻转：人类执子的一方始终显示在近端
- ✅ 点击选子并高亮所有合法落点

### AI 引擎
- ✅ 内置 C++ 搜索引擎，编译为 WASM，浏览器端零服务端依赖
- ✅ Web Worker 异步搜索，AI 思考不阻塞 UI
- ✅ 内置开局库：红方早期主线直接返回，无需搜索
- ✅ 搜索算法：迭代加深 + 置换表（TT）+ 杀手启发 + 历史启发 + 静态搜索（QSearch）+ Null-Move Pruning + Aspiration Windows + 根节点 PVS
- ✅ 评估函数：材料分 + 棋子位置表 + 压将线特征（车/炮对准将线额外加分）
- ✅ 默认搜索预算：2000ms，最大深度 20 层
- ✅ AI 分析面板：实时显示最佳走法、评估分、完成深度、节点数、用时及主要变例（PV）

### 工程与部署
- ✅ C++ 规则引擎编译为 WASM，浏览器零服务端运行
- ✅ 走棋历史记录，FEN 实时显示
- ✅ GitHub Actions CI（原生测试 + WASM 构建 + 前端构建）
- ✅ 自动部署至 GitHub Pages

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

浏览器版本依赖 WASM 来运行 C++ 规则引擎和 AI 搜索。请先安装 [Emscripten](https://emscripten.org/docs/getting_started/downloads.html)，然后：

```bash
make wasm
```

这会生成 `web/public/wasm/chinese_chess_wasm.js` 和 `.wasm`。

> **注意**：WASM 目标编译时启用了 `-sENVIRONMENT=web,worker`，以支持在 Web Worker 内加载模块。

### 3. 启动前端开发服务器

```bash
make web-install   # 首次运行，安装 npm 依赖
make web-dev       # 启动 Vite 开发服务器
```

打开浏览器访问 **http://localhost:5173**，即可开始下棋。

---

## 所有构建命令

```
make help               # 查看所有可用命令
make test               # 构建 C++ 并运行全套测试
make wasm               # 构建 WebAssembly 桥接层（含 Worker 支持）
make web-install        # 安装前端 npm 依赖
make web-dev            # 启动前端开发服务器（热重载，http://localhost:5173）
make web-build          # 构建生产版前端包
make web-preview        # 本地预览生产包（http://localhost:4173）
make pages-build        # 构建 GitHub Pages 版（带 /chinese-chess/ 前缀）
make pikafish           # 构建 Pikafish 引擎（可选，仅本地原生模式）
make engine-smoke       # 用 Pikafish 引擎做快速冒烟测试
make fake-engine-smoke  # 用内置假引擎做冒烟测试
make clean              # 清除所有构建产物
```

---

## 运行测试

```bash
make test
```

这会用 CMake 配置项目、编译 C++ 核心和测试，并通过 CTest 运行全套测试。

当前测试覆盖：
- 规则引擎走法合法性、将军检测
- UCI 编解码（Pikafish 方言）
- WASM 桥接层
- 搜索引擎节点预算与搜索树压缩验证
- 评估函数压将线特征回归测试

---

## 项目架构

```
chinese-chess/
├── src/
│   ├── core/               # C++ 象棋规则引擎（走法生成、将军检测、FEN 解析）
│   ├── engine/
│   │   ├── search.h/.cpp       # 可移植搜索引擎（迭代加深 + TT + QSearch + Null-Move + PVS）
│   │   ├── opening_book.h/.cpp # 离线开局库（早期红方主线）
│   │   ├── uci_codec.h/.cpp    # Pikafish 兼容 UCI 坐标编解码
│   │   └── pikafish_process.h/.cpp  # 本地 UCI 子进程适配器（原生模式）
│   ├── bridge/
│   │   ├── browser_session.h/.cpp   # 带走棋历史的浏览器会话层
│   │   └── wasm_exports.cpp         # WASM C ABI 导出面
│   └── apps/cli/           # 本地命令行界面（测试/调试用）
├── web/                    # Vue 3 + Vite + TypeScript + Tailwind + Phaser 前端
│   ├── src/
│   │   ├── App.vue             # 主界面（选边、AI 流程、悔棋、AI 分析面板）
│   │   ├── bridge/
│   │   │   ├── wasmBridge.ts           # WASM 模块动态加载与 cwrap 绑定
│   │   │   └── aiSearchWorkerProtocol.ts # Worker 消息协议类型
│   │   ├── workers/
│   │   │   └── aiSearchWorker.ts       # Web Worker：独立加载 WASM，异步搜索
│   │   ├── components/
│   │   │   └── PhaserBoard.vue         # Phaser 棋盘组件 + DOM 覆盖层 + SVG 走法高亮
│   │   └── game/               # Phaser 场景 + FEN 工具函数
│   └── public/wasm/        # WASM 构建产物（由 make wasm 生成，已加入 .gitignore）
├── tests/                  # C++ 测试 + fake UCI 引擎 fixture
├── scripts/                # build_pikafish.sh、build_wasm.sh
├── third_party/
│   └── pikafish/           # Pikafish 引擎（git 子模块）
└── .github/workflows/
    ├── ci.yml              # 原生构建 + 测试 CI
    └── pages.yml           # GitHub Pages 自动部署
```

### 技术栈

| 层次 | 技术 |
|------|------|
| 规则引擎 & AI 搜索 | C++20，CMake |
| 浏览器运行时 | Emscripten → WebAssembly（web + worker 环境） |
| AI 异步执行 | Web Worker |
| 前端框架 | Vue 3 + TypeScript |
| 构建工具 | Vite 8 |
| 棋盘渲染 | Phaser 3 |
| 样式 | Tailwind CSS 4 |
| 可选引擎（本地） | Pikafish（UCI 协议，git 子模块） |
| CI/CD | GitHub Actions + GitHub Pages |

---

## AI 搜索说明

浏览器 AI 使用内置的可移植 C++ 搜索引擎（编译为 WASM），在独立的 Web Worker 线程中运行，不会阻塞 UI。

**搜索算法**（`src/engine/search.cpp`）：
- 迭代加深（Iterative Deepening）
- 置换表（Transposition Table）
- 杀手启发（Killer Heuristic）
- 历史启发（History Heuristic）
- 静态搜索（Quiescence Search）
- Null-Move Pruning
- Aspiration Windows
- 根节点主变例搜索（Root PVS）

**评估函数**：
- 各兵种材料分
- 棋子位置分（PST）
- 压将线特征：车/炮与对方将帅在同一文/横线时给予额外加分

**开局库**：内置红方早期主线，对局开始若干步内直接返回预设着法，跳过搜索。

**默认参数**：时间预算 2000ms，最大深度 20 层。

> Pikafish 引擎（`third_party/pikafish/`）仅用于本地原生模式，浏览器版本不使用 Pikafish。

---

## FEN 格式说明

本项目使用与 Pikafish 兼容的 FEN：

- 马 = `n/N`，象 = `b/B`（与国际象棋表示一致）
- 完整六字段格式，例如：

```
rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1
```

- UCI 坐标：`a0`（红方左下角）到 `i9`（黑方右上角），走法示例：`a0a1`
- `rank=0` 为黑方（顶部），`rank=9` 为红方（底部）

---

## 使用 Pikafish 引擎（可选，仅本地）

Pikafish 是专为中国象棋设计的强力引擎（基于 Stockfish 改造），以 git 子模块形式包含在本仓库中，**仅供本地原生模式使用**。

```bash
make pikafish      # 编译引擎（约需 2-5 分钟）
make engine-smoke  # 让引擎分析开局并返回最佳走法
```

---

## 贡献

欢迎 PR 和 Issue！开发前请先运行 `make test` 确认全套测试通过。
