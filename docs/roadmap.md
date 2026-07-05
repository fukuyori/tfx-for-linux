# tfx for Linux Roadmap

Current version: **0.7.0**

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

## 0.6.4 — Phase 9: Stability and process hygiene (from tfx macOS 0.9.3) (done)

External-process and long-run robustness so a hung tool or huge tree cannot
wedge the UI.

- Git watchdog: every git invocation (branch, prefix, status) now carries a
  hard timeout (terminate after 30 s, kill 5 s later), discards stderr, and
  the status query is scoped to the current directory with a `-- .` pathspec.
  Same-directory refreshes are throttled to at most one per second per pane;
  navigation refreshes stay immediate. (done)
- Porcelain paths are now rebased through `git rev-parse --show-prefix`
  (shared `porcelainPathInDirectory` helper with tests), fixing status badges
  that were wrong or missing when the pane showed a repository
  subdirectory — porcelain output is repository-root relative. (done)
- Bounded subfolder search with an explicit depth limit (128) by walking one
  non-recursive iterator at a time over a pending-directory queue, and the
  completion message notes when the depth limit was hit. (done)
- `QProcess` audit: Qt drains child pipes into internal buffers, so no
  OS-level pipe-buffer deadlock exists. Real findings fixed: archive
  compress/extract waited with `waitForFinished(-1)` freezing the UI
  indefinitely (now waits in a local event loop with user input excluded, so
  the window keeps painting), and user-command output was buffered without
  bound (now read incrementally with a 1 MiB cap per stream and a truncation
  notice). The bounded `unzip`/`pdftoppm` waits in the platform layer keep
  their finite timeouts. (done)
- Terminal shutdown: QTermWidget teardown only sends SIGHUP, which a shell
  that traps it survives. The pane destructor now sends SIGTERM, reaps with a
  short deadline, and escalates to SIGKILL so no shell outlives the
  application. (done)
- Pre-copy tallies and free-space checks: audited; entry counting already
  runs on the worker thread and no free-space queries exist. (done)
- Crash audit: `.first()`/`.last()`/`takeFirst()` call sites are all guarded
  by emptiness checks; no unchecked-deref pattern equivalent to the macOS
  force-unwrap fixes was found. (done)

## 0.6.5 — Phase 10: Security hardening, round 2

Follow-up to the Phase 7 hardening, focused on untrusted file content and
filenames reaching shells or parsers.

- Preview resource limits: CSV/TSV previews now parse through a shared RFC
  4180 parser (`core/DelimitedText`, quoted fields, embedded delimiters and
  newlines) capped at 1,000 rows × 100 columns with a visible limit notice,
  and text previews load through a shared size-capped loader
  (`core/PreviewText`, 4 MB cap, UTF-8-boundary-safe truncation with an
  indicator) instead of a flat 256 KB read. (done)
- Argument-injection audit: `zip` compression now passes `--` before
  user-controlled names. `unzip` has no reliable end-of-options separator
  (verified: a `-d<dir>` entry name redirects extraction), so
  `zipEntryPathIsSafe` now also rejects entry names beginning with `-`.
  git invocations use fixed arguments with absolute canonical directories,
  and open/trash helpers pass absolute paths — no other injectable sites
  found. (done)
- ZIP hardening follow-up: archives containing symbolic-link entries are
  refused for full extraction (zip-slip via link targets; `unzip` extracts
  links by default — verified) and link entries cannot be opened from the
  archive browser. Full extraction is capped at 100,000 entries / 4 GiB
  uncompressed (via `inspectZipArchive`, which parses `zipinfo` modes and
  sizes) and extracts into a hidden work directory that is renamed into place
  only on success. (done)
- User-command token expansion audit: tokens expand inside POSIX single
  quotes with embedded quotes escaped as `'"'"'`, so quotes, `$`, backticks,
  and newlines in file names stay literal. The guarantee is now documented in
  `docs/configuration.md`. (done)
- Clipboard materialization: files created from clipboard content are opened
  with `NewOnly` and restricted to owner read/write before content is
  written; base names are fixed internal placeholders, so no name
  sanitization is needed. (done)
- Config robustness: malformed-config test covers invalid UTF-8, an
  unterminated string, a 2 MB value, control characters, and junk syntax;
  the parser degrades to defaults without crashing. (done)

## 0.6.6 — Phase 11: Performance (from tfx macOS 0.9.3 / 0.9.4) (done)

Measured before/after; no behavior changes.

- Search results now resolve file icons through a session-lifetime
  per-extension cache instead of one `QFileIconProvider` lookup per row, and
  the ZIP browser reuses two generic icons per fill instead of one provider
  call per entry. Microbenchmark (5,000 files, 10 extensions, offscreen):
  18 ms per-row vs 2 ms cached — a ~9× reduction, larger under real icon
  themes. (done)
- Search-result insertion audited: rows already stream into the model
  incrementally (no lookup rebuilds), the name match runs before any row
  items are constructed, and `QTableView` does not re-sort on append, so no
  change was needed. (done)
- Folder-tree seeding: skipped. The tree is an independent, lazily-populated
  `QFileSystemModel` that only enumerates expanded nodes; sharing pane
  listings with it would mean replacing the model for a negligible win. (done)
- Case-only renames (`foo` → `Foo`) now hop through a hidden temporary name
  (shared `renameWithinDirectory` helper with tests, wired into the file
  list's rename editing), so they work on case-insensitive mounts
  (exFAT/NTFS) where a direct rename is refused or becomes a no-op. (done)
- Load check: startup into a 100,000-entry directory populates without
  app-side per-row work (the pane list is `QFileSystemModel`'s async native
  enumeration; proxy columns compute on demand for visible rows only). No
  remaining app-side hotspot identified on the load path. (done)
