# tfx for Linux Qt

Version: **0.5.4**

This directory contains the C++/Qt 6 Widgets implementation of `tfx-for-linux`.

## Build

```sh
./build.sh
./build/tfx
```

## Scope

- Folder tree and pinned folders
- Folder sidebar visibility toggle, collapse-all control, and startup visibility control
- Single-pane and split-pane file browser
- Startup path handling from command-line folder or current working directory
- Preview pane with source/rendered view switching, external image viewer, and multi-selection summary
- Preview keyboard shortcuts for source/rendered switching and opening externally
- Built-in command pane
- Restored window size, pane visibility, splitter sizes, tabs, and file-list columns
- `config.toml` settings including window/pane transparency (`[opacity]`)
- User-defined commands with `[[commands]]`
- Paste/drop conflict handling with overwrite, skip, and rename choices
- Clipboard-to-file paste for images, rich/plain text, URLs, CSV, and TSV
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

The original macOS SwiftUI source is not part of this Linux Qt implementation.
