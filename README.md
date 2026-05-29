# tfx for Linux

**Terminal-inspired interface File eXplorer for Linux**  
Version: **0.1.0**

English | [Japanese](README.ja.md)

`tfx-for-linux` is a C++/Qt port of `tfx`, focused on a terminal-like file manager experience for Linux desktops.

This repository contains only the Linux Qt implementation. The original macOS SwiftUI project files are intentionally excluded.

## Requirements

- Linux
- CMake 3.20 or later
- C++17 compiler
- Qt 6 Widgets

## Build

```sh
cmake -S linux-qt -B /tmp/tfx-qt-build
cmake --build /tmp/tfx-qt-build
/tmp/tfx-qt-build/tfx-qt
```

To open a specific folder:

```sh
/tmp/tfx-qt-build/tfx-qt /path/to/folder
```

When no folder is specified, the application opens the current working directory.

## Current Scope

- Folder tree and pinned folders
- Single-pane and split-pane file list
- Preview pane with source/rendered switching
- Built-in command pane
- Window, splitter, pane visibility, tab, and column setting restoration
- File operations: open, rename, new file/folder, trash, copy/cut/paste, copy path
- Configurable file-list columns:
  - Name
  - Type
  - Size
  - Date Created
  - Date Modified
  - File Mode, such as `drwxrwxr-x`
  - Git Status

Column visibility and order can be changed from the file-list header menu. Column order is changed by dragging item names in the settings dialog.

## Repository

```text
https://github.com/fukuyori/tfx-for-linux.git
```

## License

License information is not yet finalized for this Linux port.
