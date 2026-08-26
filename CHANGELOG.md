# Changelog

This file records notable changes to `tfx-for-linux`.

## [0.8.6] - 2026-08-26

### Fixed

- `[startup] rightFolder` / `rightFolders` had no effect. They were applied
  before the saved tabs were restored, and restoring a tab navigates the pane,
  so the previous session's folder always overwrote the configured one. They
  are now applied after the tabs, matching how the command-line folder is
  applied to the left pane. When neither key resolves to an existing directory
  the right pane still keeps its restored folder.
- An unrecognised key under `[startup]` — a misspelling such as `rightfolders`
  — was ignored silently. It now produces the same `config.toml:<line>` warning
  as the other sections.

### Documentation

- `docs/configuration.md` documented `rightFolder` / `rightFolders` in a single
  line. The `[startup]` section now covers both keys with examples, states that
  `rightFolders` is checked first and `rightFolder` is the fallback, that the
  configured folder overrides the restored session, and that there is no
  `leftFolder` key.

## [0.8.5] - 2026-08-25

### Fixed

- The file list measured text elision with the monospace family while the
  stylesheet painted it with the UI family, so names could be elided at the
  wrong width. Both now use the same face.
- Dock title rows (the "Folder", "Preview" and "Terminal" headers) painted
  nothing at all: they have no style of their own, so in a translucent window
  they were fully see-through holes rather than window chrome. They now use the
  window-chrome colour and render at `[opacity] background` like the menu bar.
  Measured over the whole window, fully transparent pixels drop from 23,175 to
  88 — and those 88 are the rounded corners of the file pane, which are meant
  to show through.
- Frameless windows (`[window] titleBar = "integrated"`) gave no sign of where
  the resize zone was: the band is now 8px instead of 6px and the pointer turns
  into a resize cursor over it, so the window edges — the bottom one included —
  can be grabbed without hunting for them.

### Changed

- File-list icons are drawn by tfx instead of taken from the desktop icon
  theme: a filled folder and an outlined page in prism-fm's shapes, tinted with
  `[colors] directoryForeground` / `fileForeground`. The list now looks the same
  on every icon theme, follows the palette, and the same glyphs are used in the
  search results and the ZIP browser. The icon sits centred in a fixed 32px slot
  at the start of the name cell, the way prism-fm lays its icon column out.
- The default theme was reworked to follow prism-fm: translucent surfaces
  (`[opacity] background` now defaults to `0.65`), a UI sans-serif face instead
  of monospace for the menus, file lists and folder tree, 8px rounded corners on
  panes, menus, tabs and input fields, thin rounded scrollbars,
  hairline separators in place of full borders, small muted section headers, and
  a neutral grey selection. Monospace is kept for the terminal pane and the
  preview's source view. Existing `[colors]`, `[font]` and `[opacity]` overrides
  keep working, so a `config.toml` that pins colours will still show them.
- Nine colours were hardcoded in the theme and ignored `[colors]` entirely:
  the selected pane tab, the terminal action buttons (background, border, hover
  and pressed states), the splitter handle and window separator, the separator
  hover, and the breadcrumb separator glyph. They now resolve through the
  configured tokens like everything else. The only literal left is the active
  pane badge's text colour, a contrast constant for a widget that is not shown.
- The file-list selection and hover colours now come from `[colors]`
  (`fileListRowSelected`, `fileListRowHovered`, `fileListRowSelectedForeground`).
  The row delegate painted hardcoded values, so those three settings had no
  effect on the rows themselves.
- `PreviewPane` now paints its own background. It has always had a stylesheet
  rule, but Qt ignores backgrounds on a plain QWidget subclass unless
  `WA_StyledBackground` is set, so its children painted the surface individually
  and showed up as lighter blocks in a translucent window.

## [0.8.4] - 2026-08-25

### Added

- A badge next to the cursor during a drag naming the folder the drop lands in
  and whether it moves or copies, updating as Ctrl is pressed or released.
- Sort Options chooser, opened with `Ctrl+Shift+S` (also View ▸ Sort Options...
  and the file-list header context menu): a keyboard-driven popup listing the
  sort keys with a cursor on the current one. Up/Down (or `k`/`j`) and the
  digit keys move, Space (or Left/Right) flips ascending/descending, Enter
  applies and Esc cancels. It reaches hidden columns and works in icon view,
  where there is no header to click. The popup sizes itself to its longest
  label, measured after the theme font is applied, so it neither clips the
  text nor spreads wider than the entries need.
- `Natural` sort key: numeric-aware name ordering, so `file2` sorts before
  `file10` instead of after it. The choice is saved with the column layout.
- New `sortOptions` entry in `[shortcuts]`.

### Fixed

- Drag-and-drop pointed at the wrong destination. The row under the cursor was
  always framed, but a drop only enters that row when it is a folder —
  over a file it goes to the folder being listed. Hovering a file therefore
  drew a box around the file as if it would receive the drop. Only rows that
  actually take the drop are highlighted now; anything else shows the
  pane-wide frame that stands for the listed folder. The highlight and the
  drop are driven by the same targeting functions, so they cannot disagree.
  In the details view that highlight now frames the folder's icon and name
  instead of spanning every column, which read as "this row is selected"
  rather than "this folder receives the drop".
- With `[opacity] background` below 1, the file list was very nearly opaque
  while the rest of the window was see-through: the theme's generic `QWidget`
  background rule has every widget paint the same translucent fill, and the
  file list sits four widgets deep (pane, view stack, view, scroll viewport),
  so the alpha compounded — 0.6 became 0.97. Those inner containers are pure
  containers with no colour of their own, so they no longer repaint the
  surface the pane already paints. Measured over the whole window, the area
  behind the file names now renders at exactly the configured opacity, and no
  region became fully transparent that was not already.
- The preview pane, the toolbar and the folder sidebar were affected by the
  same stacking: the preview's view stack and scroll viewport repainted the
  surface the preview view already paints, the toolbar's stretch filler
  repainted the toolbar's, and the folder tree, pinned list and disk list each
  repainted the sidebar's. All three now render at the configured opacity
  (measured: preview 0.97 → 0.60, toolbar 0.84 → 0.60, sidebar views
  0.84 → 0.60).
- The tab stack around the terminal no longer repaints the pane's surface
  (same stacking problem as the file list). The terminal's own background is
  now told to follow `[opacity] background` as well, but note that this has no
  visible effect yet: QTermWidget honours the request when it is a top-level
  window, and does not when embedded as tfx embeds it, so the terminal still
  renders opaque.
- `[colors] titleBarBackgroundActive` / `titleBarBackgroundInactive` had no
  visible effect across most of the pane header. The theme's generic
  `QWidget` background rule also matches the header's path container, so Qt
  marked it as styled and painted the panel background over the title bar —
  only the few pixels of layout margin kept the configured colour. The
  container is now cleared explicitly.
- Sorting did nothing at all once a column layout had been saved — which is
  every profile that has ever resized or reordered a column. The pane re-reads
  the stored layout on each model `layoutChanged`, and sorting emits exactly
  that, so every sort immediately restored the previously stored sort while
  the in-flight save was suppressed. Layout restores triggered by a model
  reorganisation no longer touch the sort, and the new sort is persisted once
  the layout-change window closes.
- The sort saved with the column layout was not restored on the next launch:
  the restore path blocks the header's signals, so the view never forwarded
  the sort to the model. The model is now sorted explicitly during a restore.
- Sorting by Date Modified, File Mode or Git Status did nothing. Those columns
  are synthesised past the ones the underlying model provides, so Qt could not
  resolve a source column to sort on and skipped sorting entirely. Sorting now
  runs on the name column with the chosen key applied in the comparator.
- Sorting by Type compared the unrelated source column that happens to share
  the Type column's index (the file size) instead of the type name.
- The sorted column had no visible marker: theming `QHeaderView::section`
  suppresses Qt's native sort arrow, so the sorted column now carries a ▲/▼ in
  its header title.

### Changed

- Dropping on the `..` row now resolves the parent directory instead of
  targeting a path ending in `/..`.
- The pinned-folder insertion line uses the configured
  `[colors] fileListRowDropTarget` accent like the other views, instead of a
  hardcoded green.
- Documentation: `[colors]` no longer lists `disabledForeground`,
  `scrollbarThumb`, `scrollbarThumbHovered`, `scrollbarThumbDragging` or
  `folderTreeFolderIcon`. All five were read from `config.toml` but never used
  by anything, so setting them did nothing. The notes now say what applies
  instead: scrollbars follow the platform style, and disabled items are dimmed
  from `fileForeground` via `[opacity] disabledItem`. Their parsing and the
  unused `AppColors` fields behind them were removed as well; `[colors]`
  ignores keys it does not know, so existing files keep loading without
  warnings.
- The `..` parent entry stays on the first row for every sort key and
  direction rather than being ordered like a file, and no longer fills in
  Type/Size/Created/Modified/Mode/Git — those described the folder above and
  only added noise. Only its name is shown.
- Rows that compare equal on the sorted column (same size, same timestamp,
  same mode) now fall back to name order, so the listing is deterministic
  instead of arbitrary. A size sort also groups directories ahead of files.

## [0.8.3] - 2026-07-28

### Changed

- `tfx` now forks and detaches from the launching terminal at startup, so the
  shell prompt returns immediately without appending `&`. Closing the terminal
  afterwards no longer terminates the application. Detached runs redirect
  stdin/stdout/stderr to `/dev/null` so Qt runtime warnings (e.g. the benign
  `qt.qpa.services` portal app-ID message) no longer print over the prompt;
  use `--foreground` to see them.

### Added

- New command-line option `-f` / `--foreground` keeps `tfx` attached to the
  terminal (previous behavior), e.g. for reading log output.
- `LICENSE` file: the project is now licensed under the Apache License 2.0,
  matching the original macOS version. The license file is installed with the
  documentation and declared in the DEB/RPM package metadata.

## [0.8.2] - 2026-07-26

Structural refactoring; no user-visible behavior changes.

### Internal

- The sidebar view classes (`FolderTreeView`, `PinnedListWidget`, the DISKS
  and folder-tree delegates) moved from `MainWindowSidebar.{h,cpp}` into
  `views/SidebarViews.{h,cpp}`; `MainWindowSidebar.cpp` now holds only the
  MainWindow-side sidebar logic.
- `applyTerminalTheme()` (460 lines) split into `buildThemeStyleSheet()`
  (stylesheet assembly, const) and `applyPaneThemeSettings()` (widget-side
  fonts/colors that a stylesheet cannot express), with a thin orchestrator
  on top — the config live reload path is now easier to follow.

## [0.8.1] - 2026-07-26

Hardening and internal refactoring; no user-visible behavior changes.

### Internal

- The configuration-error dialog renders as plain text so config.toml
  content can never be interpreted as rich text; the mount-table watch fd
  is closed on shutdown.
- The type-to-select matching, DISKS volume filter, and pinned-path
  display logic moved into testable core units (`core/TypeAhead`,
  `core/SidebarLogic`) with new test suites; AppConfig tests now cover
  `[window]` parsing and shortcut-conflict line numbers (14 suites total).

## [0.8.0] - 2026-07-26

Parity with the macOS 0.9.9 network-volume resilience fix, applied to the
Linux hazards, plus an optional integrated title bar.

### Added

- Optional integrated title bar: `[window] titleBar = "integrated"` hides
  the native title bar and moves the minimize/maximize/close controls into
  the menu bar. The empty menu-bar area drags the window (double-click
  maximizes) via startSystemMove, and window edges resize via
  startSystemResize, so it works on both X11 and Wayland. The default stays
  `system` (native title bar); the setting applies on the next launch.

### Changed

- The LEFT/RIGHT badge in the pane header is no longer shown; the title-bar
  background continues to mark the active pane.
- Pinned folders now show their directory as well: paths under the home
  directory start with "~" (e.g. `~/source/cpp/tfx`), the middle is elided
  when the sidebar is too narrow, and the full path shows in the tooltip.
- New quick-navigation keys in the file list: `~` jumps to the home
  directory, `/` to the filesystem root. Both take precedence over
  type-to-select.

### Fixed

- An unresponsive network mount (NFS/CIFS/sshfs) can no longer freeze the
  UI through the sidebar: the DISKS volume scan (statfs can block for
  minutes on a dead mount) now runs on a worker thread, with superseded
  scans discarded; the current-volume highlight and the folder-tree drop's
  same-volume detection use string matching against the cached mount roots
  instead of per-call syscalls. Directory enumeration itself was already
  off the main thread (QFileSystemModel's gatherer); folder sizes in the
  Properties dialog are tallied on a worker thread as of 0.7.10.
- The folder tree's scroll-to-top (0.7.7) now lands reliably: tree rows
  arrive asynchronously and later insertions shifted the scroll position,
  so the scroll is re-applied on each relevant directory load until the
  current folder's own listing has arrived. Selections made inside the
  tree still keep the tree's scroll position.

## [0.7.11] - 2026-07-26

Parity with tfx for Windows 0.9.10 / macOS 0.9.9: in-app config editing and
live reload.

### Added

- A settings gear at the end of the toolbar with **Edit Config File...** and
  **Editor Settings...**; both also live in the File menu. The new
  `editConfig` shortcut (default `Ctrl+,`) opens `config.toml` directly. The
  editor command is chosen in Editor Settings (`{path}` expands to the
  config file, environment variables are expanded, empty falls back to the
  OS association).
- `config.toml` changes apply on save: the file is watched
  (directory-level too, so atomic editor saves are caught) and shortcuts,
  theme, `[colors]`, `[opacity]`, `[font]`, `[openWith]`, `[preview]`, and
  `[[commands]]` re-apply live — the menu bar and all config-bound
  shortcuts are rebuilt. `[startup]` keeps its launch-time semantics.
- Configuration errors are now listed in a dialog (each entry prefixed with
  its `config.toml` line number) at startup and after each reload, in
  addition to the status bar; the app keeps running on the previous or
  default values.

## [0.7.10] - 2026-07-26

Parity with tfx for Windows 0.9.12 / macOS 0.9.9: a Properties dialog.
(The header-menu column controls and drag reordering from the same upstream
releases were already present in the Linux port.)

### Added

- **Properties** at the end of the file context menu (and in the File menu,
  default shortcut `Alt+Return`, config key `showProperties`): name, type,
  location, link target, size, permissions (mode string and octal),
  owner/group, and created/modified/accessed times for the selected item —
  or for the current folder when nothing is selected. Folder sizes are
  tallied on a worker thread with a live "Calculating…" placeholder, so a
  large or slow tree never blocks the UI; the tally stops when the dialog
  closes. Disabled for multi-selection and inside archives.

## [0.7.9] - 2026-07-26

Parity with tfx for Windows 0.9.13 / macOS 0.9.9: drop onto the folder tree.

### Added

- Files and folders dragged from the file panes (or external apps) can now
  be dropped onto folder-tree nodes. Dropping on the same volume defaults
  to Move and across volumes to Copy; Shift forces Move and Ctrl forces
  Copy. The hovered node is highlighted without being selected (selection
  would navigate the pane), a collapsed node auto-expands after a short
  hover, and drops run through the pane's shared pipeline — including the
  folder-into-itself guard and the overwrite/skip/rename conflict prompt.
  Tree nodes themselves remain non-draggable.

## [0.7.8] - 2026-07-26

Parity with tfx for Windows 0.9.10/0.9.12 (via macOS 0.9.9): DISKS sidebar
section and collapsible sidebar sections.

### Added

- A DISKS sidebar section between PINNED and FOLDERS lists every mounted
  browsable volume (root filesystem, real block devices, network mounts —
  snap/loop mounts and /boot partitions are skipped) with a thin usage bar.
  The tooltip shows free/total space, clicking a disk opens its mount point
  in the active pane, and the volume containing the current folder stays
  highlighted. The list refreshes automatically on mount/unmount via a
  poll-free watch on /proc/self/mounts.
- The PINNED / DISKS / FOLDERS section headers are now clickable and
  collapse their section; a chevron shows the state, which persists across
  sessions.

## [0.7.7] - 2026-07-26

Parity with tfx for Windows 0.9.15 / macOS 0.9.10: type-to-select and
folder-tree navigation refinements.

### Added

- Explorer/Finder-style type-to-select in the file panes. Typing a printable
  character jumps the selection to the first row whose name starts with it;
  more characters within one second extend the prefix. Pressing the same
  single character repeatedly cycles through the rows with that initial,
  wrapping at the end. A key that matches nothing keeps the current prefix
  and selection. The active prefix shows as "Find: …" in the pane status
  line; it resets on navigation. Text fields, the terminal, and shortcuts
  with modifiers are unaffected.

### Changed

- The folder tree now mirrors the pane's location: navigation arriving from
  outside the tree (file list, pinned folders, path bar) scrolls the current
  folder's node to the top of the tree, expands it one level so the listed
  subfolders are visible, and collapses expanded branches that are off the
  new path. Clicking a node inside the tree keeps the tree's own scroll
  position.

## [0.7.6] - 2026-07-24

### Changed

- The two file panes now live in one dock connected by a splitter, so they
  always stay adjacent and float together as a single unit instead of being
  separable into two detached windows. Toggling split view happens inside
  the splitter and no longer disturbs the other docks' widths.
- Turning on split view now divides the single file list's width evenly, so
  both panes open at the same width instead of Qt's arbitrary redistribution.
  The splitter position is saved and restored across sessions.

### Fixed

- The folder tree pane no longer changes width when the preview pane is
  shown or hidden. Toggling a dock made Qt redistribute the horizontal space
  across all docks; the sidebar width is now pinned during the relayout, the
  same way the split-view toggle already handled it.

## [0.7.5] - 2026-07-09

### Fixed

- Ctrl+C in the preview pane now copies the selected preview text instead of
  triggering the window-wide "Copy Items" (file copy) shortcut. Read-only
  preview text widgets don't reserve Ctrl+C via Qt's shortcut-override
  mechanism, so the global copy action used to win even with an active text
  selection in the preview.

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
