# llama-gui-qt

llama.cpp `llama-server` 的 Qt 图形化启动器。在界面上选择模型、调整参数，由程序拼装命令行启动 / 停止 / 重启 `llama-server` 子进程，并实时查看日志与推理速度统计。

## 主要功能

- **模型管理**：选择 llama.cpp 工具目录（启动时自动在 PATH 中搜索 `llama-server`）、扫描模型目录下的所有 `.gguf` 文件、支持多模态 `mmproj` 文件
- **核心参数**：上下文窗口、CPU 线程数、Flash Attention、GPU 卸载层数（`-ngl`）、CPU MoE、内存映射、多卡拆分模式、KV 缓存量化（`-ctk/-ctv`）、MTP 投机解码（`--spec-type draft-mtp`）等
- **服务器控制**：监听地址 / 端口、API Key、model alias、CORS、WebUI 开关；一键启动 / 停止 / 重启
- **运行监控**：实时解析 server 输出，显示每个 slot 的 prompt 处理 / 生成速度与 token 统计，独立的运行日志、测试日志、服务器日志页
- **辅助工具**：命令行预览、生成 `.bat` 启动脚本、聊天测试、最优启动参数推荐（按显存自动估算）、参数配置的保存 / 加载

## 环境依赖

| 依赖 | 说明 |
|------|------|
| Qt 6.x | 模块：`core`、`gui`、`widgets`；本机开发使用 6.9.1（MinGW 64-bit 版） |
| MinGW-w64 / MSVC | 任一支持 C++17 的编译器；MinGW 构建（GCC 13.1 验证通过） |
| qmake | 随 Qt 附带，用于生成 Makefile |
| llama.cpp 的 `llama-server` | 运行时依赖，不随本项目编译，需单独从 [llama.cpp releases](https://github.com/ggml-org/llama.cpp/releases) 下载或自行编译 |

> 想用 GPU 推理需下载 `cuda` / `vulkan` 等对应后端的 llama.cpp 版本，并确保显卡驱动正常（程序会尝试调用 `nvidia-smi` 检测显存）。

## 构建方法

### 方式一：Qt Creator（推荐）

1. 安装 Qt 6.x（勾选 MinGW 组件）与 Qt Creator；
2. 用 Qt Creator 打开 `llama_gui.pro`；
3. 配置 Kit 后直接「构建」→「运行」。

### 方式二：命令行（MinGW）

```bash
# 将 Qt 的 bin 与 MinGW 的 bin 加入 PATH，例如：
# set PATH=D:\Qt\6.9.1\mingw_64\bin;C:\mingw64\bin;%PATH%

qmake llama_gui.pro
mingw32-make -j8          # 或 make -j8 / jom
```

构建产物在 `release/llama_gui.exe`（Debug 在 `debug/`）。

## 使用说明

1. 启动 `llama_gui.exe`；
2. 「llama.cpp 工具地址」填入 `llama-server.exe` 所在目录（若已加入 PATH 会自动探测）；
3. 「GGUF 模型文件夹」填入模型目录，下拉框会列出其中所有 `.gguf` 文件；
4. 按需勾选参数，左侧「启动参数配置信息」会同步显示对应的命令行参数；
5. 点击启动按钮运行 `llama-server`，可在右侧页签查看日志与速度统计，或直接打开内置 WebUI。

参数配置可通过「保存启动参数 / 引入启动参数」导出为 JSON 文件复用。
