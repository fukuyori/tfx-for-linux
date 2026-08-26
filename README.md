# tfx for Linux

**Terminal-inspired interface File eXplorer for Linux**  
Version: **0.8.6**

English | [Japanese](README.ja.md)

`tfx-for-linux` is a C++/Qt port of `tfx`: a keyboard-centric two-pane file manager for Linux desktops.

The default theme is translucent and rounded, in the spirit of
[prism-fm](https://github.com/fukuyori/prism-fm) — a dark surface the desktop
shows through, a UI sans-serif face, generous row spacing and neutral greys for
selection, and file-list icons drawn by tfx itself — a filled folder and an
outlined page, centred in their own slot and tinted from `[colors]` — so the
list looks the same whatever icon theme the desktop uses. Monospace is kept
where it carries meaning: the terminal pane and the preview's source view. Everything remains overridable through `[colors]`,
`[font]` and `[opacity]` in `config.toml`; set `[opacity] background = 1.0` for a
solid window.

This repository contains only the Linux Qt implementation. The original macOS SwiftUI project files are intentionally excluded.

## Requirements

- Linux
- CMake 3.20 or later
- C++17 compiler
- Qt 6 Widgets

## Build

```sh
scripts/build.sh
./build/tfx
```

For a clean build:

```sh
scripts/build.sh --clean
```

To build and run in one step:

```sh
scripts/build.sh --run
```

To open a specific folder:

```sh
./build/tfx /path/to/folder
```

To set the initial window geometry:

```sh
./build/tfx --geometry 1280x780+80+40
```

When no folder is specified, the application opens the current working directory.

`tfx` detaches from the terminal at startup, so the shell prompt returns immediately (no `&` needed). To keep it attached — for example to see log output — use:

```sh
./build/tfx --foreground
```

## Tests

```sh
scripts/build.sh --tests
ctest --test-dir build --output-on-failure
```

The default `scripts/build.sh` run skips the test executables; pass `--tests`
to build them.

The GitHub Actions build runs configure, build, tests, and an install smoke
test for the Linux Qt target.

## Current Scope

- Folder tree and pinned folders (reorder pins by dragging; drop folders to pin)
- Folder sidebar visibility toggle, collapse-all control, and startup visibility control
- Single-pane and split-pane file list, with per-pane details and icon view modes
- Dockable panes: the sidebar, both file panes, the preview, and the terminal can be rearranged, floated, or tabbed; toggling visibility splits the current window instead of resizing it
- Range selection (click then Shift+click), toggle selection (Ctrl+click), and rubber-band selection by dragging from the empty area
- Clickable breadcrumb path in the pane header (each segment navigates to that ancestor; clicking the empty area switches to the editable path field)
- Startup path handling: the left pane opens the command-line folder, or the current working directory when no folder is specified
- Startup visibility control for preview, terminal, and folder sidebar
- Startup window geometry via `--geometry` or `[startup] geometry`
- Preview pane with source/rendered switching, an external image viewer button, and a multi-selection summary
- Preview keyboard shortcuts for source/rendered switching and opening the current preview externally
- Markdown preview with GitHub-style tables and local image embedding
- Browse ZIP archives as folders (navigate in, open/extract entries)
- Interactive terminal pane (QTermWidget) whose working directory follows the active pane; falls back to a simple command pane when unavailable. The cwd sync button resolves the active tmux pane's directory when tmux is running inside the terminal
- User-editable `config.toml` created under `~/.config/tfx/`, including window/pane transparency, per-pane fonts, and the terminal colour scheme
- User-defined `[[commands]]` for the context menu/menu bar, token expansion, shortcut conflict warnings, and the Command Output dock
- Window, dock layout, pane visibility, tab, and column setting restoration
- Multi-tab file panes with close buttons, duplicate-tab suppression, tab
  context menu actions, and restored tab cleanup
- File operations: open, open with (native chooser), rename, link, new file/folder, trash, background copy/cut/paste with progress/cancel, copy path; symbolic links are copied as links with their link text preserved; copies keep permissions and modification times
- Drag-and-drop of files and folders between panes and to/from external file managers (drop onto a folder to move, or hold Ctrl to copy)
- Drag-and-drop feedback that names where the drop lands: only folder rows are
  highlighted, framing the folder's icon and name rather than the whole row
  (dropping anywhere else goes to the listed folder, shown as a frame around
  the pane), and a badge by the cursor reads "Move to <folder>" or
  "Copy to <folder>", following Ctrl as you hold it. Affected panes refresh
  immediately
- Conflict handling for paste/drop operations with overwrite, skip, and rename choices and an "Apply to all" checkbox for the rest of the batch; overwrite replaces the existing item atomically so a failed copy never destroys it
- Clipboard-to-file paste for images, rich/plain text, URLs, CSV, and TSV, plus Paste as Plain Text
- Recursive subfolder search: type a term and press Enter; results stream into a dedicated view and the search is cancelled when the pane changes folder
- Search-result keyboard navigation, context menu actions, sortable result
  columns, preview updates, close button, and persistent search history
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
Clicking a column title sorts the file list by that column and clicking it again
reverses the direction; the sorted column is marked with ▲/▼ in its title and
the sort column and direction are saved with the column layout.

The `..` parent entry stays on the first row whichever key and direction are
active, and shows only its name — its type, size and timestamps described the
folder above and were only noise.

`Ctrl+Shift+S` (View ▸ Sort Options..., or the header context menu) opens a
keyboard-driven sort chooser: Up/Down (or `k`/`j`) or a digit picks the key,
Space (or Left/Right) flips ascending/descending, Enter applies and Esc cancels.
Alongside the columns it offers **Natural**, a numeric-aware name order that
places `file2` before `file10`. The chooser also reaches columns that are
currently hidden, and works in icon view where there is no header to click.

## Configuration

The Linux port follows the same split as tfx for Windows:

- `~/.config/tfx/config.toml` is the user-editable configuration file.
- Qt session settings remain app-owned state for window placement, last paths, pane visibility, dock layout, pinned folders, tabs, and column layout.

On startup, tfx creates `config.toml` when it does not already exist. Existing files are not overwritten.

Currently supported sections are:

- Top-level `version = 1` and `theme = "dark" | "light"`
- `[font]`
- `[colors]`
- `[opacity]` (`background`, `inactivePane`, `disabledItem`)
- `[startup]`
- `[naming]`
- `[shortcuts]`
- `[terminal]`
- `[openWith]`
- `[[commands]]`

See [docs/configuration.md](docs/configuration.md) for the Linux-specific notes.

## Roadmap

See [docs/roadmap.md](docs/roadmap.md).

## Packaging

The CMake install target installs the executable, desktop entry, SVG icon, and
documentation. `scripts/build_package.sh` builds DEB/RPM/tar.gz packages into
`dist/`. See [docs/packaging.md](docs/packaging.md).

## Repository

```text
https://github.com/fukuyori/tfx-for-linux.git
```

## License

This project is licensed under the [Apache License 2.0](LICENSE), the same
license as the original macOS version of `tfx`.
