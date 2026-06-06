# tfx for Linux Qt

Version: **0.2.1**

This directory contains the C++/Qt 6 Widgets implementation of `tfx-for-linux`.

## Build

```sh
./build.sh
./build/tfx
```

## Scope

- Folder tree and pinned folders
- Single-pane and split-pane file browser
- Startup path handling from command-line folder or current working directory
- Preview pane with source/rendered view switching
- Built-in command pane
- Restored window size, pane visibility, splitter sizes, tabs, and file-list columns
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
