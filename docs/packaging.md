# Packaging

## Distributable packages

`scripts/build_package.sh` produces installable packages under `dist/`:

```sh
scripts/build_package.sh            # DEB + RPM + tar.gz (auto-detected tools)
scripts/build_package.sh --deb      # only the Debian package
scripts/build_package.sh --clean    # wipe build-package/ and dist/ first
scripts/build_package.sh --skip-tests
```

The script configures a Release build in `build-package/`, runs the test
suite, validates the desktop entry when `desktop-file-validate` is available,
and invokes CPack. Each package ships with a `.sha256` checksum file.
Generators are selected automatically: `DEB` when `dpkg-deb` exists, `RPM`
when `rpmbuild` exists, and a `tar.gz` archive always.

The Debian package resolves shared-library dependencies from the built binary
(`dpkg-shlibdeps`) and recommends the optional runtime tools `zip`, `unzip`,
and `poppler-utils` (PDF preview). Install and remove with:

```sh
sudo apt install ./dist/tfx_<version>_amd64.deb
sudo apt remove tfx
```

## Manual install from source

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

The staged install should contain the same paths that would be installed under
the selected prefix, rooted below the `--destdir` directory.

Installed files include:

- `bin/tfx`
- `share/applications/tfx.desktop`
- `share/icons/hicolor/scalable/apps/tfx.svg`
- `share/doc/tfx-for-linux/`

Release verification:

```sh
ctest --test-dir build --output-on-failure
cmake --install build --prefix /usr --destdir /tmp/tfx-package
```

If `desktop-file-validate` is available, the desktop entry can be checked with:

```sh
desktop-file-validate linux-qt/packaging/tfx.desktop
```

The build also provides an `uninstall` target after installation:

```sh
cmake --build build --target uninstall
```
