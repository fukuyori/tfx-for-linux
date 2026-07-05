# Changelog

This file records notable changes to `tfx-for-linux`.

## [0.7.4] - 2026-07-06

### Fixed

- Long file names no longer get cut far short of the column edge: word wrap
  in the details/search/ZIP views broke names at hyphens and showed only the
  first fragment plus an ellipsis ("tfx-0.7.2-…"). Names now elide at the
  column boundary, and the file-list font is applied as a widget font so the
  elision metrics match the painted glyphs.
- Startup no longer prints "Using a variable-width font in the terminal":
  QTermWidget's constructor probes its own "Monospace" default before the
  configured font is applied, and on CJK systems that resolves to a
  dual-width font. The spurious constructor warning is filtered; a genuinely
  variable-width configured font still warns.

## [0.7.3] - 2026-07-06

### Added

- Clickable breadcrumb path in the pane header: each segment navigates to
  that ancestor, long paths collapse leading segments behind an ellipsis, and
  clicking the empty area switches to the editable path field (Esc, focus
  loss, or commit returns to the breadcrumb). ZIP browsing shows a static
  path.
- Rubber-band selection: press on the empty area of the file list and drag
  across rows to range-select them; Ctrl/Shift adds the band to the existing
  selection. (QTableView's own band selection gives up when a corner of the
  rectangle falls outside the rows.)
- Conflict dialog "Apply to all" checkbox: the chosen action (overwrite /
  skip / rename) is reused for the remaining conflicts of the same paste or
  drop batch.
- Copies now preserve modification and access times for files, directories,
  and symbolic links (`utimensat`; directory times are set after their
  contents so child writes do not bump them).
- The terminal cwd sync button resolves the active tmux pane's directory
  when tmux is running inside the terminal (client found via /proc, then
  `list-clients` → session → `display-message`).
- New Folder avoids name collisions like New File does: an existing name
  gets a " 2", " 3", ... suffix (without extension splitting) instead of
  failing, and the new folder is selected.

### Fixed

- Multi-selection no longer breaks after model refreshes: selection ranges
  covering the proxy's synthesised columns did not survive layout changes
  (sort, Git-status refresh, directory reload), leaving rows partially
  selected so move/copy/trash/drag acted on only the current row. Selections
  are now normalised back to full rows on model changes, operations derive
  rows from any selected cell, and the row highlight spans the full row
  again.
- Missing `ItemIsDragEnabled` flags made the base view treat a press on a
  selected row as the start of a rubber band, collapsing the multi-selection
  on the first pixel of movement; dragging several items at once now works.
- Ctrl+click multi-selection is covered by widget-level and FilePane-level
  regression tests (`FileViewsSelectionTest`, `FilePaneSelectionTest`),
  including survival across deferred click handling and model refreshes.

## [0.7.2] - 2026-07-05

### Added

- Config parity with the tfx `config.toml` spec: `[colors]` gains drop-target,
  title-bar, status-line, folder-tree, split-handle-idle, and Git-status-badge
  tokens; `[opacity]` gains `dropIndicator` (plus accepted-but-inert macOS
  tokens); new `[preview]` / `[preview.extensions]` / `[preview.markdown]`
  sections control preview mode per extension and Markdown external images.
- Previously unbound documented shortcuts are now wired: `openItem`,
  `openTerminal`, `swapPanes`, `compressToZip`, `extractZip`, `movePasteItems`,
  `selectAll`, `revealInFinder`, `focusTerminalPane`. Shortcut names align with
  the spec (`toggleTerminalPane`, `previousTab`, `copyPath`), keeping the old
  names as aliases.

### Changed

- Toolbar icons follow the configured foreground colour instead of a fixed grey.
- `docs/configuration.md` updated to document the full supported schema,
  including a shortcuts reference and the new colour/opacity/preview keys.

## [0.7.1] - 2026-07-05

### Added

- User-defined commands now follow the fuller `[[commands]]` schema: `run`,
  `extensions`, `target` (file/folder/current/any), `selection`
  (single/multiple/any), `requireGit`, `terminal`, and `shell`. Commands appear
  in the context menu only when the current selection matches their filters, and
  multi-line `run` bodies execute as temporary shell scripts.
- Commands with `terminal = true` stream their output live to the terminal
  pane's Output tab; `terminal = false` records output quietly in the Command
  Output dock (revealed on failure).
- Toolbar toggle button for the Command Output dock.

### Changed

- Context menu regrouped into a standard order with clearer separators;
  user-defined commands are listed flat, directly under Open.
- The Command Output dock's history and output areas are now resizable via a
  draggable splitter, and its detail view omits the redundant command / working
  directory / exit lines.

## [0.7.0] - 2026-07-05

### Added

- Top toolbar navigation buttons: Back, Forward, and Parent Folder, placed to
  the left of the search box.
- Top toolbar toggle buttons on the right: hidden-file visibility and folder
  sidebar visibility, alongside the existing split, preview, icon, and terminal
  toggles. Toolbar toggles stay in sync with their View-menu counterparts.
- Terminal pane signal buttons (`^C`, `^\`, `^Z`) that send the corresponding
  control characters to the interactive shell; the directory-sync (`cwd`)
  button moved next to them.
- Terminal pane is now tabbed with a "Terminal" tab (interactive shell) and an
  "Output" tab for user-command output.

## [0.6.7] - 2026-07-04

### Fixed

- Opening an executable file now actually runs it. Previously "Open" always
  handed the file to the desktop's URL handler, which never executes
  programs. Binary executables (ELF and other non-text files with the execute
  bit) launch directly in their own directory; executable text scripts prompt
  whether to run or open for editing.

## [0.6.6] - 2026-07-04

Performance release (from tfx macOS 0.9.3 / 0.9.4).

### Added

- Case-only renames (`foo` → `Foo`) in the file list now work on
  case-insensitive filesystems (exFAT/NTFS mounts) by hopping through a
  hidden temporary name; a direct rename there is refused or silently does
  nothing. No behavior change on normal case-sensitive filesystems.

### Changed

- Search results resolve file icons through a per-extension cache instead of
  one icon-provider lookup per row (~9× faster in a 5,000-file
  microbenchmark; more under real icon themes), and the ZIP browser reuses
  two generic icons per fill instead of one provider call per entry.

### Audited (no change needed)

- Search-result insertion already streams rows incrementally, matches names
  before constructing row items, and does not re-sort on append.
- Folder-tree seeding from pane listings was evaluated and skipped: the tree
  is an independent lazily-populated model that only enumerates expanded
  nodes.
- Startup into a 100,000-entry directory populates without app-side per-row
  work on the load path.

## [0.6.5] - 2026-07-04

Security-hardening release, round 2: untrusted file content and filenames
reaching parsers and external tools.

### Added

- CSV/TSV previews now use an RFC 4180 parser: quoted fields, escaped quotes,
  and embedded delimiters/newlines render correctly, capped at 1,000 rows ×
  100 columns with a visible notice when the limit is hit.
- Text previews load through a shared size-capped loader (4 MB) with a
  truncation indicator; the cut never splits a multi-byte UTF-8 character.
  Previously a flat 256 KB was read silently.
- Full-archive extraction is now refused for archives containing symbolic
  links (a link entry followed by files under its name can write outside the
  destination — unzip extracts links by default), and is capped at 100,000
  entries / 4 GiB uncompressed.
- Extraction runs into a hidden work directory that is renamed into place on
  success, so a failed extraction leaves nothing under the final name.
- Symbolic-link entries can no longer be opened from the archive browser.
- Qt Test coverage: RFC 4180 parsing, size-capped loading with UTF-8
  boundaries, zip inspection (symlink and size detection), option-like entry
  names, and malformed `config.toml` inputs (invalid UTF-8, unterminated
  strings, oversized values, control characters).

### Changed

- `zip` compression passes `--` before user-controlled file names so a name
  beginning with `-` cannot be parsed as an option.
- ZIP entry names beginning with `-` are now rejected as unsafe: `unzip` has
  no reliable end-of-options separator, and such names can inject options
  (including `-d<dir>`, which redirects extraction).
- Files created from clipboard content are created owner-read/write only.

### Documentation

- Documented the user-command quoting guarantee: tokens expand inside POSIX
  single quotes, so quotes, `$`, backticks, and newlines in file names are
  passed literally and cannot inject shell commands.
- Audit results: git invocations use fixed arguments with canonical absolute
  paths, and open/trash helpers receive absolute paths, so no `--` separators
  were needed there.

## [0.6.4] - 2026-07-04

Stability and process-hygiene release (from tfx macOS 0.9.3).

### Added

- Every git invocation now has a watchdog: terminate after 30 seconds,
  kill 5 seconds later, with stderr discarded, so a hung `git` (slow network
  filesystem, stuck hook) can no longer accumulate processes.
- Git status refreshes for the directory already shown are throttled to at
  most one per second per pane; navigating to a different directory still
  refreshes immediately.
- Subfolder search is bounded to a descent depth of 128 so bind-mount loops
  or pathological trees cannot make a search run forever; the completion
  message notes when the limit was hit.
- The built-in terminal's shell is now reaped on shutdown with SIGTERM
  followed by SIGKILL escalation, so a shell that ignores SIGHUP cannot
  outlive the application.

### Changed

- `git status` is now scoped to the current directory with a pathspec instead
  of scanning the whole repository.
- User-defined command output is read incrementally with a 1 MiB cap per
  stream (with a truncation notice) instead of buffering unbounded output in
  memory.
- Archive compression/extraction no longer blocks the UI event loop
  indefinitely; the window keeps repainting while the tool runs (user input
  stays disabled to avoid re-entrancy).

### Fixed

- Git status badges now appear correctly when the pane shows a subdirectory
  of a repository: porcelain paths are repository-root relative and are now
  rebased via `git rev-parse --show-prefix` before being resolved. Previously
  badges in subdirectories were wrong or missing.
- The branch lookup process is now tracked and cancelled on refresh like the
  status process, so rapid navigation cannot pile up git processes.

## [0.6.3] - 2026-07-04

File-safety release for the copy/move/paste pipeline (from tfx macOS 0.9.2).

### Added

- Symbolic links are now copied as links, preserving their raw relative or
  absolute link text, instead of materializing the link target. Directories
  reached through a link are no longer recursed into, and moving a link
  removes only the link itself, never the target's contents.
- Overwrite ("Replace") during paste/drop is now atomic: the copy is written
  to a hidden temporary name in the destination directory and swapped in with
  `rename(2)` only after it fully succeeds, so a mid-copy failure keeps the
  existing file intact. Replacing a directory moves the old tree aside first
  and rolls it back if the swap fails. Same-filesystem moves over a file
  replace it in a single atomic rename.
- Qt Test coverage for link-preserving copies (file, directory, and broken
  links), atomic replacement of files and directories, original preservation
  when a replace fails, hidden-file copies, symlinked nesting detection, and
  write-error propagation.

### Changed

- The self/descendant transfer guard now canonicalizes both sides through the
  shared `transferWouldNestInsideSource` helper, so nesting a folder into
  itself through a symlinked path is refused.
- Directory copies now include hidden and system entries; dotfiles were
  silently skipped before.
- Name-conflict detection now also triggers when the destination is a broken
  symbolic link.

### Fixed

- Write, flush, and close errors during a copy now fail the operation instead
  of being reported as success.
- Overwriting no longer deletes the destination up front on the UI thread;
  the replacement happens on the worker after the new copy is complete.

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
