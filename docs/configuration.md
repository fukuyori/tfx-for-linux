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
- `[font]`
- `[colors]`
- `[startup]`
- `[shortcuts]`
- `[terminal]`
- `[openWith]`

The loader intentionally accepts a small TOML subset:

- Tables with `[section]` headers
- Assignments with `key = value`
- Double-quoted strings
- Arrays of double-quoted strings for `rightFolders`
- Numeric font sizes
- Quoted `#RRGGBB` colors
- `#` comments outside quoted strings

Unsupported sections and unsupported keys are ignored. Invalid values fall back to built-in defaults and surface a status warning instead of crashing the app.

## Default File

New installations create a Linux-native file like this:

```toml
version = 1

[font]
ui = "system"
mono = "monospace"
size = 12

[shortcuts]
reload = "f5"
openTerminal = "ctrl+shift+t"
togglePreview = "ctrl+shift+p"
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
# rightFolder = "~/Downloads"
# rightFolders = ["~/Downloads", "~/Documents"]

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
```

## Startup

`[startup] layout` accepts `single`, `split`, or `restore`.

`[startup] preview` accepts `show`, `hide`, or `restore`.

`rightFolder` and `rightFolders` accept absolute paths or `~`-expanded paths. When `rightFolders` is used, the first valid folder is used for the right pane.

## Colors

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

## Notes

`[terminal]` and `[openWith]` are parsed for compatibility with the Windows configuration shape. Their detailed Linux behavior is still being wired into the file operations and terminal launcher.
