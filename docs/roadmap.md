# tfx for Linux Roadmap

Current version: **0.3.2**

This roadmap tracks the practical development steps for the Linux Qt port as it
moves toward feature parity with tfx for Windows (0.6.x). It is phased so each
stage is buildable and verifiable on its own.

## 0.3.0 — Phase 1: Usability (done)

- Application version shown in the status bar.
- Current Git branch shown per pane in the status line.
- "Create Link" (symlink) in the file context menu.
- "Open With → Other..." using the desktop's native application chooser.
- Multi-selection preview summary (count, total size, item list).
- Auto-refresh of the file list and Git status on directory changes.

## 0.3.1 — Phase 2: Core file interactions

- Drag-and-drop between panes and to/from external file managers; drop onto a
  folder to move, or hold Ctrl to copy. (done)
- Recursive subfolder search started with Enter, streamed into a results view,
  and cancelled on navigation. (done)
- Remaining: drop onto the folder tree and pinned list.
- Remaining: conflict handling and progress/cancellation for longer file
  operations.

## 0.5.0 / 0.6.0 — Phase 3: Larger subsystems

- Interactive terminal pane backed by a real PTY (colors, cursor, `Ctrl+C`,
  path drag-in), replacing the current command-output pane.
- Browse ZIP archives as folders (navigate in, extract individual entries,
  drag out).
- Icon view mode in addition to the details view, with persistence.

## 0.6.x — Phase 4: Polish and hardening

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
