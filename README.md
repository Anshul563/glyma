# Glyma

Open-source, native image editor built from scratch using C++20 on Ubuntu Linux.

## Dependencies (Ubuntu)

```bash
sudo apt install build-essential cmake ninja-build pkg-config libpng-dev
```

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## PNG Import/Export

- Use `glyma::core::PngReader::read(path)` to decode supported PNG files (RGB, RGBA, grayscale, grayscale+alpha, palette with transparency) into RGBA8 `ImageBuffer`.
- Use `glyma::core::PngWriter::write(image, path)` to export an `ImageBuffer` as RGBA PNG.
- Use `glyma::core::DocumentIO::exportPng(document, path)` to render a document and export the result.

Supported formats: standard PNG via libpng. Alpha is normalized to RGBA8; transparent pixels remain transparent. Color profile management is not implemented in this milestone.

## Known limitations

- No JPEG/WebP/SVG/PSD support.
- No GUI/file dialogs.
- No undo/redo memory budgeting beyond count limits.
- No color profile/ICC or wide-gamut management.
