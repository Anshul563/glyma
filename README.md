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
  fit-to-window, zoom reset, and middle-button drag panning.
- Layers panel with stack order, selection, visibility toggles, opacity, and
  add/delete/rename/reorder controls. Rename with double-click or F2.
- New documents with a transparent or solid background color, opening `.glyma`
  projects or importing PNG files, saving projects, Save As, and exporting
  rendered PNGs through native file dialogs.
- Unsaved-changes guard before New, Open, and window close with Save, Discard,
  and Cancel choices; modified documents show a `*` in the title bar.
- Brush and eraser tools with foreground color, size, opacity, and hardness
  controls in a dedicated tool options bar.
- Rectangle and ellipse selection tools with clear/invert actions, plus a
  bucket fill tool with tolerance and opacity controls that fills with the
  foreground color.
- Text tool with a Typography panel for font family, size, weight, italic,
  color, alignment, line/letter spacing, word wrap, and auto/fixed box sizing;
  text layers are marked with a `T` badge in the Layers panel and can be
  rasterized on demand.
- Image adjustments (exposure, brightness, contrast, saturation, grayscale,
  gaussian blur, and sharpen) in a live-preview dialog under the Image menu.
- Canvas crop, canvas/image resize, 90/180 and arbitrary rotation, and
  horizontal/vertical flipping under the Image menu, plus per-layer rotate,
  flip, scale, and move commands under the Layer menu.
- Undo/redo for brush/eraser strokes, selection changes, bucket fills,
  image adjustments, text add/edit/rasterize, and layer
  add/remove/reorder/rename, visibility, and opacity changes through the engine
  command history.
- Dark neutral Qt stylesheet with a neutral workspace around the canvas.

File and Edit actions use the existing engine serialization, PNG, and
command-history APIs. Rendering is performed by `Document::render()`; the canvas
receives an independent `QImage` copy.

## Painting Tools

The CPU brush engine lives in `include/glyma/core/brush.hpp`,
`include/glyma/core/brush_engine.hpp`, and `src/engine/brush_engine.cpp`. It has
no Qt dependencies; the canvas only supplies document-space pointer positions.

- `Brush` (`B`) paints with the foreground color and `Eraser` (`E`) reduces
  alpha on the selected layer. The active tool is highlighted in the tool bar.
- Brush settings: diameter 1-10000 px, foreground RGBA color, opacity 0-100%,
  and hardness 0-100%. The default is a 20 px, fully opaque, hard round brush.
- Dabs are interpolated between pointer samples, including subpixel positions,
  so fast movements do not leave gaps. The affected region is clipped to the
  active layer's buffer.
- Coverage uses a one-pixel linear antialiased edge: full coverage inside
  `radius * hardness - 0.5`, zero coverage beyond `radius + 0.5`, and linear
  interpolation between. A stroke keeps the maximum coverage per pixel, so
  overlapping dabs do not build up beyond the selected opacity.
- Painting uses straight-alpha source-over blending. Erasing preserves RGB for
  partial erasure and writes `{0, 0, 0, 0}` for fully erased pixels.
- A complete stroke is one command-history entry. The command stores only the
  sparse set of changed pixels with their before/after values, so undo and redo
  are exact without full-document snapshots.
- Left-button drag draws. Middle-button drag pans. Escape or a right-button
  press cancels the in-progress stroke.

## Selection and Bucket Fill

The selection model lives in `include/glyma/core/selection.hpp` and
`src/engine/selection.cpp`; the flood-fill engine lives in
`include/glyma/core/fill.hpp` and `src/engine/fill.cpp`. Both are Qt-independent.

- `Rectangle Select` (`R`) and `Ellipse Select` drag out a rectangular or
  elliptical selection; drags in any direction are normalized. The `Clear
  Selection` and `Invert Selection` toolbar buttons remove the selection and
  select its complement. The active tool is highlighted in the tool bar.
- Selections are geometry-only (a region type plus bounds) with an `inverted`
  flag, so they stay cheap to copy and compare; there is no full-document mask.
  Geometry is clipped to the document bounds.
- `Bucket Fill` flood-fills the selected layer with the foreground color,
  starting from the clicked pixel using 4-connectivity. Tolerance (0-255 per
  channel, alpha included) and opacity 0-100% are configurable in the tool
  options bar.
- Fills respect the active selection: only selected pixels are changed. Fill
  blending uses the same straight-alpha source-over model as the brush.
- A selection change and a bucket fill are each one command-history entry,
  storing the previous selection or the sparse set of changed pixels.

## Image Adjustments and Filters

The adjustment engine lives in `include/glyma/core/adjustments.hpp` and
`src/engine/adjustments.cpp`; the destructive-edit command lives in
`include/glyma/core/pixel_edit.hpp` and `src/engine/pixel_edit.cpp`. All are
Qt-independent. The dialog lives in
`include/glyma/app/adjustment_dialog.hpp` and `src/app/adjustment_dialog.cpp`.

- `Adjustments...` (`Ctrl+M`) in the Image menu opens a live-preview dialog for
  the selected visible layer. Controls are exposure (-4 to +4 EV), brightness,
  contrast, and saturation (-100% to +100%), grayscale, gaussian blur radius
  (0-50 px), and sharpen amount (0-200%). `Reset` restores the defaults;
  cancelling restores the original pixels; accepting commits one undoable edit.
- Adjustments apply in a fixed order: exposure, brightness, contrast,
  saturation, grayscale, blur, sharpen. The preview is recomputed from a
  snapshot on every change, so it never compounds.
- Operations are tonal approximations in the encoded (non-linear) pixel space
  and are not color-managed. Luminance uses the Rec. 601 weights
  (0.299/0.587/0.114). Alpha is preserved throughout; blur and sharpen operate
  on premultiplied alpha to avoid color fringing in transparent regions.
- Adjustments respect the active selection: only selected pixels are changed.
  A committed adjustment is one command-history entry that stores the smaller of
  a sparse pixel-diff or a before/after buffer snapshot, so undo and redo are
  exact without always paying for a full-document copy.

## Canvas and Layer Transforms

The transform engine lives in `include/glyma/core/transform.hpp` and
`src/engine/transform.cpp`. It is Qt-independent, and every whole-document or
single-layer operation has a matching undoable command.

- `Crop` (`C`) activates a draggable crop overlay with a rule-of-thirds grid and
  eight handles. Drag inside the canvas to draw a region, adjust it, then apply
  with the `Apply Crop` toolbar button, `Ctrl+Return`, or Enter. Escape or a
  right-click cancels the pending crop.
- `Canvas Size...` (`Ctrl+Alt+C`) changes the canvas without resampling layers,
  repositioning existing content according to a nine-way anchor.
- `Image Size...` (`Ctrl+Alt+I`) resamples the canvas and every layer, scaling
  offsets and sizes by the same factors (bilinear, premultiplied alpha).
- `Rotate Canvas 90° Clockwise/Counterclockwise`, `Rotate Canvas 180°`, and
  `Rotate Canvas by Angle...` rotate the whole document; non-quarter-turn
  rotations expand the canvas to the axis-aligned bounding box and clear the
  selection.
- `Flip Canvas Horizontal/Vertical` mirror the canvas content.
- Per-layer `Rotate`, `Flip`, `Scale...`, and `Move...` commands transform only
  the selected layer, keeping the layer centre fixed for rotations.
- Selection geometry follows document crops, resizes, rotations, and flips.
- Each transform is a single command-history entry; undo and redo restore the
  exact prior state (dimensions, layer geometry and pixels, and selection).

## Text Layers and Typography

Text layers keep their content editable instead of baking pixels immediately.
The Qt-free model lives in `include/glyma/core/text_layer.hpp` and
`src/engine/text_layer.cpp`; rendering lives in the application layer
(`include/glyma/app/text_renderer.hpp`), so the engine never depends on Qt.

- `TextLayerData` stores the string, a `TextStyle` (font family, pixel size,
  CSS-style weight 1-1000, italic, RGBA color, line spacing, letter spacing,
  horizontal/vertical alignment, word wrap), an auto-size flag, and a fixed box
  size. Payloads are validated against shared limits (`maxBoxDimension`,
  `maxPixelCount`).
- A text layer's `ImageBuffer` is a regenerable render cache. `TextRenderer`
  lays out and rasterizes the payload with Qt's font engine (multi-line, word
  wrap, alignment, spacing), measures auto-sized text, and refreshes every text
  cache after each document change.
- `AddTextLayerCommand`, `SetTextDataCommand`, and `RasterizeTextLayerCommand`
  make creation, typography edits, and rasterization undoable. Removing a text
  layer preserves its payload so undo restores editability.
- Transforms keep text editable where the glyphs remain upright: crop, canvas
  resize, 90° document rotation, and flips reposition the box (swapping fixed
  box dimensions on 90°); resampling scales the font and box. Transforms that
  cannot preserve upright glyphs (180°/arbitrary single-layer rotation, flips)
  bake the pixels through the command snapshot.

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

### Manifest schema (version 2)

```json
{
  "format": "glyma",
  "version": 2,
  "name": "Example",
  "width": 1920,
  "height": 1080,
  "background": { "type": "solid", "r": 0, "g": 0, "b": 0, "a": 255 },
  "layers": [
    {
      "id": 1,
      "name": "Layer 1",
      "type": "raster",
      "visible": true,
      "opacity": 1.0,
      "x": 0,
      "y": 0,
      "width": 1920,
      "height": 1080,
      "data": "layers/layer-1.rgba"
    },
    {
      "id": 2,
      "name": "Caption",
      "type": "text",
      "visible": true,
      "opacity": 1.0,
      "x": 64,
      "y": 480,
      "width": 512,
      "height": 96,
      "data": "layers/layer-2.rgba",
      "text": {
        "content": "Hello\nWorld",
        "fontFamily": "DejaVu Sans",
        "fontSize": 48.0,
        "fontWeight": 700,
        "italic": false,
        "color": { "r": 255, "g": 255, "b": 255, "a": 255 },
        "lineSpacing": 1.0,
        "letterSpacing": 0.0,
        "horizontalAlign": "center",
        "verticalAlign": "middle",
        "wordWrap": true,
        "autoSize": false,
        "boxWidth": 512,
        "boxHeight": 96
      }
    }
  ]
}
```

`background.type` is either `"transparent"` or `"solid"`. Layer order in the
`layers` array is bottom-to-top. Layer IDs are stable across save/load. Pixel
data files contain `width * height * 4` bytes in RGBA order. `type` defaults to
`"raster"` when omitted; text layers additionally carry a `text` object and
store their rendered cache in the pixel data file.

### Versioning policy

The manifest carries an explicit `format` identifier and integer `version`.
Version 1 stores raster layers only and is migrated transparently on load by
treating every layer as raster. Version 2 adds text layers. Future versions must
either stay backward compatible or provide explicit migrations; unsupported
versions are rejected rather than guessed.

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
- No GPU rendering.
- Adjustments are basic and destructive (pixels are baked on commit); there are
  no curves, levels, or per-channel controls.
- No textured brushes, pressure sensitivity, brush dynamics, or advanced brush
  presets.
- No layer blending modes, duplication, merging, grouping, or drag-and-drop
  reordering.
- No undo/redo memory budgeting beyond count limits.
- No color profile/ICC or wide-gamut management.
- Text layers use a single style per layer (no per-character rich text),
  text-on-path, or vertical writing.
- No autosave, recovery snapshots, or encryption yet.
