# Packaging

The Linux Qt build installs a freedesktop desktop entry, scalable SVG icon, and
project documentation in addition to the `tfx` executable.

```sh
cmake -S linux-qt -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix /usr/local
```

For a staged install:

```sh
cmake --install build --prefix /usr --destdir /tmp/tfx-package
```

Installed files include:

- `bin/tfx`
- `share/applications/tfx.desktop`
- `share/icons/hicolor/scalable/apps/tfx.svg`
- `share/doc/tfx-for-linux/`

The build also provides an `uninstall` target after installation:

```sh
cmake --build build --target uninstall
```
