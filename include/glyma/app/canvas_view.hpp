#pragma once

#include <QGraphicsView>
#include <QPointF>
#include <QSize>

#include "glyma/core/selection.hpp"

class QGraphicsPixmapItem;
class QImage;
class QGraphicsScene;
class QKeyEvent;
class QMouseEvent;
class QShowEvent;
class QResizeEvent;
class QWheelEvent;

namespace glyma::app {

class CheckerboardItem;
class CropItem;
class SelectionItem;

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
    };

    static constexpr double minZoom = 0.05;
    static constexpr double maxZoom = 32.0;
    static constexpr int fitMargin = 24;

    explicit CanvasView(QWidget* parent = nullptr);

    void setImage(const QImage& image);
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

    QGraphicsScene* scene_{nullptr};
    CheckerboardItem* checkerboard_{nullptr};
    QGraphicsPixmapItem* pixmapItem_{nullptr};
    SelectionItem* selectionItem_{nullptr};
    CropItem* cropItem_{nullptr};
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
};

} // namespace glyma::app
