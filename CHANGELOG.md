# Changelog

This file records notable changes to `tfx-for-linux`.

## [0.6.2] - 2026-07-04

Maintenance release for MainWindow refactoring.

### Changed

- Split `MainWindow` responsibilities into focused translation units for file
  operations, menus/toolbars, settings/search history, sidebar/pinned folders,
  theme application, and dock/visibility handling. `MainWindow.cpp` now keeps
  the main construction flow plus active-pane focus handling.

## [0.6.1] - 2026-07-04

Maintenance release for FilePane refactoring and documentation cleanup.

### Changed

- Split the remaining `FilePane` implementation into focused translation units
  for UI setup, signal wiring, navigation, state/selection, search, tabs,
  actions, file operations, clipboard/drop, archives, columns, and user
  commands. `FilePane.cpp` now only orchestrates construction and initial
  navigation; behavior is unchanged.
- Organized `FilePane.h` private declarations and member fields by subsystem so
  the split implementation files are easier to navigate.

## [0.6.0] - 2026-07-04

Release cleanup for packaging, file-operation robustness, preview/security
hardening, search and tab polish, and expanded Qt Test coverage.

### Added

- Packaging installs the executable, desktop entry, SVG icon, documentation,
  and provides an uninstall target; CI now runs an install smoke test.
- Copy/move paste and drop operations now run on a background worker with a
  status-bar progress indicator, cancellation, queued follow-up operations, and
  partial destination cleanup on cancel.
- File-operation progress now reports the total item count as soon as the
  worker has prepared the batch, before the first transfer completes.
- File-operation progress now keeps a visible status-bar summary with queued
  item counts, and cancel clears pending queued operations.
- Queued file operations now continue only after a successful batch; failures
  stop and clear the pending queue so follow-up operations do not run blindly.
- Added Qt Test coverage for the file-operation worker, including recursive
  copy, rename and copy-fallback moves, cancellation cleanup, and CTest in CI.
- Hardened ZIP browsing/extraction by rejecting unsafe entry paths before
  preview extraction or full archive extraction, with regression coverage.
- Hardened rendered previews: HTML files render as escaped source, Markdown
  remote images render as links instead of auto-loaded images, preview links
  only open `http`/`https`, and local embedded images must stay under the
  Markdown file's directory.
- Hardened process launch paths by canonicalizing terminal working directories,
  using `cd --` for explicit terminal navigation, and rejecting unavailable
  working directories for user-defined commands.
- Hardened Git status refresh by canonicalizing query directories and ignoring
  porcelain paths that are absolute or escape the queried directory.
- Added top-level `theme = "dark" | "light"` config support; `[colors]`
  continues to override the selected built-in palette.
- Added a disk cache for PDF preview images and multi-stage rendering fallback
  so previews can reuse successful renders and retry at smaller sizes.
- Added AppConfig test coverage for light theme presets, color overrides,
  invalid theme warnings, and `[openWith]` mappings.
- Moved canonical directory resolution into shared file-operation helpers and
  added path-handling tests for canonical directories and unique-name suffixes.
- Expanded file-operation helper tests to cover recursive directory copies and
  destination-exists failures.
- Added column layout normalization and test coverage for saved column order,
  widths, and sort state.
- Added Git service tests for porcelain rename parsing and status path
  containment.
- Wired `[openWith]` extension mappings into the file context menu; configured
  programs now appear in Open With and launch directly with the selected file.
- Search results now handle keyboard navigation more consistently: Enter opens
  the current result, Escape returns to the file list, and Backspace goes up.
- Search-result selections now update the preview pane, including multi-result
  preview summaries.
- The pane status line now reflects search-result counts, selection counts, and
  whether a search is still running.
- Search results now have a context menu for opening a result, jumping to its
  containing folder, revealing it in the file manager, and copying paths.
- Search-result columns are now sortable, with size and modified-time columns
  sorted by their underlying values instead of display text.
- Added a toolbar button to close search results and return focus to the active
  file list.
- Search terms are now kept in a persistent toolbar history, with newest terms
  first and duplicates collapsed.
- Added search-history helper coverage for trimming, duplicate collapse,
  existing-history cleanup, and history limits.
- Added a terminal header button that syncs the active file pane to the
  terminal's current working directory.
- File-pane tabs now show close buttons when multiple tabs are open, keep the
  final tab protected, and expose each tab's full path as a tooltip.
- Tab additions, closes, selection changes, and drag reordering now save the
  restored tab state immediately.
- Restored file-pane tab paths are canonicalized, so duplicate saved entries
  that point to the same directory collapse cleanly.
- File-pane tabs now avoid opening duplicate directories, and the tab bar
  context menu supports new tab, close tab, close other tabs, and copy tab path.
- Added tab-state helper coverage for restored tab normalization, duplicate
  restored tabs, and active-tab index clamping.
- Expanded path-handling coverage for trailing separators and directory aliases.

## [0.5.5] - 2026-07-04

View polish, naming control, and documentation closure.

### Added

- Markdown preview uses GitHub-style rendering, embeds local images as `data:`
  URLs, and only shows the external-image button when remote images are present.
- Drag-and-drop now highlights the target row/item or empty pane area and
  refreshes affected panes immediately after successful transfers.
- Startup window geometry can be set with `-g` / `--geometry` or
  `[startup] geometry`.
- `[naming] placeholderLanguage` controls generated placeholder names.
- Column-header sorting now shows and saves the active sort column/direction.
- English command-configuration documentation is complete.

## [0.5.4] - 2026-07-04

View and preview workflow polish.

### Added

- Folder sidebar visibility toggle and `[startup]` visibility controls for the
  terminal and folder sidebar.
- Folder-tree collapse-all control in the sidebar and tree context menu.
- Preview-pane shortcuts for source/rendered switching and opening the current
  preview externally.

## [0.5.3] - 2026-07-04

File-operation and clipboard paste improvements.

### Added

- Paste/drop name-conflict handling with overwrite, skip, and rename choices.
- Clipboard-to-file paste for images, RTF, HTML, URLs, CSV, TSV, and plain text,
  including a Paste as Plain Text action.

## [0.5.2] - 2026-07-04

User-defined command support.

### Added

- `[[commands]]` entries in `config.toml` add custom commands to the menu bar
  and file-list context menu.
- Command token expansion for selected paths, current directory, file name
  parts, and the user scripts directory.
- User-defined command shortcuts with conflict warnings.
- Command Output dock with run history, stdout/stderr, working directory, and
  exit status.

## [0.5.1] - 2026-06-14

Terminal and dock-layout fixes.

### Fixed

- Terminal font now resolves to a verified fixed-pitch family, reducing the
  QTermWidget "variable-width font" warnings.
- Docked panes can be resized again: the `QMainWindow::separator` is given a
  grabbable width (the stylesheet had collapsed it to zero).
- The terminal restarts correctly after the shell exits with `exit`: the
  finished widget is recreated and the configured font/colour scheme reapplied.
- The terminal pane now spans the full width beneath both file panes with a
  draggable separator, instead of a non-resizable bottom dock area.
- Toggling the split view no longer changes the folder-tree (sidebar) width.

### Added

- "Reset Layout" in the View menu restores the default dock arrangement
  (re-docking any floating panes) while keeping each pane's visibility.

### Changed

- The left/right file panes no longer show "Left"/"Right" dock titles.

## [0.5.0] - 2026-06-13

Phase 3 features: ZIP browsing, interactive terminal, and font control.

### Added

- Browse ZIP archives as folders: opening a `.zip` lists its contents with
  in-archive navigation; opening an entry extracts and opens it, and `..` at the
  root returns to the containing folder.
- Interactive terminal pane backed by QTermWidget (true colour, colour scheme,
  cursor, `Ctrl+C`, TUI apps); the working directory follows the active pane.
  Falls back to the simple command pane when QTermWidget is unavailable.
- Toolbar toggle button (with icon) for showing/hiding the terminal pane.
- Per-pane font overrides in `[font]`: `fileList`, `preview`, `terminal`,
  `folderTree` (each with an optional `*Size`), overriding the global mono font.
- `[terminal] colorScheme` to choose the terminal's QTermWidget colour scheme
  (background/foreground).

## [0.4.2] - 2026-06-13

Internal refactor (no behavior change); concludes the layered restructuring.

### Changed

- Extracted the asynchronous Git work (branch + status) out of `FilePane` into a
  reusable `GitStatusController` (under `controllers/`) that reports results via
  signals and drops stale results after navigation.

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
