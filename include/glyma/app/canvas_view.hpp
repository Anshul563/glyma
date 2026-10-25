#pragma once

#include <optional>
#include <vector>

#include <QGraphicsView>
#include <QPointF>
#include <QSize>

#include "glyma/core/selection.hpp"
#include "glyma/core/shape_layer.hpp"

class QImage;
class QRect;
class QGraphicsScene;
class QKeyEvent;
class QMouseEvent;
class QShowEvent;
class QResizeEvent;
class QWheelEvent;

namespace glyma::app {

class CanvasImageItem;
class CheckerboardItem;
class CropItem;
class SelectionItem;
class ShapePreviewItem;

// Graphics-view based canvas with checkerboard transparency, zoom, panning,
// and fit-to-window support. The view displays a snapshot image; it never
// retains or mutates engine document storage.
class CanvasView : public QGraphicsView {
    Q_OBJECT

public:
    enum class Tool {
        Brush,
        SelectRectangle,
        SelectEllipse,
        BucketFill,
        Crop,
        Text,
        ShapeRectangle,
        ShapeRoundedRectangle,
        ShapeEllipse,
        ShapeLine,
        ShapeArrow,
        ShapePolygon,
    };

    // True for the vector shape tools (box/line/arrow drag or polygon clicks).
    [[nodiscard]] static bool isShapeTool(Tool tool) noexcept;
    // The primitive created by a shape tool.
    [[nodiscard]] static core::ShapeType shapeTypeForTool(Tool tool) noexcept;

    static constexpr double minZoom = 0.05;
    static constexpr double maxZoom = 32.0;
    static constexpr int fitMargin = 24;

    explicit CanvasView(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    // Repaint only `region` (document pixel coordinates) of the displayed
    // snapshot with `subImage` (same size as the region). Used for incremental
    // edits so the whole canvas is not re-rendered or re-uploaded per event.
    void patchImage(const QRect& region, const QImage& subImage);
    [[nodiscard]] QSize imageSize() const noexcept;

    [[nodiscard]] double zoom() const noexcept;
    void setZoom(double zoom);
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    void resetZoom();

    [[nodiscard]] bool isFitMode() const noexcept;

    [[nodiscard]] static double clampZoom(double zoom) noexcept;
    [[nodiscard]] static double fitZoom(
        const QSize& imageSize,
        const QSize& viewportSize,
        int margin = fitMargin
    ) noexcept;

    // Convert a viewport position to continuous document/scene coordinates,
    // preserving subpixel precision through the current zoom and pan.
    [[nodiscard]] QPointF documentPosition(const QPointF& viewportPosition) const;

    [[nodiscard]] Tool activeTool() const noexcept;
    void setActiveTool(Tool tool);

    // Style template for shapes created by the shape tools. Geometry is taken
    // from the drag/click; fill, stroke, corner radius, and arrowheads come
    // from this template.
    void setShapeTemplate(const core::ShapeLayerData& shape);

    // Set the active pixel selection for overlay rendering. The selection is
    // stored in document coordinates (scene coordinates) and drawn separately
    // from the image pixels.
    void setSelection(const core::Selection& selection);

    // Crop overlay in document coordinates. A pending crop is retained until
    // clearCrop() is called (Apply/Cancel are owned by the window). Setting an
    // empty rectangle clears the overlay.
    void setCropRect(const QRectF& rect);
    void clearCrop();
    [[nodiscard]] QRectF cropRect() const noexcept;
    [[nodiscard]] bool hasCrop() const noexcept;

signals:
    // Emitted after the active tool actually changes. Owners use it to close
    // tool-scoped editing sessions (for example a pending text edit).
    void toolChanged();
    void strokeStarted(const QPointF& documentPosition);
    void strokeMoved(const QPointF& documentPosition);
    void strokeEnded();
    void strokeCancelled();
    // Emitted while a selection drag is in progress, in document coordinates.
    void selectionDragStarted(const QPointF& documentPosition);
    void selectionDragMoved(const QPointF& documentPosition);
    void selectionDragCompleted(const QPointF& start, const QPointF& end);
    void selectionDragCancelled();
    // Emitted when the bucket fill tool is clicked at a document position.
    void bucketFillRequested(const QPointF& documentPosition);
    // Emitted when the text tool is clicked at a document position; the window
    // creates a text layer there and focuses the typography controls.
    void textPlacementRequested(const QPointF& documentPosition);
    // Emitted when a shape tool finishes creating a shape (drag release for
    // box/line/arrow tools, or a click/Enter for the polygon tool). The window
    // creates a vector layer from the payload.
    void shapeCreationRequested(const core::ShapeLayerData& shape);
    // Emitted whenever the crop rectangle is created or adjusted. An empty
    // rectangle means there is no usable crop area.
    void cropRectChanged(const QRectF& documentRect);
    void cropApplyRequested();
    void cropCancelRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class CropHandle {
        None,
        New,
        Move,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    void applyZoom(double zoom, bool fitMode);
    void updateSceneBounds();
    [[nodiscard]] CropHandle cropHandleAt(const QPointF& point) const;
    [[nodiscard]] QRectF clampCropRect(const QRectF& rect) const;
    void updateCropItem();
    [[nodiscard]] core::ShapeLayerData buildShapeFromDrag(
        const QPointF& start,
        const QPointF& end
    ) const;
    void updateShapePreview();
    void cancelShapeCreation();
    void finishPolygon();

    QGraphicsScene* scene_{nullptr};
    CheckerboardItem* checkerboard_{nullptr};
    CanvasImageItem* pixmapItem_{nullptr};
    SelectionItem* selectionItem_{nullptr};
    CropItem* cropItem_{nullptr};
    ShapePreviewItem* shapePreviewItem_{nullptr};
    QSize imageSize_;
    double zoom_{1.0};
    bool fitMode_{false};
    Tool activeTool_{Tool::Brush};
    bool drawing_{false};
    bool selecting_{false};
    bool panning_{false};
    bool cropDragging_{false};
    CropHandle cropHandle_{CropHandle::None};
    QPointF cropStartPoint_;
    QRectF cropStartRect_;
    bool hasCrop_{false};
    QPointF selectionStart_;
    QPoint lastPanPoint_;
    core::ShapeLayerData shapeTemplate_;
    bool shapeDragging_{false};
    QPointF shapeStart_;
    std::optional<core::ShapeLayerData> pendingShape_;
    std::vector<QPointF> polygonPoints_;
};

} // namespace glyma::app
