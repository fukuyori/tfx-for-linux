# Changelog

This file records notable changes to `tfx-for-linux`.

## [0.4.1] - 2026-06-13

Internal refactor (no behavior change).

### Changed

- Split the oversized `FilePane` translation unit into focused units: the column
  model moved to `models/` (`FileSystemProxyModel`, `FileColumns`) and the file
  list views moved to `views/` (`FileItemDelegate`, `FileTableView`,
  `FileIconView`, selection helpers). `FilePane.cpp` shrank from ~2500 to ~1750
  lines.

## [0.4.0] - 2026-06-13

Architecture restructuring and dockable panes.

### Added

- Panes are now dock widgets: the sidebar, both file panes, the preview, and the
  terminal can be rearranged, floated, or tabbed by dragging their title bars.

### Changed

- Toggling a pane's visibility now redistributes space within the current window
  instead of resizing the window — friendlier for multi-platform use.
- Window layout is persisted via `QMainWindow` state (dock positions, sizes, and
  floating), replacing the previous per-splitter persistence.
- Restructured the codebase into layers: `core/` (platform-agnostic logic:
  file-type/size/mode formatting, file operations, git parsing) and `platform/`
  (an OS abstraction with a Linux implementation for open/reveal/trash, the
  native "open with" chooser, the terminal shell, and PDF rendering). The build
  selects the platform implementation per OS, preparing for macOS/Windows ports.

## [0.3.3] - 2026-06-07

Icon view and multi-selection (start of Phase 3).

### Added

- Added an icon view mode for the file list, toggled per pane from the toolbar or the View menu and remembered separately for the left and right panes. Icon view shares the model and selection with the details view and supports the same drag-and-drop.
- Added range selection (click then Shift+click) and toggle selection (Ctrl+click) in the details view.

### Changed

- The icon-view toggle reflects and applies to the active pane only, so split view can show one pane as details and the other as icons.

## [0.3.2] - 2026-06-07

Drag-and-drop and search refinements.

### Added

- Added a drag image that follows the cursor while dragging file-list items (stacked rows with a `+N` badge for multiple items).
- Search results now appear in the same table format as the file list, with file icons and the Name/Type/Size/Date Modified/File Mode columns.
- Pinned folders can be reordered by dragging, with an insertion indicator showing the drop position; folders dragged from the file list are inserted at that position.

### Changed

- The folder tree no longer participates in drag-and-drop (its folders cannot be dragged out and nothing can be dropped onto it).

## [0.3.1] - 2026-06-07

Phase 2 core file interactions.

### Added

- Added drag-and-drop for files and folders: drag out to other panes and external file managers, drop in from them, and drop onto a folder to move (or copy with Ctrl) into it.
- Added recursive subfolder search: type a term and press Enter to search the current folder and all subfolders, with results streamed into a dedicated results view showing relative paths and a live match count.

### Changed

- Search no longer filters the current directory as you type; it now starts an explicit recursive search on Enter and is cancelled automatically when the pane navigates to another folder.

## [0.3.0] - 2026-06-07

Phase 1 usability features bringing the Linux port closer to tfx-for-windows.

### Added

- Added the application version at the right edge of the status bar.
- Added the current Git branch (`⎇ name`) in the status bar, following the active pane.
- Added "Create Link" (symlink) to the file context menu.
- Enabled "Open With → Other..." to show the desktop's native application chooser via xdg-desktop-portal (falling back to a launch-command prompt when the portal is unavailable).
- Added a multi-selection preview summary (item count, total size, and a per-item list) in the preview pane.
- Added auto-refresh of Git status when the directory changes (via `QFileSystemWatcher`) plus a periodic poll to catch commits and staging.

## [0.2.1] - 2026-06-06

File-list columns, preview, and window appearance refinements.

### Added

- Added a configurable `[opacity]` section (`background`, `inactivePane`, `disabledItem`) for window/pane transparency.
- Added an "open in external viewer" button for image previews, placed beside the preview source toggle.

### Changed

- Moved the preview source/rendered toggle button above the file metadata, and hid the metadata while the rendered view is shown.
- Showed English file-type names in the Type column (e.g. "Plain text") instead of the locale-dependent system description.
- Displayed file sizes with an English "Byte" unit and right-aligned the Size column.
- Slimmed the Git column (no header label, single-letter status such as "M", centered).

### Fixed

- Fixed the Modified, Mode, and Git columns showing no data by synthesising indices for columns beyond the underlying model.
- Fixed column widths reverting on restart/reload by sharing the layout from the left pane, re-applying it after model reorganisation, and suppressing saves of header reset values.

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
