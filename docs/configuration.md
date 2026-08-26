# tfx for Linux Configuration

Linux | Based on the tfx for Windows configuration model

tfx for Linux stores user-editable configuration under:

```text
~/.config/tfx/
```

The main user-editable configuration file is `config.toml`. tfx creates it on startup when it does not already exist. Existing files are not overwritten.

`config.toml` can be opened from the app via the toolbar gear menu ("Edit Config File...", default shortcut `Ctrl+,`); the editor used is chosen in "Editor Settings...". Saves apply immediately: shortcuts, `theme`, `[colors]`, `[opacity]`, `[font]`, `[openWith]`, `[preview]`, and `[[commands]]` reload live. `[startup]` keeps its launch-time semantics. Configuration errors are listed in a dialog with `config.toml` line numbers; the app keeps running on the previous or default values.

Session state such as window placement, last-opened paths, pinned folders, column layout, splitter sizes, and view mode remains app-owned state. Use `config.toml` for hand-written preferences.

## Current Scope

`config.toml` supports these sections:

- Top-level `version = 1`
- Top-level `theme = "dark"` or `theme = "light"`
- `[window]`
- `[font]`
- `[colors]`
- `[opacity]`
- `[startup]`
- `[naming]`
- `[shortcuts]`
- `[terminal]`
- `[openWith]`
- `[preview]`, `[preview.extensions]`, `[preview.markdown]`
- `[[commands]]`

The loader intentionally accepts a small TOML subset:

- Tables with `[section]` headers
- Assignments with `key = value`
- Double-quoted strings
- Arrays of double-quoted strings for `rightFolders`
- Array tables with `[[commands]]`
- Numeric font sizes
- Quoted `#RRGGBB` colors
- Decimal opacity values between `0.0` and `1.0`
- `#` comments outside quoted strings

Unsupported sections and unsupported keys are ignored. Invalid values fall back to built-in defaults and surface a status warning instead of crashing the app.

## Default File

New installations create a Linux-native file like this:

```toml
version = 1
theme = "dark"

[font]
ui = "system"
mono = "monospace"
size = 12
# Optional per-pane overrides (family and/or size):
# fileList = "monospace"
# fileListSize = 12
# preview = "monospace"
# previewSize = 12
# terminal = "monospace"
# terminalSize = 12
# folderTree = "monospace"
# folderTreeSize = 12

[shortcuts]
reload = "f5"
openTerminal = "ctrl+shift+t"
togglePreview = "ctrl+shift+p"
togglePreviewSource = "ctrl+shift+r"
openPreviewExternal = "ctrl+shift+i"
toggleSplit = "ctrl+backslash"
focusSearch = "ctrl+f"
sortOptions = "ctrl+shift+s"
toggleHidden = "ctrl+shift+."
goBack = "alt+left"
goForward = "alt+right"
goUp = "alt+up"
newFolder = "ctrl+shift+n"
newFile = "ctrl+n"
rename = "f2"
moveToTrash = "delete"
copyItems = "ctrl+c"
cutItems = "ctrl+x"
pasteItems = "ctrl+v"
newTab = "ctrl+t"
closeTab = "ctrl+w"
prevTab = "ctrl+shift+["
nextTab = "ctrl+shift+]"
toggleTerminal = "ctrl+j"
quit = "ctrl+q"

# [startup]
# layout = "restore"
# preview = "restore"
# terminal = "restore"
# folderTree = "restore"
# geometry = "1280x780+80+40"
# rightFolder = "~/Downloads"
# rightFolders = ["~/Downloads", "~/Documents"]

# [naming]
# placeholderLanguage = "auto"

# [opacity]
# background = 0.65     # window background opacity (0.0 = transparent, 1.0 = opaque)
# inactivePane = 0.5    # opacity of the inactive file pane
# disabledItem = 0.45   # opacity of disabled items

# [colors]
# fileListBackground = "#1E1E28"
# fileForeground = "#F2F2F2"
# directoryForeground = "#FFFFFF"
# paneBorderKeyboardTarget = "#AAAAAA"

# [terminal]
# app = "x-terminal-emulator"
# arguments = "--working-directory={path}"

# [openWith]
# md = "code"
# * = "xdg-open"

# [[commands]]
# name = "Open in VS Code"
# run = "code {paths}"
# shortcut = "ctrl+shift+o"
# target = "any"
# terminal = false
```

## Window

`[window]` controls the window chrome:

```toml
[window]
titleBar = "system"   # system or integrated
```

- `system` (default): the native window manager title bar.
- `integrated`: the title bar is hidden and the minimize/maximize/close
  controls move into the menu bar. Drag the empty menu-bar area to move the
  window (double-click maximizes); window edges and the status-bar grip
  resize.

Unlike the rest of the configuration, `titleBar` is applied at launch —
changing it takes effect after a restart.

## Fonts

`[font]` sets the global UI font (`ui`), the monospace family used across the
file areas (`mono`), and the base point `size`.

Each pane can override the global monospace font and/or size. An empty family or
a size of `0` (i.e. the key omitted) inherits the global values.

- `fileList` / `fileListSize` — the file list (details and icon views, search
  and archive views).
- `preview` / `previewSize` — the preview pane (source and rendered text).
- `terminal` / `terminalSize` — the terminal pane.
- `folderTree` / `folderTreeSize` — the folder tree and pinned list.

```toml
[font]
mono = "monospace"
size = 12
terminal = "JetBrains Mono"
terminalSize = 13
```

Notes:

- For the terminal, prefer a strictly monospaced family (for Nerd Fonts, the
  "... Nerd Font Mono" variant) so TUI applications keep their column
  alignment. Icon glyphs in the non-Mono variants occupy more than one cell.
- `terminalSize` is in **points**; every other size here is in **pixels**. The
  same number therefore renders larger in the terminal than in the file list.
- `mono` is the fallback for the preview and the terminal only. The menus, file
  list and folder tree follow `ui` unless `fileList` / `folderTree` name a
  family of their own.
- The file-list font is applied as a widget font so text elision uses the
  same metrics as the painted glyphs.

## Startup

`[startup] layout` accepts `single`, `split`, or `restore`.

`[startup] preview` accepts `show`, `hide`, or `restore`.

`[startup] terminal` accepts `show`, `hide`, or `restore`.

`[startup] folderTree` accepts `show`, `hide`, or `restore`.

`[startup] rightFolder` sets the folder the **right** pane opens at. It accepts
an absolute path or a `~`-prefixed one, which is expanded against `$HOME`:

```toml
[startup]
rightFolder = "~/Downloads"
```

`[startup] rightFolders` is a list of candidates for the same purpose. The first
entry that exists as a directory is used, so one config file can cover machines
that do not all have the same folders:

```toml
[startup]
rightFolders = ["~/Downloads", "~/Documents", "/srv/share"]
```

Both keys may be set together. `rightFolders` is checked first and wins when any
of its entries resolves; `rightFolder` is the fallback. If nothing resolves —
every listed folder is missing, or neither key is set — the right pane keeps the
folder it had when tfx last exited.

Note that these keys override the restored session: they are applied after the
saved tabs are restored, so the right pane opens at the configured folder on
every launch. There is no `leftFolder`; the left pane opens the folder given on
the command line, or the current working directory.

`geometry` accepts `WIDTHxHEIGHT` or `WIDTHxHEIGHT+X+Y`. The command-line
option `-g` / `--geometry` uses the same format and takes precedence over
`config.toml`.

At startup `tfx` forks and detaches from the launching terminal, so the shell
prompt returns immediately. The command-line option `-f` / `--foreground`
disables this and keeps `tfx` attached to the terminal.

## Naming

`[naming] placeholderLanguage` controls generated placeholder names such as
new files, new folders, links, archives, and clipboard-created files.

Accepted values:

- `auto` — follow the system UI language.
- `en` — use English placeholder names.
- `ja` — use Japanese placeholder names.

```toml
[naming]
placeholderLanguage = "en"
```

## Shortcuts

`[shortcuts]` overrides keyboard shortcuts by action name. Values use the Qt key
sequence grammar (e.g. `"Ctrl+Shift+P"`, `"Alt+Left"`, `"F5"`). Defaults below
are the Linux defaults; conflicting bindings are reported as configuration
warnings and fall back to the defaults.

| Action | Default | Description |
| --- | --- | --- |
| `reload` | `F5` | Reload the active pane. |
| `focusSearch` | `Ctrl+F` | Focus the search field. |
| `sortOptions` | `Ctrl+Shift+S` | Open the Sort Options chooser for the active pane. |
| `togglePreview` | `Ctrl+Shift+P` | Show/hide the preview pane. |
| `togglePreviewSource` | `Ctrl+Shift+R` | Toggle rendered/source in the preview. |
| `openPreviewExternal` | `Ctrl+Shift+I` | Open the current preview externally. |
| `toggleSplit` | `Ctrl+\` | Show/hide split view. |
| `toggleHidden` | `Ctrl+Shift+.` | Show/hide hidden files. |
| `toggleTerminalPane` | `Ctrl+J` | Show/hide the built-in terminal pane. (alias: `toggleTerminal`) |
| `focusTerminalPane` | `Ctrl+Alt+J` | Show and focus the terminal pane. |
| `openTerminal` | `Ctrl+Shift+T` | Open the external terminal app at the current folder. |
| `swapPanes` | `Ctrl+Shift+X` | Swap the two panes' current folders. |
| `goBack` / `goForward` / `goUp` | `Alt+Left` / `Alt+Right` / `Alt+Up` | Navigation. |
| `openItem` | `Ctrl+O` | Open the selected item. |
| `newFile` / `newFolder` | `Ctrl+N` / `Ctrl+Shift+N` | Create file / folder. |
| `rename` | `F2` | Rename the selection. |
| `moveToTrash` | `Del` | Move the selection to Trash. |
| `showProperties` | `Alt+Return` | Show properties for the selection (or the current folder). |
| `editConfig` | `Ctrl+,` | Open config.toml in the configured editor. |
| `compressToZip` / `extractZip` | `Ctrl+Alt+Z` / `Ctrl+Alt+E` | Zip / unzip the selection. |
| `copyItems` / `cutItems` | `Ctrl+C` / `Ctrl+X` | Copy / cut the selection. |
| `pasteItems` / `movePasteItems` | `Ctrl+V` / `Ctrl+Shift+V` | Paste (copy) / paste-move. |
| `copyPath` | `Ctrl+Shift+C` | Copy the selected path(s). |
| `selectAll` | `Ctrl+A` | Select all visible items. |
| `revealInFinder` | `Ctrl+Alt+R` | Reveal the selection in the file manager. |
| `newTab` / `closeTab` | `Ctrl+T` / `Ctrl+W` | Open / close a tab. |
| `previousTab` / `nextTab` | `Ctrl+Shift+[` / `Ctrl+Shift+]` | Switch tabs. (alias for previous: `prevTab`) |
| `quit` | `Ctrl+Q` | Quit tfx. |

```toml
[shortcuts]
toggleTerminalPane = "ctrl+j"
selectAll = "ctrl+a"
copyPath = "ctrl+shift+c"
```

## Opacity

`[opacity]` controls window and pane transparency. Each value is a decimal between `0.0` (fully transparent) and `1.0` (fully opaque).

The default theme ships translucent (`background = 0.65`). Set it to `1.0` for a
solid window, which is also what you get without a compositing window manager.

Effective on the Linux port:

- `background` — overall window background opacity. Values below `1.0` enable a translucent window surface, so the desktop shows through behind the panes (requires a compositing window manager). The menu bar, toolbar, folder sidebar, file lists, preview and status bar all render at this value. The terminal pane does not: QTermWidget paints its colour scheme opaquely when embedded, so it stays solid.
- `inactivePane` — opacity applied to the file pane that is not currently active.
- `disabledItem` — opacity applied to disabled controls and menu items.
- `dropIndicator` — opacity of the pinned-folder drop insertion indicator.

```toml
[opacity]
background = 0.40
inactivePane = 0.5
disabledItem = 0.45
dropIndicator = 0.85
```

The following keys are accepted for compatibility with tfx for macOS but have no
visual effect on the Linux port, because the corresponding affordance does not
exist here: `headerSecondary`, `selectedParentRow`, `dragPreview`,
`dragPreviewShadow`, `subtleBackground`.

## Colors

Set `theme = "dark"` or `theme = "light"` at the top level to choose the built-in
palette. `[colors]` values override the selected theme.

Values must be quoted `#RRGGBB` strings. Tokens are optional; missing tokens keep
the selected theme's colour.

File pane / list rows:

- `fileListBackground`
- `fileListRowHovered`
- `fileListRowSelected`
- `fileListRowSelectedForeground`
- `fileListRowDropTarget` — highlight colour for the in-progress drop target.
- `fileForeground`
- `directoryForeground`
- `secondaryForeground`

File pane chrome:

- `headerForeground`
- `headerBackground`
- `inputBackground`
- `titleBarBackgroundActive` / `titleBarBackgroundInactive` — pane title-bar
  (badge + path row) background, by active state.
- `statusLineBackground` / `statusLineForegroundActive` /
  `statusLineForegroundInactive` — pane status line.

Pane borders:

- `paneBorderInactive`
- `paneBorderActive`
- `paneBorderKeyboardTarget`

Folder tree:

- `folderTreeBackground`
- `folderTreeForeground`
- `folderTreeSelectedForeground`
- `folderTreeSelectedActive` — selected row while the tree has focus.
- `folderTreeSelectedInactive` — selected row while the tree is unfocused.
- `folderTreeSectionHeader` — `PINNED` / `FOLDERS` section headers.

Split handle:

- `splitHandleIdle`
- `splitHandleActive`

Git status badges (colour of the per-row Git status letter):

- `gitModified` / `gitAdded` / `gitDeleted` / `gitRenamed` / `gitUntracked` /
  `gitIgnored` / `gitConflicted`

Notes:

- `paneBorderActive` and `paneBorderKeyboardTarget` currently map to the same
  active-border colour.
- Scrollbars follow the platform style and are not themed by `[colors]`.
- Disabled items are dimmed from `fileForeground` through
  `[opacity] disabledItem` rather than taking a colour of their own.

## Terminal

`[terminal] colorScheme` sets the built-in QTermWidget colour scheme for the
terminal pane (this determines the background and foreground colours). Available
schemes include: `DarkPastels`, `Falcon`, `GreenOnBlack`, `Linux`, `Solarized`,
`SolarizedLight`, `Tango`, `Ubuntu`, `BreezeModified`, `WhiteOnBlack`,
`BlackOnWhite`, `BlackOnLightYellow`. An unknown or empty name keeps the default.

```toml
[terminal]
colorScheme = "DarkPastels"
```

The terminal font is set with `[font] terminal` / `terminalSize` (see Fonts).

## Preview

`[preview]`, `[preview.extensions]`, and `[preview.markdown]` control how the
preview pane renders files.

`[preview] default` sets the mode for extensions that are not listed in
`[preview.extensions]`. `[preview.extensions]` overrides the mode per extension
(keys are extensions without the leading dot).

```toml
[preview]
default = "auto"

[preview.extensions]
md = "rendered"
json = "text"
log = "text"
zip = "none"
```

Preview modes:

- `auto` — built-in preview selection (keeps the current rendered/source
  preference).
- `rendered` — start in the rendered view when a rendered form is available
  (Markdown, HTML, CSV/TSV, JSON).
- `text` — start in the raw text/source view.
- `none` — disable content preview for that extension; file metadata still
  appears.

`[preview.markdown] externalImages` controls external (`https:`) images in
Markdown previews:

```toml
[preview.markdown]
externalImages = "button"
```

- `never` — never load external images inline (they become links).
- `button` — treated like `never` on the Linux port (no inline load); the first
  external image URL is still reachable via "open externally".
- `always` — keep the image inline.

Note: the Linux preview pane renders through `QTextBrowser`, not a web engine.
`always` keeps the inline image markup, but remote `https:` images may not be
fetched and displayed; `never` reliably blocks inline external images.

## User Commands

`[[commands]]` adds custom commands to the menu bar and file-list context menu.
A command is shown in the context menu only when the current selection matches
its `target` / `selection` / `extensions` / `requireGit` filters. Each command
runs in its own process whose working directory is the parent folder of the
first selected item (or the pane's current folder for `target = "current"` and
empty selections).

```toml
[[commands]]
name = "Count Lines"
run = "wc -l {paths}"
target = "file"
selection = "single"
extensions = ["txt", "md"]
terminal = true
shortcut = "ctrl+shift+l"

[[commands]]
name = "Git Pull"
run = "git pull --ff-only"
target = "current"
requireGit = true
terminal = true

[[commands]]
name = "Open Config Scripts"
run = "xdg-open {scripts}"
target = "current"
```

Supported fields:

- `name` — menu label. **Required.**
- `run` — command line or multi-line script body. **Required.** (`command` is
  accepted as a legacy alias.) A multi-line `run` is written to a temporary
  script file and executed through `shell`.
- `extensions` — array of matching file extensions, lowercase and without the
  dot (e.g. `["txt", "md"]`). Empty or `["*"]` matches all files.
- `target` — `file`, `folder`, `current`, or `any` (default `any`). `file` /
  `folder` require the selection to be entirely files / folders; `current`
  operates on the pane folder regardless of selection.
- `selection` — `single`, `multiple`, or `any` (default `any`). Ignored when
  `target = "current"`.
- `requireGit` — `true` to show the command only inside a Git work tree.
  Default `false`.
- `terminal` — `true` streams stdout/stderr live to the terminal pane's
  **Output** tab. `false` (default) captures output quietly into the Command
  Output dock as browsable history (revealed automatically only on failure).
- `shortcut` — optional keyboard shortcut. Conflicts with built-in shortcuts or
  other commands are reported as configuration warnings.
- `shell` — shell used to run the command. Defaults to `$SHELL`, then `/bin/sh`.

Supported tokens:

- `{path}` — first selected item, shell-quoted.
- `{paths}` — all selected items, shell-quoted and space-separated.
- `{dir}` — directory of the first selected item, shell-quoted.
- `{name}` — file name of the first selected item, shell-quoted.
- `{stem}` — file name without extension, shell-quoted.
- `{ext}` — extension of the first selected item, shell-quoted.
- `{cwd}` — current pane directory, shell-quoted.
- `{scripts}` — `~/.config/tfx/scripts`, shell-quoted.

Quoting guarantee: every token expands inside POSIX single quotes (embedded
single quotes are escaped as `'"'"'`), so file names containing spaces,
quotes, `$`, backticks, semicolons, or newlines are passed to the shell as
literal arguments and cannot inject additional commands. Anything you write
around the tokens in `command` is ordinary shell syntax and is your own
responsibility.

## Notes

`[openWith]` maps file extensions to launch programs shown in the file context
menu's Open With submenu. Keys are lowercase extensions without the dot; `*`
adds a fallback program for all files. The configured program is launched
directly with the selected file path as its only argument.
