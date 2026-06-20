# qlocate

一个快速、跨平台的文件定位工具，灵感来自 Unix 的 `locate`。qlocate 会为你的文件系统
构建一个紧凑的二进制索引，让你能够用通配符模式按文件名或路径**即时搜索**。它在单个
二进制文件中同时提供 **Qt 图形界面**和**命令行界面**。

## 特性

- **即时搜索**：基于预先构建的索引，每次查询无需实时遍历文件系统。
- **一个二进制、两种界面**：无参数启动图形界面，带参数则进入命令行模式。
- **通配符匹配**：支持 `*` 和 `?`，大小写不敏感。
- **按文件名或完整路径匹配**：当模式中包含 `/` 时自动切换为路径匹配。
- **仅可执行文件过滤**（`-x`）：用于查找可运行的文件。
- **多索引（“数据库”）**：每个索引拥有自己的包含/排除路径。
- **目录忽略列表**（如 `node_modules`、`.git`、`__pycache__`）：在建立索引时跳过噪声。
- **紧凑的自定义二进制格式**（`FILESNAP` v5）：使用父记录位置的反向引用，而非为
  每个条目存储完整路径。
- **跨平台**：macOS、Linux 和 Windows（Qt5 或 Qt6）。

## 工作原理

qlocate 将*建立索引*与*搜索*分离：

1. **建立索引**（`--update`）会递归扫描配置的 `include_paths`，跳过符号链接、被排除的
   路径以及被忽略的目录名。每个条目以固定大小的记录（flags、mode、name）存储，其中
   `flags` 打包了父记录的位置、一个“是否含有子目录”位和一个“是否为文件”位。完整路径
   在搜索时通过沿父引用回溯来重建，因此索引保持小巧。
2. **搜索**会将索引载入内存，并对每个文件名或每个重建出的完整路径运行递归通配符匹配。

索引文件会写入每个数据库所配置的 `filename`（例如 `~/.local/share/qlocate/main.idx`）。

## 构建

### 依赖要求

- 支持 C++20 的编译器
- CMake ≥ 3.16
- Qt 6（优先）或 Qt 5 —— `Widgets` 组件

构建会优先选择 Qt6，并在缺失时自动回退到 Qt5。

### 构建步骤

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

如果 Qt 安装在非标准位置，可向 CMake 指明路径：

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt
```

单头文件依赖 [`toml.hpp`](third_party/toml.hpp) 已内置在 `third_party/` 下，因此解析配置
无需额外安装任何包。

## 配置

qlocate 读取一个 TOML 配置文件，按以下顺序查找：

1. 与可执行文件同目录的 `qlocate.toml`
2. `~/.config/qlocate/qlocate.toml`

你也可以通过 `-c <file>` 显式指定路径。

示例（`src/qlocate.toml`）：

```toml
[settings]
file_limit_interactive = 50      # 图形界面输入时显示的最大结果数
file_limit_search      = 2000    # 显式搜索的最大结果数

[[database]]
name           = "main"
filename       = "~/.local/share/qlocate/main.idx"
include_paths  = ["/home/user/projects", "/home/user/Documents"]
exclude_paths  = []
ignore_dirnames = ["node_modules", ".git", ".svn", "__pycache__", ".cache", "build", "dist"]
default_update = true            # 普通“更新索引”操作时是否包含此库
```

### 配置项参考

**`[settings]`**

| 键                       | 默认值  | 说明                                       |
| ------------------------ | ------- | ------------------------------------------ |
| `file_limit_interactive` | `50`    | 图形界面实时（边输入边搜）的结果数上限。   |
| `file_limit_search`      | `2000`  | 显式搜索的结果数上限。                     |

**`[[database]]`**（一个或多个；这是一个 TOML 表数组）

| 键                | 默认值      | 说明                                                             |
| ----------------- | ----------- | ---------------------------------------------------------------- |
| `name`            | `"default"` | 索引的可读标签。                                                 |
| `filename`        | `""`        | 要写入/读取的索引文件路径。                                      |
| `include_paths`   | `[]`        | 要建立索引的根目录。                                             |
| `exclude_paths`   | `[]`        | 要跳过的绝对路径（保留该条目，但不索引其内容）。                 |
| `ignore_dirnames` | `[]`        | 任意位置都要跳过的目录**名**（如 `node_modules`、`.git`）。      |
| `default_update`  | `true`      | 普通更新时是否重建此数据库。                                     |

## 用法

### 构建 / 刷新索引

```bash
qlocate --update      # 或 -u
```

这会重建所有已配置的数据库并打印进度。

### 命令行搜索

```bash
qlocate <pattern> [<pattern> ...]   # 按文件名 / 路径搜索
qlocate -x <pattern>                # 仅可执行文件
qlocate -c myconfig.toml <pattern>  # 使用指定的配置文件
```

示例：

```bash
qlocate '*.pdf'             # 所有 PDF
qlocate 'report?'           # report1、reportA、……
qlocate '/projects/*/main*' # 完整路径匹配（模式包含 '/'）
qlocate -x build            # 名字类似 "build" 的可执行文件
```

如果尚未建立索引，qlocate 会提醒你先运行 `--update`。

### 图形界面

不带参数运行即可打开图形界面：

```bash
qlocate
```

- 输入至少 2 个字符即触发实时搜索（除非模式包含 `/`，否则会自动补一个尾部 `*`）。
- 按 **Enter** 或点击 **Search** 进行完整搜索。
- 勾选 **Exec only** 将结果限定为可执行文件。
- 点击 **Update Index** 重建 `default_update = true` 的数据库；点击时按住 **Shift**
  可强制重建*所有*数据库。
- **双击**某个结果可在文件管理器中定位它。

### 命令行选项

| 选项              | 说明                       |
| ----------------- | -------------------------- |
| `--update`、`-u`  | 更新所有索引。             |
| `--exec`、`-x`    | 仅显示可执行文件。         |
| `-c <file>`       | 使用指定的配置文件。       |
| `--help`、`-h`    | 显示用法。                 |
| *(无参数)*        | 启动图形界面。             |

## 打包

[`package.sh`](package.sh) 会为当前平台生成可分发的产物：

```bash
./package.sh              # 构建 + 打包
./package.sh --skip-build # 对已有的 build/ 直接打包
```

- **macOS** → `.dmg`（通过 `macdeployqt` 携带 Qt 框架的 `.app` 包）
- **Linux** → `.zip`（二进制 + `qlocate.toml.example`）
- **Windows** → `.zip`（二进制 + 通过 `windeployqt` 携带的 Qt DLL）

如果部署工具不在 `PATH` 中，可设置 `QT_DIR` 指向你的 Qt 安装目录。

## 发布

推送匹配 `v*` 的标签会触发 [GitHub Actions 发布工作流](.github/workflows/release.yml)，
它会构建并上传 macOS、Linux 和 Windows 的产物到 GitHub Release：

```bash
git tag v1.0.0
git push origin v1.0.0
```

## 项目结构

```
qlocate/
├── CMakeLists.txt          # 构建配置（Qt6 → Qt5 回退）
├── package.sh              # 跨平台打包脚本
├── src/
│   ├── main.cpp            # 入口，参数解析，CLI + GUI 分发
│   ├── config.{h,cpp}      # TOML 配置加载与查找
│   ├── indexer.{h,cpp}     # 构建 FILESNAP 二进制索引
│   ├── searcher.{h,cpp}    # 载入索引，通配符匹配
│   ├── mainwindow.{h,cpp}  # Qt 图形界面
│   └── qlocate.toml        # 示例配置
├── third_party/
│   └── toml.hpp            # 内置的 TOML 解析器（仅头文件）
└── .github/workflows/
    └── release.yml         # CI 构建 + 发布流水线
```

## 索引格式

`FILESNAP` v5 二进制布局：

```
头部: char magic[8] = "FILESNAP"; uint32 version = 0x00000500; int32 nEntry;
记录（重复 nEntry 次）:
  uint32 flags     # 位 0–29: 父记录索引（0x3FFFFFFF = 根/无）
                   # 位 30: 含有子目录
                   # 位 31: 是文件
  uint16 mode      # POSIX 权限位（st_mode & 07777）
  uint16 reserved
  uint32 namelen
  char   name[]    # 以 NUL 结尾，填充到 4 字节边界
```

根记录将一个绝对路径存为其 `name`；其余所有记录只存储单个路径组件并引用其父记录，
因此完整路径通过沿父链回溯来重建。

## 许可证

本项目采用 BSD 3-Clause 许可证，详见 [LICENSE](LICENSE)。
