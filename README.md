# MPQ Viewer Editor for Linux by X08

https://github.com/X08/MPQ-Viewer-Editor-for-Linux

A native Linux GUI for browsing and editing Blizzard MPQ archives (Qt 6 + [StormLib](https://github.com/ladislav-zezula/StormLib), MIT).

**Features**
- Open MPQ v1–v4 (Diablo II, StarCraft, Warcraft III, WoW 1.x–4.x, StarCraft II …), read/write or read-only
- Folder tree with size / packed size / ratio / flags, sortable, with a path filter
- Preview: text, hex, PNG/JPG/BMP/GIF, and **BLP textures** (BLP1 paletted, BLP2 paletted/DXT1/3/5/raw)
- Extract selection or everything (paths preserved, `..` and drive prefixes are stripped)
- Add files / folders (menu or drag & drop onto the window), delete, rename/move (also whole folders)
- Create new archives (v1–v4), compact, enlarge max file count, archive info
- External listfile support (names for files that are not in the archive's own `(listfile)`; shown greyed as `File0000xxxx.ext` until resolved)
- Compression for added files: none / zlib / bzip2 / LZMA

**Build**
    sudo apt install cmake g++ qt6-base-dev
    ./packaging/build-appimage.sh          # -> appbuild/MPQ_Viewer-0.1.0-x86_64.AppImage
or just `cmake -S . -B build && cmake --build build` for a plain binary.

**Tests** (`-DBUILD_TESTING=ON`): `test-core` (archive round-trips on v1–v4, BLP decoder), `test-gui` (offscreen screenshots).

**Known limits**
- BLP1 files with embedded JPEG content (mostly Warcraft III) show as hex, not as an image.
- No signature verification and no strong-signature creation.
- Edits are flushed to disk after every operation; killing the app *during* a write can still damage an archive, so work on a copy of anything you care about.
