# tfx for Linux Roadmap

Version target: **0.2.x -> 0.3.0**

This roadmap tracks the next practical development steps for the Linux Qt port. It is intentionally focused on the file manager experience before packaging and broader platform polish.

## 0.2.x Stabilization

- Stabilize file-list selection across mouse, keyboard, directory changes, sorting, and filtering.
- Verify split/preview pane resizing across repeated show/hide operations and restored sessions.
- Finish preview reliability for images, Markdown, HTML, PDF, CSV, TSV, JSON, and plain text.
- Review keyboard behavior for common navigation flows: Enter, Backspace, Tab, Shift+Tab, arrow keys, and rename.
- Keep the folder tree compact and readable without folder icons.
- Improve error messages for missing tools such as PDF render helpers.

## 0.3.0 File Operations

- Harden copy, cut, paste, rename, trash, and folder/file creation behavior.
- Add conflict handling for paste, extract, and archive creation.
- Add progress feedback for longer-running file operations.
- Add safer cancellation behavior for operations that can take time.
- Expand context menus based on the current tfx behavior.

## 0.4.0 Preview And Metadata

- Improve preview-pane metadata for owner, group, permissions, symlink targets, MIME/type details, and Git status.
- Add preview fallback states for unsupported, binary, very large, or unreadable files.
- Improve rendered preview styling for Markdown, HTML, and tabular data.
- Add explicit source/rendered preview mode persistence.

## 0.5.0 Configuration And Customization

- Complete `config.toml` support for terminal and open-with behavior.
- Add validation feedback for unsupported config keys and values.
- Add user-configurable pinned folders.
- Add configurable preview limits and file association rules.
- Document all supported settings in Japanese and English.

## 0.6.0 Packaging

- Add install and uninstall targets.
- Prepare desktop entry and application icon assets.
- Add packaging notes for common Linux distributions.
- Add CI build checks for the Qt project.

## Later

- Multi-tab workflow improvements.
- Search and filter refinements.
- Optional terminal integration improvements.
- Test coverage for path handling, column configuration, and file operation helpers.

