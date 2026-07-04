# tfx for Linux Configuration

Linux | Based on the tfx for Windows configuration model

tfx for Linux stores user-editable configuration under:

```text
~/.config/tfx/
```

The main user-editable configuration file is `config.toml`. tfx creates it on startup when it does not already exist. Existing files are not overwritten.

Session state such as window placement, last-opened paths, pinned folders, column layout, splitter sizes, and view mode remains app-owned state. Use `config.toml` for hand-written preferences.

## Current Scope

`config.toml` supports these sections:

- Top-level `version = 1`
- Top-level `theme = "dark"` or `theme = "light"`
- `[font]`
- `[colors]`
- `[opacity]`
- `[startup]`
- `[naming]`
- `[shortcuts]`
- `[terminal]`
- `[openWith]`
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
# background = 0.40     # window background opacity (0.0 = transparent, 1.0 = opaque)
# inactivePane = 0.5    # opacity of the inactive file pane
# disabledItem = 0.45   # opacity of disabled items

# [colors]
# fileListBackground = "#151A1E"
# fileForeground = "#D9E1E8"
# directoryForeground = "#E5EDF3"
# paneBorderKeyboardTarget = "#36E67A"

# [terminal]
# app = "x-terminal-emulator"
# arguments = "--working-directory={path}"

# [openWith]
# md = "code"
# * = "xdg-open"

# [[commands]]
# name = "Open in VS Code"
# command = "code {paths}"
# shortcut = "ctrl+shift+o"
# requiresSelection = true
# showOutput = false
```

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

## Startup

`[startup] layout` accepts `single`, `split`, or `restore`.

`[startup] preview` accepts `show`, `hide`, or `restore`.

`[startup] terminal` accepts `show`, `hide`, or `restore`.

`[startup] folderTree` accepts `show`, `hide`, or `restore`.

`rightFolder` and `rightFolders` accept absolute paths or `~`-expanded paths. When `rightFolders` is used, the first valid folder is used for the right pane.

`geometry` accepts `WIDTHxHEIGHT` or `WIDTHxHEIGHT+X+Y`. The command-line
option `-g` / `--geometry` uses the same format and takes precedence over
`config.toml`.

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

## Opacity

`[opacity]` controls window and pane transparency. Each value is a decimal between `0.0` (fully transparent) and `1.0` (fully opaque). Omitted keys default to `1.0`.

- `background` — overall window background opacity. Values below `1.0` enable a translucent window surface, so the desktop shows through behind the panes (requires a compositing window manager).
- `inactivePane` — opacity applied to the file pane that is not currently active.
- `disabledItem` — opacity applied to disabled controls and menu items.

```toml
[opacity]
background = 0.40
inactivePane = 0.5
disabledItem = 0.45
```

## Colors

Set `theme = "dark"` or `theme = "light"` at the top level to choose the built-in
palette. `[colors]` values override the selected theme.

The Linux port accepts the same semantic color names used by tfx for Windows where practical:

- `fileListBackground`
- `headerBackground`
- `inputBackground`
- `fileListRowHovered`
- `fileListRowSelected`
- `fileListRowSelectedForeground`
- `fileForeground`
- `directoryForeground`
- `secondaryForeground`
- `headerForeground`
- `paneBorderInactive`
- `paneBorderActive`
- `paneBorderKeyboardTarget`
- `splitHandleActive`
- `folderTreeBackground`
- `disabledForeground`
- `scrollbarThumb`
- `scrollbarThumbHovered`
- `scrollbarThumbDragging`

Values must be quoted `#RRGGBB` strings.

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

## User Commands

`[[commands]]` adds custom commands to the menu bar and file-list context menu.
Commands run through `/bin/sh -c` with the current pane directory as the default
working directory. Runs are recorded in the Command Output dock; commands with
`showOutput = true` and failed commands automatically reveal that dock.

```toml
[[commands]]
name = "Count Lines"
command = "wc -l {paths}"
shortcut = "ctrl+shift+l"
requiresSelection = true
showOutput = true

[[commands]]
name = "Open Config Scripts"
command = "xdg-open {scripts}"
requiresSelection = false
showOutput = false
```

Supported fields:

- `name` — menu label.
- `command` — shell command to run.
- `shortcut` — optional keyboard shortcut. Conflicts with built-in shortcuts or
  other commands are reported as configuration warnings.
- `workingDirectory` — optional directory, defaulting to `{cwd}`.
- `requiresSelection` — `true` by default. Set `false` for folder-level commands.
- `showOutput` — `true` by default. Failures always show output.

Supported tokens:

- `{path}` — first selected item, shell-quoted.
- `{paths}` — all selected items, shell-quoted and space-separated.
- `{dir}` — directory of the first selected item, shell-quoted.
- `{name}` — file name of the first selected item, shell-quoted.
- `{stem}` — file name without extension, shell-quoted.
- `{ext}` — extension of the first selected item, shell-quoted.
- `{cwd}` — current pane directory, shell-quoted.
- `{scripts}` — `~/.config/tfx/scripts`, shell-quoted.

## Notes

`[openWith]` maps file extensions to launch programs shown in the file context
menu's Open With submenu. Keys are lowercase extensions without the dot; `*`
adds a fallback program for all files. The configured program is launched
directly with the selected file path as its only argument.
