# Glyma

Open-source, native image editor built from scratch using C++20 on Ubuntu Linux.

## Dependencies (Ubuntu)

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
    libpng-dev libzip-dev nlohmann-json3-dev qt6-base-dev
```

The desktop application is tested with Qt 6.10.2 Widgets. A local development
install at `~/.local/qt6/usr` is detected automatically. For other non-standard
prefixes, configure with `-DCMAKE_PREFIX_PATH=<prefix>` (the same flag can point
at locally built libzip/nlohmann/json installations).

If the distribution packages for libzip or nlohmann/json are unavailable, they can
be built/installed into `~/.local` (CMake searches `$ENV{HOME}/.local` as a hint):

```bash
# libzip (static, no crypto)
curl -L https://libzip.org/download/libzip-1.11.4.tar.gz | tar xz
cmake -S libzip-1.11.4 -B libzip-1.11.4/build \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local" -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_COMMONCRYPTO=OFF -DENABLE_GNUTLS=OFF -DENABLE_MBEDTLS=OFF \
    -DENABLE_OPENSSL=OFF -DBUILD_TOOLS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_REGRESS=OFF
cmake --build libzip-1.11.4/build -j
cmake --install libzip-1.11.4/build

# nlohmann/json single header
mkdir -p "$HOME/.local/include/nlohmann"
curl -L https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp \
    -o "$HOME/.local/include/nlohmann/json.hpp"
```

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Launch the desktop application:

```bash
./build/glyma
```

The engine and tests can be built without Qt:

```bash
cmake -S . -B build-core -G Ninja -DGLYMA_BUILD_APP=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## Desktop Application

The Qt 6 Widgets shell lives under `include/glyma/app` and `src/app`. The engine
remains Qt-independent; the application layer owns the active `Document`, its
`CommandHistory`, and all UI state.

- Native main window with File, Edit, View, and Help menus, a toolbar, a status
  bar, and a dockable Layers panel.
- Checkerboard-backed canvas with centered display, mouse-wheel and menu zoom,
  fit-to-window, zoom reset, and drag panning.
- Layers panel with stack order, selection, visibility toggles, and opacity.
- New documents, opening `.glyma` projects or importing PNG files, saving
  projects, and exporting rendered PNGs through native file dialogs.
- Undo/redo for visibility and opacity changes through the engine command
  history.
- Dark neutral Qt stylesheet with a neutral workspace around the canvas.

File and Edit actions use the existing engine serialization, PNG, and
command-history APIs. Rendering is performed by `Document::render()`; the canvas
receives an independent `QImage` copy.

## PNG Import/Export

- Use `glyma::core::PngReader::read(path)` to decode supported PNG files (RGB, RGBA, grayscale, grayscale+alpha, palette with transparency) into RGBA8 `ImageBuffer`.
- Use `glyma::core::PngWriter::write(image, path)` to export an `ImageBuffer` as RGBA PNG.
- Use `glyma::core::DocumentIO::exportPng(document, path)` to render a document and export the result.

Supported formats: standard PNG via libpng. Alpha is normalized to RGBA8; transparent pixels remain transparent. Color profile management is not implemented in this milestone.

## Native `.glyma` Project Format

`ProjectWriter::save(document, path)` writes, and `ProjectReader::load(path)`
reads, a ZIP-based project container:

```text
example.glyma
├── manifest.json
└── layers/
    ├── layer-<id>.rgba   # raw straight-alpha RGBA8, row-major, no padding
    └── layer-<id>.rgba
```

```cpp
#include "glyma/io/project_reader.hpp"
#include "glyma/io/project_writer.hpp"

glyma::core::Document document("Example", 1920, 1080);
// ... add layers ...
glyma::core::ProjectWriter::save(document, "example.glyma");

auto reopened = glyma::core::ProjectReader::load("example.glyma");
```

### Manifest schema (version 1)

```json
{
  "format": "glyma",
  "version": 1,
  "name": "Example",
  "width": 1920,
  "height": 1080,
  "background": { "type": "solid", "r": 0, "g": 0, "b": 0, "a": 255 },
  "layers": [
    {
      "id": 1,
      "name": "Layer 1",
      "visible": true,
      "opacity": 1.0,
      "x": 0,
      "y": 0,
      "width": 1920,
      "height": 1080,
      "data": "layers/layer-1.rgba"
    }
  ]
}
```

`background.type` is either `"transparent"` or `"solid"`. Layer order in the
`layers` array is bottom-to-top. Layer IDs are stable across save/load. Pixel
data files contain `width * height * 4` bytes in RGBA order.

### Versioning policy

The manifest carries an explicit `format` identifier and integer `version`.
Version 1 is the only supported version. Future versions must either stay
backward compatible or provide explicit migrations; unsupported versions are
rejected rather than guessed.

### Security limits

- Canvas: up to 64 megapixels (`Document::maxPixelCount`).
- Layers: up to 1024 (`ProjectReader/ProjectWriter::maxLayerCount`).
- Uncompressed archive entry: up to 512 MiB
  (`ProjectReader::maxArchiveUncompressed`).
- Layer data length must exactly match `width * height * 4`.
- Archive paths must be relative, without `..`, and are never extracted to the
  filesystem.
- Duplicate layer IDs, invalid dimensions/opacity, malformed manifests, missing
  entries, and unsupported versions are rejected.
- Saves are written to a temporary file in the destination directory and
  renamed into place, so a failed save does not corrupt an existing project.

Undo/redo history is session-only and is not serialized.

## Known limitations

- No JPEG/WebP/SVG/PSD support.
- No painting tools, brush strokes, selections, transformations, or GPU rendering.
- No layer creation, deletion, reordering, renaming, or blending modes in the UI.
- Undo/redo in the UI currently covers layer visibility and opacity changes.
- No undo/redo memory budgeting beyond count limits.
- No color profile/ICC or wide-gamut management.
- No autosave, recovery snapshots, encryption, or format migration yet.
