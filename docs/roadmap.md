# tfx for Linux Roadmap

Current version: **0.5.5**

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
- Remaining terminal follow-ups: a folder-sync button reading the foreground
  process working directory, and session persistence across show/hide toggles.

## Phase 4: User-defined commands (from tfx macOS 0.7.0)

- `[[commands]]` section in `config.toml` for custom context-menu commands. (done)
- Token expansion: `{path}`, `{paths}`, `{dir}`, `{name}`, `{stem}`, `{ext}`,
  `{cwd}`, `{scripts}`. (done)
- User-defined keyboard shortcuts with conflict detection. (done)
- A command Output dock showing the result and history of runs. (done)
- English and Japanese documentation for command configuration. (done)

## Phase 5: File-operation robustness and clipboard (from tfx macOS 0.8.0 / 0.8.3)

- Inline progress for copy/move with cancellation, run on a background queue so
  the UI stays responsive.
- Quit-safety prompt while file operations are in flight; partial-file cleanup
  on cancel.
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

## Phase 7: Theme, hardening, and packaging

- Light/dark theme switching driven by `config.toml`.
- PDF preview disk cache and multi-stage fallback rendering.
- Security hardening for Markdown/HTML preview, ZIP extraction, Git, and the
  terminal launcher.
- Packaging: install/uninstall targets, desktop entry and icon, distribution
  notes, and CI build checks.

## Later

- Multi-tab workflow refinements.
- Search and filter refinements.
- Test coverage for path handling, column configuration, and file-operation
  helpers.
