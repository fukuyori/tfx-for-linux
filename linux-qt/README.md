# tfx for Linux Qt

Version: **0.6.2**

This directory contains the C++/Qt 6 Widgets implementation of `tfx-for-linux`.

## Build

```sh
./build.sh
./build/tfx
```

Install:

```sh
cmake --install build --prefix /usr/local
```

Test:

```sh
ctest --test-dir build --output-on-failure
```

## Scope

- Folder tree and pinned folders
- Folder sidebar visibility toggle, collapse-all control, and startup visibility control
- Single-pane and split-pane file browser
- Startup path handling from command-line folder or current working directory
- Startup window geometry via `--geometry` or `[startup] geometry`
- Preview pane with source/rendered view switching, external image viewer, and multi-selection summary
- Preview keyboard shortcuts for source/rendered switching and opening externally
- Markdown preview with GitHub-style tables and local image embedding
- Built-in command pane
- Restored window size, pane visibility, splitter sizes, tabs, and file-list columns
- Multi-tab panes with close buttons, duplicate-tab suppression, tab context
  menu actions, and restored tab cleanup
- `config.toml` settings including light/dark theme presets and window/pane transparency (`[opacity]`)
- User-defined commands with `[[commands]]`
- Paste/drop conflict handling with overwrite, skip, and rename choices
- Drag-and-drop target highlighting and immediate refresh of affected panes
- Clipboard-to-file paste for images, rich/plain text, URLs, CSV, and TSV
- Recursive search with sortable result columns, preview updates, context menu
  actions, close control, and persistent search history
- Per-pane status line with item counts, selection, and current Git branch
- Auto-refresh of the file list and Git status on directory changes
- Mouse and keyboard selection with persistent row highlight
- Configurable file-list columns:
  - Name
  - Type
  - Size
  - Date Created
  - Date Modified
  - File Mode
  - Git Status
- Clickable column-header sorting with saved sort direction

The original macOS SwiftUI source is not part of this Linux Qt implementation.
