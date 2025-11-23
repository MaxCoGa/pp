# Paran package format (paranpackage)

This document describes the expected layout and manifest format for a paranpackage — the simple tarball package format used by the `pp` package manager.

Overview
- A paranpackage is a tarball (gzip is fine) containing a top-level `MANIFEST` file and package files/scripts referenced from that manifest.
- `pp` extracts the tarball into `pp_download/<pkgname>/`, reads the `MANIFEST`, saves the `MANIFEST` and any uninstall script to `pp_info/<pkgname>/`, and executes the `install` script from the extracted tree.

Archive layout (recommended)
- The package archive should contain the package files with `MANIFEST` located at the archive root (top-level). Example tree inside tarball:

  MANIFEST
  install.sh
  uninstall.sh
  files/
    ... package payload files ...

- When packaging locally, create the archive from the package directory (from its parent):

```bash
# from the parent of the package directory
tar -czf mypkg-1.0.0.tar.gz -C mypkg_dir .
```

MANIFEST format
- Plain text `key: value` lines. Keys are case-sensitive as described here (simple parser in `pp` scans for specific keys).
- Common keys recognized by `pp`:
  - `name:` — package name (required)
  - `version:` — version string (required)
  - `description:` — short description (optional)
  - `dependencies:` — a space-separated list of dependencies (note: `pp` currently does not enforce dependencies)
  - `install:` — relative path to the install script inside the package (e.g. `install.sh`) (optional but recommended)
  - `uninstall:` — relative path to the uninstall script inside the package (e.g. `uninstall.sh`) (optional but recommended)
  - `helper:` — space-separated list of helper files inside the package that should be preserved in `pp_info/<pkg>/` and kept available to the package manager during removal or upgrades (e.g. `helper: uninstall-gcc-from-dir.sh uninstall.sh`). Helpers are copied into `pp_info/<pkg>/` and made executable where applicable.

Example MANIFEST
```
name: helloworld
version: 1.2.3
description: helloworld package
dependencies: bash
install: install.sh
uninstall: uninstall.sh
helper: uninstall-gcc-from-dir.sh uninstall.sh
```

Notes about fields
- `dependencies:`; `pp` currently doesn't parse or resolve dependencies automatically, this is informational for now.
- `install:` and `uninstall:` — these should be executable shell scripts in the package. `pp` will try to make them executable and then run them.
- `pp` saves the `MANIFEST` and the uninstall script into `pp_info/<pkgname>/` to support subsequent removal and upgrades.

Helper files
- `helper:` — If present, `pp` will copy each filename listed after `helper:` from the extracted package into `pp_info/<pkg>/` during install and will attempt to remove them during uninstall. This is useful for packages that ship additional helper scripts or support files the uninstall step depends on (for example, a repo-level helper that removes a staged prefix).

Scaffolding packages with `new-paranpackage.sh`
- There is a helper script at `scripts/new-paranpackage.sh` that scaffolds a new package directory template. It creates a `MANIFEST`, `install.sh`, `uninstall.sh`, and a `files/` layout. After scaffolding, add any helper files you need to the package directory and list them in `MANIFEST` using the `helper:` key so `pp` will preserve them in `pp_info` at install-time.

Example `helper:` usage:
```
name: gcc
version: 14.1.0
install: install.sh
uninstall: uninstall.sh
helper: uninstall-gcc-from-dir.sh uninstall.sh
```

Repository / index
- `pp` expects a repository list in `pkg_list` (for now this can be a local file). Each line in `pkg_list` follows the format used by `pp` and `pp_pkg_list`:

  name version sha256 url status

- `pp` stores a local package list in `pp_pkg_list` with the same format. The SHA256 field is intended for checksum verification (TODO in `pp` implementation).

How to create and add a package to the local repo list
1. Build the tarball from your package directory (example):

```bash
cd /path/to/package_parent
tar -czf helloworld-1.2.3.tar.gz -C helloworld .
```

2. Compute SHA256:

```bash
sha256sum helloworld-1.2.3.tar.gz | awk '{print $1}'
```

3. Add an entry to `pkg_list` (or `pp_pkg_list` for local testing). Example line:

```
helloworld 1.2.3 a3b1c... /full/path/to/helloworld-1.2.3.tar.gz 0
```

- The `url` may be a local path (as above) or an `http(s)://` URL. `pp` will curl remote URLs or copy local files.
- The final numeric `status` field is one of the flags used internally by `pp` (0 = update, 1 = security, 2 = mandatory, etc.).

Packaging best practices and caveats (TODO)
- The `MANIFEST` should be small and human-readable. `pp` currently reads whole MANIFEST into memory.
- Use `dependencies:` even if tools don't enforce them yet.
- Provide both `install` and `uninstall` scripts where possible. Uninstall scripts help `pp` cleanly remove installed files.
- Keep install/uninstall scripts idempotent where possible to simplify upgrades.
- `pp` does not currently verify SHA256; adding a checksum verification step is recommended before publishing packages.

Example quick workflow (authoring a package)
1. Create package dir:

```
mypkg/
  MANIFEST
  install.sh
  uninstall.sh
  files/...
```

2. Create tarball:
```
tar -czf mypkg-1.0.0.tar.gz -C mypkg .
```

3. Compute sha256 and add to `pkg_list` or use `pp a` to add manually:
```
sha256=$(sha256sum mypkg-1.0.0.tar.gz | awk '{print $1}')
pp a mypkg 1.0.0 /full/path/to/mypkg-1.0.0.tar.gz $sha256
```

4. Install for testing:
```
pp i mypkg
```
---
TODO if automated: Generated by maintainer automation on behalf of the `pp` packaging workflow.
