# llama-gui-qt

llama.cpp `llama-server` 的 Qt 图形化启动器。在界面上选择模型、调整参数，由程序拼装命令行启动 / 停止 / 重启 `llama-server` 子进程，并实时查看日志与推理速度统计。

## 主要功能

- **模型管理**：选择 llama.cpp 工具目录（启动时自动在 PATH 中搜索 `llama-server`）、扫描模型目录下的所有 `.gguf` 文件、支持多模态 `mmproj` 文件
- **核心参数**：上下文窗口、CPU 线程数、Flash Attention、GPU 卸载层数（`-ngl`）、CPU MoE、内存映射、多卡拆分模式
- **显存与并发调优**：批大小 / 物理批大小（`-b`/`-ub`）、多卡显存比例（`-ts`）、主 GPU（`-mg`）、并发槽位数（`-np`）
- **上下文缓存**：KV 缓存量化（`-ctk`/`-ctv`）、初始 prompt 保留 token 数（`--keep`）、prompt 缓存上限（`--cache-ram`）、上下文检查点（`--ctx-checkpoints`）、上下文滑动（`--context-shift`）、统一 KV 缓冲（`-kvu`）
- **投机解码**：投机类型（`--spec-type`，含 `draft-mtp`/`draft-eagle3`/ngram 系列等 11 种）、起草 token 上下限（`--spec-draft-n-max`/`n-min`）、草稿模型（`--spec-draft-model`/`ngl`/`threads`/`cpu-moe`）、强度微调（`--spec-draft-p-split`/`p-min`）、ngram-mod / ngram-simple / ngram-map-k 系列参数、一键默认配置（`--spec-default`）
- **采样参数**：温度、top-k、top-p、min-p、重复惩罚、随机种子，作为请求未指定时的服务端默认采样值
- **服务与日志**：Metrics 监控端点（`--metrics`）、服务超时（`--timeout`）、日志写入文件（`--log-file`）、对话模板（`--chat-template`）、思考 token 预算（`--reasoning-budget`）
- **服务器控制**：监听地址 / 端口、API Key、model alias、CORS、WebUI 开关；一键启动 / 停止 / 重启
- **运行监控**：实时解析 server 输出，显示每个 slot 的 prompt 处理 / 生成速度与 token 统计，独立的运行日志、测试日志、服务器日志页
- **辅助工具**：命令行预览、生成 `.bat` 启动脚本、聊天测试、最优启动参数推荐（按显存自动估算）、参数配置的保存 / 加载

> 各参数的界面默认值取自 `llama-server --help` 标注的默认值；保持默认值（未勾选）时不会向启动命令输出该参数，保存配置文件时也不会写入对应的键。

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

### llama-server 命令行用法

GUI 只是把参数拼成下列命令行，了解原始用法有助于排查问题。

最基本的启动方式（只有 `-m` 是必填的）：

```bat
llama-server.exe -m 模型路径.gguf -c 8192
```

启动后默认监听 `127.0.0.1:8080`，浏览器打开 `http://localhost:8080` 即为内置 WebUI。一个较完整的手工启动示例：

```bat
cd /d D:\llama.cpp\cuda_full
llama-server.exe -m D:\models\qwen\Qwen3-8B-Q4_K_M.gguf ^
  -ngl 60 -c 8192 --spec-type draft-mtp --spec-draft-n-max 2 ^
  --host 127.0.0.1 --port 8090 -a Qwen3-8B-Q4_K_M --api-key sk-xxxx
```

常用参数按用途分组：

**模型加载**

| 参数 | 作用 |
|------|------|
| `-m, --model` | 模型文件路径，必填 |
| `-ngl, --gpu-layers` | 卸载到显存的层数，`99`/`all` 表示全部上卡 |
| `-c, --ctx-size` | 上下文长度，0 = 使用模型自带值 |
| `-mm, --mmproj` | 多模态投影文件（视觉模型需要） |
| `-a, --alias` | 对外暴露的模型名，客户端请求的 `model` 字段即它 |

**性能与显存**

| 参数 | 作用 |
|------|------|
| `-t, --threads` | CPU 线程数 |
| `-fa, --flash-attn [on\|off\|auto]` | Flash Attention，量化 KV 缓存时建议开启 |
| `--cpu-moe` | MoE 专家层放 CPU，节省显存 |
| `-ctk` / `-ctv` | KV 缓存量化类型，如 `-ctk q8_0 -ctv q8_0` |
| `-np, --parallel` | 并发 slot 数，-1 为自动 |
| `-b` / `-ub` | 逻辑 / 物理批大小，影响吞吐与显存峰值 |

**服务与网络**

| 参数 | 作用 |
|------|------|
| `--host` | 监听地址，默认 `127.0.0.1`；填 `0.0.0.0` 可被局域网访问 |
| `--port` | 监听端口，默认 8080 |
| `--api-key` | 访问密钥，客户端需带 `Authorization: Bearer <key>` |
| `--metrics` | 开启 Prometheus 兼容监控端点 |
| `--threads-http` | HTTP 请求处理线程数 |

**推理行为**

| 参数 | 作用 |
|------|------|
| `-rea, --reasoning [on\|off\|auto]` | 是否启用思考 / 推理链 |
| `--reasoning-budget N` | 思考 token 预算，0 = 立即结束思考 |
| `--jinja` | 使用模型自带 chat template，默认开启 |
| `--spec-type` / `--spec-draft-n-max` | 投机解码类型与每次起草 token 数，如 `--spec-type draft-mtp --spec-draft-n-max 2` |

### API 调用

服务启动后即为标准 HTTP 接口，可直接用 curl 或任意 OpenAI SDK（把 `base_url` 指向 `http://localhost:<port>/v1`）：

```bash
# 对话（OpenAI 兼容）
curl http://localhost:8090/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer sk-xxxx" \
  -d '{"model":"Qwen3-8B-Q4_K_M","messages":[{"role":"user","content":"你好"}],"stream":true}'

# 原生续写接口
curl http://localhost:8090/completion \
  -H "Content-Type: application/json" \
  -d '{"prompt":"写一个快速排序：","n_predict":128}'

# 向量化
curl http://localhost:8090/v1/embeddings \
  -H "Content-Type: application/json" \
  -d '{"input":"hello","model":"x"}'
```

其他常用端点：`/v1/messages`（Anthropic 兼容）、`/v1/responses`、`/tokenize`、`/detokenize`、`/reranking`、`/infill`（代码补全）、`/props`（服务状态）、`/slots`（各 slot 进度，本 GUI 即解析此类输出显示速度统计）、`/metrics`。

完整参数列表可执行 `llama-server.exe --help` 查看（本仓库的 `server_help.txt` 即一份完整输出）。显存不足时，依次尝试：降低 `-ngl` → 加 `--cpu-moe` → 开启 KV 量化（`-ctk q8_0 -ctv q8_0`，需配合 `-fa on`）→ 降低 `-c`。
