#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "glyma/core/command.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"

namespace glyma::core {

// Nine-way canvas anchor used by Canvas Size. The anchor describes where the
// existing content sits inside the new canvas.
enum class Anchor : std::uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};

// Horizontal and vertical anchor factors: 0.0 = start, 0.5 = centre,
// 1.0 = end. Exposed for callers that need to compute offsets consistently.
[[nodiscard]] double anchorFactorX(Anchor anchor) noexcept;
[[nodiscard]] double anchorFactorY(Anchor anchor) noexcept;

// A rectangular crop region in document pixel coordinates. x/y may be
// negative or extend past the canvas when supplied by a drag; the document
// operations clamp the rectangle to the current canvas.
struct CropRect {
    std::int32_t x{0};
    std::int32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] bool isEmpty() const noexcept {
        return width == 0 || height == 0;
    }

    [[nodiscard]] std::int64_t right() const noexcept {
        return static_cast<std::int64_t>(x) + width;
    }

    [[nodiscard]] std::int64_t bottom() const noexcept {
        return static_cast<std::int64_t>(y) + height;
    }
};

// True when a canvas can be allocated: non-zero and within
// Document::maxPixelCount.
[[nodiscard]] bool isValidCanvasSize(
    std::uint32_t width,
    std::uint32_t height
) noexcept;

// Intersect `rect` with [0, width) x [0, height). Returns an empty rectangle
// when there is no overlap.
[[nodiscard]] CropRect clipCropRect(
    const CropRect& rect,
    std::uint32_t width,
    std::uint32_t height
) noexcept;

// Build a pixel-aligned crop rectangle from two continuous document points
// (for example the start and end of a drag). Points may be in any order and
// may lie outside the canvas; the result is clamped.
[[nodiscard]] CropRect cropRectFromPoints(
    double x0,
    double y0,
    double x1,
    double y1,
    std::uint32_t documentWidth,
    std::uint32_t documentHeight
) noexcept;

// ---------------------------------------------------------------------------
// Image-buffer primitives (Qt-independent and allocation validated)
// ---------------------------------------------------------------------------

// Crop a region out of a buffer. (x, y) is relative to the source buffer and
// may be negative; destination pixels with no source coverage are
// transparent. Throws std::invalid_argument for zero output dimensions.
[[nodiscard]] ImageBuffer cropImage(
    const ImageBuffer& source,
    std::int32_t x,
    std::int32_t y,
    std::uint32_t width,
    std::uint32_t height
);

// Bilinear resampling with premultiplied-alpha interpolation. The result is
// returned as straight-alpha RGBA8. Throws std::invalid_argument for invalid
// dimensions.
[[nodiscard]] ImageBuffer scaleImage(
    const ImageBuffer& source,
    std::uint32_t width,
    std::uint32_t height
);

// Exact 90/180 degree rotations and mirrors. No interpolation is performed.
[[nodiscard]] ImageBuffer rotateImage90(
    const ImageBuffer& source,
    bool clockwise
);
[[nodiscard]] ImageBuffer rotateImage180(const ImageBuffer& source);
[[nodiscard]] ImageBuffer flipImage(
    const ImageBuffer& source,
    bool horizontal
);

// Free rotation by an arbitrary angle in degrees (positive is clockwise in
// screen coordinates). The output is expanded to the axis-aligned bounding
// box of the rotated source. Interpolation uses premultiplied alpha.
[[nodiscard]] ImageBuffer rotateImage(
    const ImageBuffer& source,
    double degrees
);

// ---------------------------------------------------------------------------
// Whole-document operations. These mutate the document immediately; use the
// command classes below or AppController to make the change undoable.
// ---------------------------------------------------------------------------

// Crop the document canvas to `rect`, clipping every layer's pixels and
// translating its offset relative to the new canvas origin. Layer metadata
// (name, visibility, opacity, order and IDs) is preserved. The selection is
// translated and re-clipped; an invalid/empty selection is cleared. Throws
// std::invalid_argument when the clamped crop is empty.
void cropDocument(Document& document, const CropRect& rect);

// Change canvas dimensions without resampling any layer pixels. Existing
// content is moved according to `anchor`; expanding adds transparent canvas
// area and shrinking clips content naturally. Selection geometry moves with
// the content.
void resizeCanvas(
    Document& document,
    std::uint32_t width,
    std::uint32_t height,
    Anchor anchor = Anchor::Center
);

// Resample the whole document (canvas and every layer) to the requested pixel
// dimensions. Layer offsets and sizes are scaled by the same factors, and the
// selection geometry follows the resampling.
void resizeDocumentImage(
    Document& document,
    std::uint32_t width,
    std::uint32_t height
);

// Exact canvas/layer rotation by quarter turns. The canvas dimensions are
// swapped for 90 degree rotations and layer offsets are transformed around
// the canvas origin.
void rotateDocument90(Document& document, bool clockwise);
void rotateDocument180(Document& document);

// Arbitrary canvas rotation. When `expandCanvas` is true the canvas grows to
// the axis-aligned bounding box of the rotated source. Non-quarter-turn
// rotations clear the selection because the ellipse/rectangle representation
// cannot represent rotated geometry without an additional rotation field.
void rotateDocument(
    Document& document,
    double degrees,
    bool expandCanvas = true
);

// Mirror the entire document (canvas content and selection) in place.
void flipDocument(Document& document, bool horizontal);

// ---------------------------------------------------------------------------
// Single-layer operations. Layer buffers are changed, but layers never move
// relative to the document unless explicitly requested.
// ---------------------------------------------------------------------------

enum class LayerTransform : std::uint8_t {
    Rotate90Clockwise,
    Rotate90CounterClockwise,
    Rotate180,
    FlipHorizontal,
    FlipVertical,
};

// Apply an exact transform to one layer's own pixels. Rotation keeps the
// layer centre fixed in document coordinates.
void transformLayer(
    Document& document,
    LayerId layerId,
    LayerTransform transform
);

// Rotate a layer by an arbitrary angle around its centre. The layer's buffer
// grows to fit the rotated content and its offset is adjusted to preserve the
// centre.
void rotateLayer(Document& document, LayerId layerId, double degrees);

// Resample one layer to explicit pixel dimensions around its centre.
void scaleLayer(
    Document& document,
    LayerId layerId,
    std::uint32_t width,
    std::uint32_t height
);

// Move a layer by a document-space delta.
void translateLayer(
    Document& document,
    LayerId layerId,
    std::int32_t deltaX,
    std::int32_t deltaY
);

// ---------------------------------------------------------------------------
// Undoable commands
// ---------------------------------------------------------------------------

// Full serializable-in-memory state used by document-wide transform commands.
// Layer copies are deep copies (including buffers) and preserve IDs.
struct DocumentSnapshot {
    std::uint32_t width{0};
    std::uint32_t height{0};
    bool solidBackground{false};
    Pixel backgroundColor{0, 0, 0, 0};
    Selection selection;
    std::vector<Layer> layers;
};

[[nodiscard]] DocumentSnapshot captureDocumentSnapshot(
    const Document& document
);
void restoreDocumentSnapshot(
    Document& document,
    const DocumentSnapshot& snapshot
);

class CropCommand : public Command {
public:
    explicit CropCommand(CropRect rect);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    CropRect rect_;
    bool captured_{false};
    DocumentSnapshot before_;
};

class CanvasResizeCommand : public Command {
public:
    CanvasResizeCommand(
        std::uint32_t width,
        std::uint32_t height,
        Anchor anchor
    );

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    Anchor anchor_;
    bool captured_{false};
    DocumentSnapshot before_;
};

class ImageResizeCommand : public Command {
public:
    ImageResizeCommand(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    bool captured_{false};
    DocumentSnapshot before_;
};

enum class DocumentRotation : std::uint8_t {
    Clockwise90,
    CounterClockwise90,
    Rotate180,
};

class RotateDocumentCommand : public Command {
public:
    explicit RotateDocumentCommand(DocumentRotation rotation);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    DocumentRotation rotation_;
    bool captured_{false};
    DocumentSnapshot before_;
};

class ArbitraryRotateDocumentCommand : public Command {
public:
    explicit ArbitraryRotateDocumentCommand(double degrees);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    double degrees_;
    bool captured_{false};
    DocumentSnapshot before_;
};

class FlipDocumentCommand : public Command {
public:
    explicit FlipDocumentCommand(bool horizontal);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    bool horizontal_;
    bool captured_{false};
    DocumentSnapshot before_;
};

class TransformLayerCommand : public Command {
public:
    TransformLayerCommand(LayerId layerId, LayerTransform transform);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId layerId_;
    LayerTransform transform_;
    bool captured_{false};
    DocumentSnapshot before_;
};

class ArbitraryRotateLayerCommand : public Command {
public:
    ArbitraryRotateLayerCommand(LayerId layerId, double degrees);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId layerId_;
    double degrees_;
    bool captured_{false};
    DocumentSnapshot before_;
};

class ScaleLayerCommand : public Command {
public:
    ScaleLayerCommand(
        LayerId layerId,
        std::uint32_t width,
        std::uint32_t height
    );

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId layerId_;
    std::uint32_t width_;
    std::uint32_t height_;
    bool captured_{false};
    DocumentSnapshot before_;
};

class TranslateLayerCommand : public Command {
public:
    TranslateLayerCommand(
        LayerId layerId,
        std::int32_t deltaX,
        std::int32_t deltaY
    );

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId layerId_;
    std::int32_t deltaX_;
    std::int32_t deltaY_;
    bool captured_{false};
    DocumentSnapshot before_;
};

} // namespace glyma::core
