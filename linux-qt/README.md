# tfx for Linux Qt

Version: **0.1.0**

This directory contains the C++/Qt 6 Widgets implementation of `tfx-for-linux`.

## Build

```sh
cmake -S linux-qt -B /tmp/tfx-qt-build
cmake --build /tmp/tfx-qt-build
/tmp/tfx-qt-build/tfx-qt
```

## Scope

- Folder tree and pinned folders
- Single-pane and split-pane file browser
- Preview pane with source/rendered view switching
- Built-in command pane
- Restored window size, pane visibility, splitter sizes, tabs, and file-list columns
- Configurable file-list columns:
  - Name
  - Type
  - Size
  - Date Created
  - Date Modified
  - File Mode
  - Git Status

The original macOS SwiftUI source is not part of this Linux Qt implementation.
