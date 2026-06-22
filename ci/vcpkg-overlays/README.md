# vcpkg overlay ports

This directory holds [vcpkg overlay ports](https://learn.microsoft.com/vcpkg/concepts/overlay-ports)
used to build FastRules in environments where the default upstream source
hosts are unreachable.

## `lua`

vcpkg's stock `lua` port downloads the release tarball from `https://www.lua.org`.
Some CI / sandboxed environments only allow a small allowlist of package
registries (GitHub, PyPI, npm, Maven Central) and block `www.lua.org`
(the egress proxy returns `host_not_allowed`).

This overlay builds the **same Lua version** from the official GitHub source
mirror (`github.com/lua/lua`) instead. Because the git tree keeps the C/H files
at the repo root, lacks `lua.hpp`, and has no `doc/readme.html`, the portfile
reshapes the tree into the `src/` layout vcpkg's `CMakeLists.txt` expects,
synthesizes `lua.hpp`, and writes the MIT `copyright` file directly.

The resulting package is byte-for-byte equivalent in behavior to the stock port:
same `unofficial-lua` CONFIG package and `unofficial::lua::lua` target.

### Usage

Point `VCPKG_OVERLAY_PORTS` at this directory when configuring:

```bash
export VCPKG_OVERLAY_PORTS="$(pwd)/ci/vcpkg-overlays"
cmake -B build -S . -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_MANIFEST_FEATURES="tests;json;xml" \
  -DFASTRULES_BUILD_TESTS=ON \
  -DFASTRULES_BUILD_EXAMPLES=ON \
  -DFASTRULES_BUILD_EXTENSIONS=ON
```

You only need this overlay if `www.lua.org` is blocked in your environment.
On a normal network the stock vcpkg `lua` port works unchanged, so leave
`VCPKG_OVERLAY_PORTS` unset.

> **Note:** The DB extension (SOCI → sqlite3) additionally needs the SQLite
> amalgamation from `www.sqlite.org`, which the same allowlists typically block.
> When SOCI is unavailable the DB extension skips itself with a warning, so a
> `tests;json;xml` build (DB off) is the maximal feature set in those
> environments — matching the project's Ubuntu CI.
