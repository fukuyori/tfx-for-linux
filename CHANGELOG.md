# Changelog

This file records notable changes to `tfx-for-linux`.

## [0.2.0] - 2026-05-31

UI and usability development release.

### Added

- Added startup path behavior for the left pane: use the command-line folder, or the current working directory when no folder is specified.
- Added direct focus restoration to the active file list after startup, so arrow keys move file-list selection immediately.
- Added mouse and keyboard file-list selection improvements with persistent row highlight.
- Added richer preview handling for rendered/source views and common file formats.
- Added Tab / Shift+Tab movement between file panes.
- Added editable path fields in file panes.
- Added minimum-size and splitter calculations for stable split/preview pane layout.

### Changed

- Refined the Linux Qt visual design toward the current tfx reference layout.
- Changed split-pane behavior so the current file-list area is divided instead of growing the application window.
- Removed folder icons from the folder tree and tightened tree indentation.
- Improved startup restoration so the left pane no longer gets overwritten by saved tab state.
- Improved file-list navigation when entering child folders and returning to parent folders.

### Notes

- The app binary remains `tfx`.
- Build output remains under `./build`.
- macOS project files remain excluded from this Linux repository.

## [0.1.0] - 2026-05-29

Initial Linux Qt development release.

### Added

- Added the C++/Qt 6 Widgets implementation under `linux-qt/`.
- Added a terminal-inspired file manager layout with folder tree, pinned folders, file panes, preview pane, and command pane.
- Added single-pane and split-pane file list modes.
- Added preview visibility toggle and split visibility toggle with restored window and splitter state.
- Added file-list column configuration with visibility, width, and drag-and-drop order persistence.
- Added file-list columns for name, type, size, created time, modified time, file mode, and Git status.
- Added `drwxrwxr-x` style file mode display.
- Added Linux `~/.config/tfx/config.toml` creation and parsing, following the tfx for Windows configuration model.
- Added configurable font, colors, startup split/preview behavior, right-pane startup folder, and shortcuts.
- Added Japanese/English automatic UI text selection.
- Added README files for the Linux port.

### Notes

- Git status collection is implemented for the visible file list using `git status --porcelain=v1`.
- Owner, group, and symlink target are intentionally reserved for preview-pane metadata rather than file-list columns.
