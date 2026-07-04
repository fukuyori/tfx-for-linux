# tfx for Linux Roadmap

Current version: **0.6.3**

This roadmap tracks the practical development steps for the Linux Qt port as it
moves toward feature parity with tfx for Windows (0.6.x) and tfx for macOS
(0.8.x). It is phased so each stage is buildable and verifiable on its own.

## 0.3.0 — Phase 1: Usability (done)

- Application version shown in the status bar.
- Current Git branch shown per pane in the status line.
- "Create Link" (symlink) in the file context menu.
- "Open With → Other..." using the desktop's native application chooser.
- Multi-selection preview summary (count, total size, item list).
- Auto-refresh of the file list and Git status on directory changes.

## 0.3.1 — Phase 2: Core file interactions (done)

- Drag-and-drop between panes and to/from external file managers; drop onto a
  folder to move, or hold Ctrl to copy. (done)
- Recursive subfolder search started with Enter, streamed into a results view,
  and cancelled on navigation. (done)
- Pinned list: reorder by dragging and drop folders to pin them. (done) The
  folder tree is intentionally excluded from drag-and-drop.

## Phase 3: Larger subsystems (done)

- Icon view mode in addition to the details view, per pane, with persistence. (done)
- Browse ZIP archives as folders (navigate in, open/extract entries). (done)
- Interactive terminal pane backed by QTermWidget (true colour, colour scheme,
  cursor, `Ctrl+C`, TUI apps), with the working directory following the active
  pane and font/colour scheme from `config.toml`. Falls back to the simple
  command pane when QTermWidget is unavailable. (done)
- Terminal follow-ups: a folder-sync button reads the terminal working
  directory, and sessions persist across show/hide toggles. (done)

## Phase 4: User-defined commands (from tfx macOS 0.7.0)

- `[[commands]]` section in `config.toml` for custom context-menu commands. (done)
- Token expansion: `{path}`, `{paths}`, `{dir}`, `{name}`, `{stem}`, `{ext}`,
  `{cwd}`, `{scripts}`. (done)
- User-defined keyboard shortcuts with conflict detection. (done)
- A command Output dock showing the result and history of runs. (done)
- English and Japanese documentation for command configuration. (done)

## Phase 5: File-operation robustness and clipboard (from tfx macOS 0.8.0 / 0.8.3)

- Inline progress for copy/move with cancellation, run on a background queue so
  the UI stays responsive. (done)
- Quit-safety prompt while file operations are in flight; partial-file cleanup
  on cancel. (done)
- Visible queue status and queue clearing when canceling file operations. (done)
- Stop and clear queued file operations after a failed batch. (done)
- Unit coverage for the background file-operation worker, including fallback
  moves and cancellation cleanup. (done)
- Conflict handling for paste/drop (overwrite / skip / rename). (done)
- Clipboard-to-file materialization with format detection (CSV, TSV, RTF, URL,
  PNG, TXT) and a "Paste as Plain Text" action. (done)

## Phase 6: View and interaction polish (from tfx macOS 0.7.2–0.7.96)

- Folder-tree visibility toggle. (done)
- Folder-tree collapse-all button. (done)
- Click-to-sort column headers with ascending/descending indicators and saved
  sort state; drag-to-reorder columns remains supported. (done)
- Markdown rendering improvements: horizontal rules, ordered lists, tables,
  embedding local images as `data:` URLs, and remote-image external button. (done)
- Preview-pane keyboard shortcuts (source/rendered toggle, open externally). (done)
- Drag-and-drop refinements: highlight the target row/item, show a pane drop
  border when dragging over empty area, refresh affected panes immediately, and
  avoid snap-back on a successful drop. (done)
- Startup visibility options for terminal/preview/folder-tree. (done)
- Window geometry option (`-g`/`--geometry` and `config.toml`). (done)
- `[naming]` config for placeholder language (auto/en/ja). (done)

## Phase 7: Theme, hardening, and packaging (done)

- Light/dark theme switching driven by `config.toml`. (done)
- PDF preview disk cache and multi-stage fallback rendering. (done)
- Security hardening for Markdown/HTML preview, ZIP extraction, Git, and the
  terminal launcher.
  - HTML previews render escaped source, Markdown remote images are link-only,
    and preview links are restricted to `http`/`https`. (done)
  - ZIP entry path validation before browsing/extracting archives. (done)
  - Terminal and user-command working directories are canonicalized and
    validated before process launch. (done)
  - Git status refresh canonicalizes query directories and ignores status paths
    that would escape the queried directory. (done)
- Packaging: install/uninstall targets, desktop entry and icon, distribution
  notes, and CI build checks. (done)

## 0.6.0 Release Closure (done)

- Multi-tab workflow refinements: close buttons/tooltips, duplicate-tab
  suppression, tab context menu, and state save/restore hardening. (done)
- Search and filter refinements: search-result keyboard, preview, status,
  context menu, sorting, close controls, and search history. (done)
- Test coverage for path handling, configuration, column configuration, Git,
  search/tab state, and file-operation helpers. (done)
- FilePane refactor: split UI setup, signal wiring, navigation, state,
  actions, file operations, clipboard/drop, archives, columns, tabs, search,
  and commands into focused translation units. (done)

## 0.6.3 — Phase 8: File safety (from tfx macOS 0.9.2) (done)

Data-loss prevention in the copy/move/paste pipeline. Every item ships with
Qt Test coverage in the file-operation worker suite.

- Copy symbolic links as links instead of materializing their targets, and do
  not recurse into directories reached through a link
  (`FileOperationWorker::copyPath`, `copyRecursively`). Preserve
  relative/absolute link text as-is; removing a moved link never touches the
  link target's contents. (done)
- Include hidden and system entries when copying directory trees; dotfiles
  were silently skipped before. (done)
- Atomic replace on overwrite: the copy is written to a hidden temporary name
  in the destination directory and swapped in with `rename(2)` on success, so
  a mid-copy failure never destroys the existing file. Directory replacement
  moves the old tree aside first and rolls it back if the swap fails. The UI
  no longer deletes the destination before the operation starts. (done)
- Canonicalize source and destination in the shared
  `transferWouldNestInsideSource` helper so the self/descendant copy guard
  cannot be bypassed through a symlinked path. (done)
- Treat write/flush/close errors as copy failures instead of reporting
  success. (done)
- Regression tests: link-preserving copy (file, directory, and broken links),
  replace failure keeps the original, symlinked nesting detected, write error
  fails the batch, hidden files copied, moved directory link keeps target
  contents. (done)

## 0.6.4 — Phase 9: Stability and process hygiene (from tfx macOS 0.9.3)

External-process and long-run robustness so a hung tool or huge tree cannot
wedge the UI.

- Git watchdog: give every `git status` invocation a hard timeout
  (30 s, terminate then kill), drain stderr concurrently, and scope the query
  to the current directory with a pathspec in addition to the existing
  debounce; throttle refreshes to at most one per second per pane.
- Bound subfolder search with an explicit depth limit (128) as a second guard
  beside the existing no-follow-symlinks iterator flags, and surface "search
  truncated" in the status line when the bound is hit.
- Audit every `QProcess` site (`zip`, `unzip`, git, user commands) for output
  handling that could stall on full pipe buffers; read incrementally instead
  of relying on `waitForFinished` + `readAll`.
- Terminal shutdown: confirm QTermWidget teardown reaps the shell (no zombie
  processes after closing the pane or the window); add explicit
  SIGTERM-then-SIGKILL escalation if it does not.
- Move pre-copy byte tallies and free-space checks for progress reporting off
  the UI thread if any remain there.
- Crash audit: replace unchecked pointer/optional assumptions on the
  preview/shortcut/QuickLook-equivalent paths with graceful degradation.

## 0.6.5 — Phase 10: Security hardening, round 2

Follow-up to the Phase 7 hardening, focused on untrusted file content and
filenames reaching shells or parsers.

- Preview resource limits: stream CSV/TSV parsing with hard caps
  (1,000 rows × 100 columns) and RFC 4180 quote handling instead of the
  current 200-line `split()`; replace the flat 256 KB text read with a shared
  size-capped loader (staged read, explicit cap, "truncated" indicator) used
  by text, Markdown, JSON, and CSV previews.
- Argument-injection audit for external commands: pass `--` before
  user-controlled paths for `zip`, `unzip`, `git`, and trash/open helpers so
  filenames beginning with `-` cannot become options.
- ZIP hardening follow-up: reject symlink entries on extraction (zip-slip via
  link targets), cap total extracted size/entry count, and extract into a
  temporary directory before moving into place.
- User-command token expansion: re-audit `{path}`/`{paths}` quoting so
  filenames containing quotes, `$`, backticks, or newlines cannot break out of
  the generated command line; document the guarantee in the commands docs.
- Clipboard materialization: sanitize generated file names (strip path
  separators and control characters) and create files with `0600`-style
  conservative permissions before content is written.
- Config robustness: fuzz-style tests for `config.toml` parsing (oversized
  values, invalid UTF-8, deeply nested arrays) so a broken config degrades to
  defaults with a warning instead of crashing at startup.

## 0.6.6 — Phase 11: Performance (from tfx macOS 0.9.3 / 0.9.4)

Measured before/after on a large directory (100k entries) and a deep search
tree; no behavior changes.

- Cache file icons per extension (single stat/lookup per extension, not per
  row) for the search-results and ZIP views that currently call
  `QFileIconProvider` per item.
- Insert search-result batches incrementally instead of rebuilding lookups per
  batch; match names before constructing full row items.
- Evaluate seeding folder-tree children from completed pane listings to avoid
  double enumeration; skip if the independent `QFileSystemModel` tree makes
  the win negligible, and record the decision here.
- Case-only rename support (`foo` → `Foo`) via a two-step rename on
  case-insensitive mounts (exFAT/NTFS/ciopfs); no-op on case-sensitive
  filesystems.
- Profile directory load and scroll after the above and note remaining
  hotspots as candidates for the next phase.
