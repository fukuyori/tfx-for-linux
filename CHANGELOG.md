# Changelog

This file records notable changes to `tfx-for-linux`.

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
- Added Japanese/English automatic UI text selection.
- Added README files for the Linux port.

### Notes

- Git status column rendering is present as a column but detailed status collection is not implemented yet.
- Owner, group, and symlink target are intentionally reserved for preview-pane metadata rather than file-list columns.
