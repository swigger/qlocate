# qlocate

A fast, cross-platform file locator inspired by Unix `locate`. qlocate builds a
compact binary index of your filesystem and lets you search it instantly by
filename or path using wildcard patterns. It ships with both a **Qt GUI** and a
**command-line interface** in a single binary.

## Features

- **Instant search** over a pre-built index — no live filesystem walk per query.
- **Two interfaces in one binary**: launch the GUI, or pass arguments for CLI use.
- **Wildcard matching** with `*` and `?`, case-insensitive.
- **Filename or full-path matching** — automatically switches to path matching
  when the pattern contains a `/`.
- **Executable-only filter** (`-x`) to find runnable files.
- **Multiple indexes** ("databases"), each with its own include/exclude paths.
- **Directory ignore lists** (e.g. `node_modules`, `.git`, `__pycache__`) so
  noise is skipped at index time.
- **Compact custom binary format** (`FILESNAP` v5) using parent-position
  back-references instead of storing full paths per entry.
- **Cross-platform**: macOS, Linux, and Windows (Qt5 or Qt6).

## How it works

qlocate separates *indexing* from *searching*:

1. **Indexing** (`--update`) recursively scans the configured `include_paths`,
   skipping symlinks, excluded paths, and ignored directory names. Each entry is
   stored as a fixed-size record (flags, mode, name) where `flags` packs the
   parent record's position, a "has subdirectory" bit, and an "is file" bit.
   Full paths are reconstructed at search time by walking parent references, so
   the index stays small.
2. **Searching** loads the index into memory and runs a recursive wildcard
   matcher against either each filename or each reconstructed full path.

The index file is written to the `filename` configured per database (for example
`~/.local/share/qlocate/main.idx`).

## Building

### Requirements

- A C++20 compiler
- CMake ≥ 3.16
- Qt 6 (preferred) or Qt 5 — `Widgets` component

The build prefers Qt6 and automatically falls back to Qt5.

### Build steps

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

If Qt is installed in a non-standard location, point CMake at it:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt
```

The single-header [`toml.hpp`](third_party/toml.hpp) dependency is vendored under
`third_party/`, so no extra package is needed for config parsing.

## Configuration

qlocate reads a TOML config file. It is searched for in this order:

1. `qlocate.toml` next to the executable
2. `~/.config/qlocate/qlocate.toml`

You can also pass an explicit path with `-c <file>`.

Example (`src/qlocate.toml`):

```toml
[settings]
file_limit_interactive = 50      # max results shown while typing in the GUI
file_limit_search      = 2000    # max results for an explicit search

[[database]]
name           = "main"
filename       = "~/.local/share/qlocate/main.idx"
include_paths  = ["/home/user/projects", "/home/user/Documents"]
exclude_paths  = []
ignore_dirnames = ["node_modules", ".git", ".svn", "__pycache__", ".cache", "build", "dist"]
default_update = true            # included in a normal "Update Index" run
```

### Config reference

**`[settings]`**

| Key                      | Default | Description                                   |
| ------------------------ | ------- | --------------------------------------------- |
| `file_limit_interactive` | `50`    | Result cap for live (as-you-type) GUI search. |
| `file_limit_search`      | `2000`  | Result cap for an explicit search.            |

**`[[database]]`** (one or more; this is a TOML array of tables)

| Key               | Default     | Description                                                            |
| ----------------- | ----------- | ---------------------------------------------------------------------- |
| `name`            | `"default"` | Human-readable label for the index.                                    |
| `filename`        | `""`        | Path of the index file to write/read.                                  |
| `include_paths`   | `[]`        | Root directories to index.                                             |
| `exclude_paths`   | `[]`        | Absolute paths to skip (the entry is kept, its contents are not).      |
| `ignore_dirnames` | `[]`        | Directory **names** to skip anywhere (e.g. `node_modules`, `.git`).    |
| `default_update`  | `true`      | Whether this database is rebuilt on a normal update.                   |

## Usage

### Build / refresh the index

```bash
qlocate --update      # or -u
```

This rebuilds every configured database and prints progress.

### Command-line search

```bash
qlocate <pattern> [<pattern> ...]   # search by filename / path
qlocate -x <pattern>                # only executable files
qlocate -c myconfig.toml <pattern>  # use a specific config file
```

Examples:

```bash
qlocate '*.pdf'             # all PDFs
qlocate 'report?'           # report1, reportA, ...
qlocate '/projects/*/main*' # full-path match (pattern contains '/')
qlocate -x build            # executables named like "build"
```

If no index exists yet, qlocate reminds you to run `--update` first.

### GUI

Run with no arguments to open the GUI:

```bash
qlocate
```

- Type at least 2 characters for live search (a trailing `*` is added
  automatically unless the pattern contains `/`).
- Press **Enter** or click **Search** for a full search.
- Toggle **Exec only** to restrict results to executables.
- Click **Update Index** to rebuild databases with `default_update = true`.
  Hold **Shift** while clicking to force-rebuild *all* databases.
- **Double-click** a result to reveal it in your file manager.

### CLI options

| Option            | Description                          |
| ----------------- | ------------------------------------ |
| `--update`, `-u`  | Update all indexes.                  |
| `--exec`, `-x`    | Only show executable files.          |
| `-c <file>`       | Use the given config file.           |
| `--help`, `-h`    | Show usage.                          |
| *(no args)*       | Launch the GUI.                      |

## Packaging

[`package.sh`](package.sh) produces a distributable for the current platform:

```bash
./package.sh              # build + package
./package.sh --skip-build # package an existing build/
```

- **macOS** → `.dmg` (a `.app` bundle with Qt frameworks via `macdeployqt`)
- **Linux** → `.zip` (binary + `qlocate.toml.example`)
- **Windows** → `.zip` (binary + Qt DLLs via `windeployqt`)

Set `QT_DIR` to point at your Qt installation if the deploy tools aren't on
`PATH`.

## Releases

Pushing a tag matching `v*` triggers the [GitHub Actions release
workflow](.github/workflows/release.yml), which builds and uploads macOS, Linux,
and Windows artifacts to a GitHub Release:

```bash
git tag v1.0.0
git push origin v1.0.0
```

## Project layout

```
qlocate/
├── CMakeLists.txt          # Build config (Qt6 → Qt5 fallback)
├── package.sh              # Cross-platform packaging script
├── src/
│   ├── main.cpp            # Entry point, arg parsing, CLI + GUI dispatch
│   ├── config.{h,cpp}      # TOML config loading and discovery
│   ├── indexer.{h,cpp}     # Builds the FILESNAP binary index
│   ├── searcher.{h,cpp}    # Loads index, wildcard matching
│   ├── mainwindow.{h,cpp}  # Qt GUI
│   └── qlocate.toml        # Example configuration
├── third_party/
│   └── toml.hpp            # Vendored TOML parser (header-only)
└── .github/workflows/
    └── release.yml         # CI build + release pipeline
```

## Index format

`FILESNAP` v5 binary layout:

```
Header: char magic[8] = "FILESNAP"; uint32 version = 0x00000500; int32 nEntry;
Record (repeated nEntry times):
  uint32 flags     # bits 0–29: parent record index (0x3FFFFFFF = root/none)
                   # bit 30: has subdirectory
                   # bit 31: is file
  uint16 mode      # POSIX permission bits (st_mode & 07777)
  uint16 reserved
  uint32 namelen
  char   name[]    # NUL-terminated, padded to a 4-byte boundary
```

Root records store an absolute path as their `name`; all other records store a
single path component and reference their parent, so full paths are
reconstructed by walking parent links.

## License

Licensed under the BSD 3-Clause License. See [LICENSE](LICENSE) for details.
