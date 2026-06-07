# tfx for Linux

**Terminal-inspired interface File eXplorer for Linux**  
Version: **0.3.2**

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
./build.sh
./build/tfx
```

For a clean build:

```sh
./build.sh --clean
```

To build and run in one step:

```sh
./build.sh --run
```

To open a specific folder:

```sh
./build/tfx /path/to/folder
```

When no folder is specified, the application opens the current working directory.

## Current Scope

- Folder tree and pinned folders
- Single-pane and split-pane file list
- Startup path handling: the left pane opens the command-line folder, or the current working directory when no folder is specified
- Preview pane with source/rendered switching, an external image viewer button, and a multi-selection summary
- Built-in command pane
- User-editable `config.toml` created under `~/.config/tfx/`, including window/pane transparency
- Window, splitter, pane visibility, tab, and column setting restoration
- File operations: open, open with (native chooser), rename, link, new file/folder, trash, copy/cut/paste, copy path
- Drag-and-drop of files and folders between panes and to/from external file managers (drop onto a folder to move, or hold Ctrl to copy)
- Recursive subfolder search: type a term and press Enter; results stream into a dedicated view and the search is cancelled when the pane changes folder
- Mouse and keyboard selection in file lists, including persistent row highlight
- Per-pane status line showing item counts, selection, and the current Git branch
- Auto-refresh of the file list and Git status when the directory changes
- Application version shown in the status bar
- Configurable file-list columns:
  - Name
  - Type
  - Size
  - Date Created
  - Date Modified
  - File Mode, such as `drwxrwxr-x`
  - Git Status

Column visibility and order can be changed from the file-list header menu. Column order is changed by dragging item names in the settings dialog.

## Configuration

The Linux port follows the same split as tfx for Windows:

- `~/.config/tfx/config.toml` is the user-editable configuration file.
- Qt session settings remain app-owned state for window placement, last paths, pane visibility, splitters, pinned folders, tabs, and column layout.

On startup, tfx creates `config.toml` when it does not already exist. Existing files are not overwritten.

Currently supported sections are:

- Top-level `version = 1`
- `[font]`
- `[colors]`
- `[opacity]` (`background`, `inactivePane`, `disabledItem`)
- `[startup]`
- `[shortcuts]`
- `[terminal]` and `[openWith]` are parsed for compatibility; detailed behavior is still being wired in.

See [docs/configuration.md](docs/configuration.md) for the Linux-specific notes.

## Roadmap

See [docs/roadmap.md](docs/roadmap.md).

## Repository

```text
https://github.com/fukuyori/tfx-for-linux.git
```

## License

License information is not yet finalized for this Linux port.
