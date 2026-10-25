#include "glyma/app/canvas_view.hpp"

#include <algorithm>
#include <cmath>

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QRect>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTransform>
#include <QWheelEvent>

#include "glyma/app/shape_renderer.hpp"

namespace glyma::app {

// Snapshot image item. Unlike QGraphicsPixmapItem, this exposes an incremental
// patch operation that repaints only the touched region: the owned QPixmap is
// modified in place (no copy-on-write detach, since the item paints it itself)
// and only that rectangle is marked dirty, so brush strokes do not force a full
// canvas re-upload and repaint on every pointer move.
class CanvasImageItem : public QGraphicsItem {
public:
    CanvasImageItem() {
        setZValue(0.0);
        setAcceptedMouseButtons(Qt::NoButton);
    }

    void setImage(const QImage& image) {
        prepareGeometryChange();
        pixmap_ = image.isNull() ? QPixmap() : QPixmap::fromImage(image);
        size_ = image.size();
        hasImage_ = !image.isNull();
        update();
    }

    void patchImage(const QRect& region, const QImage& subImage) {
        if (!hasImage_ || subImage.isNull() || region.isEmpty()) {
            return;
        }
        const QRect clipped = region.intersected(
            QRect(0, 0, size_.width(), size_.height())
        );
        if (clipped.isEmpty()) {
            return;
        }
        QPainter painter(&pixmap_);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(clipped.topLeft(), subImage);
        painter.end();
        update(QRectF(clipped));
    }

    void clear() {
        prepareGeometryChange();
        pixmap_ = QPixmap();
        size_ = QSize();
        hasImage_ = false;
        update();
    }

    [[nodiscard]] QRectF boundingRect() const override {
        return QRectF(0, 0, size_.width(), size_.height());
    }

    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem*,
        QWidget*
    ) override {
        if (!hasImage_) {
            return;
        }
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawPixmap(QPointF(0.0, 0.0), pixmap_);
    }

private:
    QPixmap pixmap_;
    QSize size_;
    bool hasImage_{false};
};

class CheckerboardItem : public QGraphicsItem {
public:
    CheckerboardItem() {
        QPixmap tile(16, 16);
        tile.fill(QColor(50, 50, 53));
        QPainter tilePainter(&tile);
        const QColor light(62, 62, 66);
        tilePainter.fillRect(QRect(0, 0, 8, 8), light);
        tilePainter.fillRect(QRect(8, 8, 8, 8), light);
        tilePainter.end();
        brush_ = QBrush(tile);

        setAcceptedMouseButtons(Qt::NoButton);
        setZValue(-1.0);
    }

    void setRect(const QRectF& rect) {
        prepareGeometryChange();
        rect_ = rect;
    }

    [[nodiscard]] QRectF boundingRect() const override {
        return rect_;
    }

    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem*,
        QWidget*
    ) override {
        painter->fillRect(rect_, brush_);
        QPen border(QColor(12, 12, 14));
        border.setCosmetic(true);
        painter->setPen(border);
        painter->drawRect(rect_);
    }

private:
    QRectF rect_;
    QBrush brush_;
};

// Dashed selection overlay drawn in document (scene) coordinates, on top of
// the image but independent of its pixels.
class SelectionItem : public QGraphicsItem {
public:
    explicit SelectionItem(const core::Selection& selection)
        : selection_(selection) {
        setZValue(1.0);
    }

    void setSelection(const core::Selection& selection) {
        prepareGeometryChange();
        selection_ = selection;
        update();
    }

    [[nodiscard]] QRectF boundingRect() const override {
        double left = 0.0;
        double top = 0.0;
        double right = 0.0;
        double bottom = 0.0;
        if (!selection_.bounds(left, top, right, bottom)) {
            return QRectF();
        }
        // Small margin so the cosmetic stroke is fully contained.
        const double margin = 1.0;
        return QRectF(left - margin, top - margin,
                      (right - left) + 2.0 * margin,
                      (bottom - top) + 2.0 * margin);
    }

    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem*,
        QWidget*
    ) override {
        if (selection_.isEmpty()) {
            return;
        }
        QPen pen(QColor(0, 200, 255), 1.0, Qt::DashLine, Qt::RoundCap,
                 Qt::RoundJoin);
        pen.setCosmetic(true);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        double left = 0.0;
        double top = 0.0;
        double right = 0.0;
        double bottom = 0.0;
        if (!selection_.bounds(left, top, right, bottom)) {
            return;
        }
        const QRectF rect(left, top, right - left, bottom - top);
        if (selection_.type() == core::Selection::Type::Ellipse) {
            painter->drawEllipse(rect);
        } else {
            painter->drawRect(rect);
        }
    }

private:
    core::Selection selection_;
};

// Crop rectangle overlay: dims everything outside the pending crop, draws a
// rule-of-thirds grid, and exposes eight draggable handles. Hit testing lives
// in CanvasView so the item itself never takes mouse events.
class CropItem : public QGraphicsItem {
public:
    CropItem() {
        setZValue(2.0);
        setAcceptedMouseButtons(Qt::NoButton);
    }

    void setCanvasRect(const QRectF& rect) {
        prepareGeometryChange();
        canvasRect_ = rect;
    }

    void setCropRect(const QRectF& rect) {
        prepareGeometryChange();
        cropRect_ = rect;
        update();
    }

    [[nodiscard]] QRectF cropRect() const noexcept {
        return cropRect_;
    }

    [[nodiscard]] QRectF boundingRect() const override {
        return canvasRect_.adjusted(-2.0, -2.0, 2.0, 2.0);
    }

    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem*,
        QWidget*
    ) override {
        if (canvasRect_.isEmpty() || cropRect_.isEmpty()) {
            return;
        }

        QPainterPath outside;
        outside.addRect(canvasRect_);
        QPainterPath inside;
        inside.addRect(cropRect_);
        painter->fillPath(
            outside.subtracted(inside),
            QColor(0, 0, 0, 130)
        );

        QPen border(QColor(245, 245, 245), 1.0, Qt::SolidLine,
                    Qt::SquareCap, Qt::MiterJoin);
        border.setCosmetic(true);
        painter->setPen(border);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(cropRect_);

        QPen thirds(QColor(245, 245, 245, 120), 1.0, Qt::DashLine);
        thirds.setCosmetic(true);
        painter->setPen(thirds);
        const double oneThirdX = cropRect_.left() + cropRect_.width() / 3.0;
        const double twoThirdsX = cropRect_.left() + cropRect_.width() * 2.0 / 3.0;
        const double oneThirdY = cropRect_.top() + cropRect_.height() / 3.0;
        const double twoThirdsY = cropRect_.top() + cropRect_.height() * 2.0 / 3.0;
        painter->drawLine(QPointF(oneThirdX, cropRect_.top()),
                          QPointF(oneThirdX, cropRect_.bottom()));
        painter->drawLine(QPointF(twoThirdsX, cropRect_.top()),
                          QPointF(twoThirdsX, cropRect_.bottom()));
        painter->drawLine(QPointF(cropRect_.left(), oneThirdY),
                          QPointF(cropRect_.right(), oneThirdY));
        painter->drawLine(QPointF(cropRect_.left(), twoThirdsY),
                          QPointF(cropRect_.right(), twoThirdsY));

        const double scale = std::abs(painter->worldTransform().m11());
        const double handleSize = scale > 1.0e-9 ? 7.0 / scale : 7.0;
        const double half = handleSize * 0.5;
        const QPointF handles[] = {
            cropRect_.topLeft(),
            QPointF(cropRect_.center().x(), cropRect_.top()),
            cropRect_.topRight(),
            QPointF(cropRect_.left(), cropRect_.center().y()),
            QPointF(cropRect_.right(), cropRect_.center().y()),
            cropRect_.bottomLeft(),
            QPointF(cropRect_.center().x(), cropRect_.bottom()),
            cropRect_.bottomRight(),
        };

        const QPen handlePen(QColor(25, 25, 28), 1.0);
        for (const QPointF& handle : handles) {
            const QRectF handleRect(
                handle.x() - half,
                handle.y() - half,
                handleSize,
                handleSize
            );
            painter->fillRect(handleRect, QColor(245, 245, 245));
            painter->setPen(handlePen);
            painter->drawRect(handleRect);
        }
    }

private:
    QRectF canvasRect_;
    QRectF cropRect_;
};

// Live preview of the shape currently being drawn. Draws the shape exactly as
// the renderer will, plus a dashed outline so the pending geometry is visible.
class ShapePreviewItem : public QGraphicsItem {
public:
    ShapePreviewItem() {
        setZValue(3.0);
        setAcceptedMouseButtons(Qt::NoButton);
    }

    void setShape(const std::optional<core::ShapeLayerData>& shape) {
        prepareGeometryChange();
        shape_ = shape;
        update();
    }

    [[nodiscard]] QRectF boundingRect() const override {
        if (!shape_.has_value()) {
            return QRectF();
        }
        const core::ShapeBounds bounds = core::shapeVisualBounds(*shape_);
        return QRectF(bounds.x, bounds.y, bounds.width, bounds.height)
            .adjusted(-3.0, -3.0, 3.0, 3.0);
    }

    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem*,
        QWidget*
    ) override {
        if (!shape_.has_value()) {
            return;
        }
        const core::ShapeLayerData& shape = *shape_;
        painter->save();
        const core::ShapePoint centre = core::shapeCentre(shape);
        if (shape.rotation != 0.0) {
            painter->translate(centre.x, centre.y);
            painter->rotate(shape.rotation);
            painter->translate(-centre.x, -centre.y);
        }
        ShapeRenderer::paintShape(*painter, shape);
        painter->restore();

        const core::ShapeBounds bounds = core::shapeVisualBounds(shape);
        QPen pen(QColor(0, 200, 255), 1.0, Qt::DashLine);
        pen.setCosmetic(true);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(
            QRectF(bounds.x, bounds.y, bounds.width, bounds.height)
        );
    }

private:
    std::optional<core::ShapeLayerData> shape_;
};

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent) {
    setObjectName(QStringLiteral("canvasView"));
    scene_ = new QGraphicsScene(this);
    setScene(scene_);

    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QColor(24, 24, 26));
    setAlignment(Qt::AlignCenter);
    setDragMode(QGraphicsView::NoDrag);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    checkerboard_ = new CheckerboardItem();
    scene_->addItem(checkerboard_);

    pixmapItem_ = new CanvasImageItem();
    scene_->addItem(pixmapItem_);

    selectionItem_ = new SelectionItem(core::Selection{});
    selectionItem_->hide();
    scene_->addItem(selectionItem_);

    cropItem_ = new CropItem();
    cropItem_->hide();
    scene_->addItem(cropItem_);

    shapePreviewItem_ = new ShapePreviewItem();
    shapePreviewItem_->hide();
    scene_->addItem(shapePreviewItem_);

    checkerboard_->hide();
    pixmapItem_->hide();
}

bool CanvasView::isShapeTool(Tool tool) noexcept {
    switch (tool) {
    case Tool::ShapeRectangle:
    case Tool::ShapeRoundedRectangle:
    case Tool::ShapeEllipse:
    case Tool::ShapeLine:
    case Tool::ShapeArrow:
    case Tool::ShapePolygon:
        return true;
    default:
        return false;
    }
}

core::ShapeType CanvasView::shapeTypeForTool(Tool tool) noexcept {
    switch (tool) {
    case Tool::ShapeRoundedRectangle:
        return core::ShapeType::RoundedRectangle;
    case Tool::ShapeEllipse:
        return core::ShapeType::Ellipse;
    case Tool::ShapeLine:
        return core::ShapeType::Line;
    case Tool::ShapeArrow:
        return core::ShapeType::Arrow;
    case Tool::ShapePolygon:
        return core::ShapeType::Polygon;
    case Tool::ShapeRectangle:
    default:
        return core::ShapeType::Rectangle;
    }
}

void CanvasView::setImage(const QImage& image) {
    if (image.isNull()) {
        imageSize_ = {};
        checkerboard_->hide();
        pixmapItem_->clear();
        pixmapItem_->hide();
        scene_->setSceneRect(QRectF());
        clearCrop();
        applyZoom(1.0, false);
        return;
    }

    const bool hadImage = !imageSize_.isEmpty();
    const bool sizeChanged = imageSize_ != image.size();
    if (sizeChanged) {
        clearCrop();
    }

    imageSize_ = image.size();
    pixmapItem_->setImage(image);
    pixmapItem_->setPos(0, 0);
    pixmapItem_->show();
    checkerboard_->setRect(
        QRectF(0, 0, imageSize_.width(), imageSize_.height())
    );
    checkerboard_->show();
    updateSceneBounds();

    // Preserve the user's zoom/pan when refreshing the same document. Only a
    // new canvas size (or the first image) resets the view to fit.
    if (!hadImage || sizeChanged) {
        fitToWindow();
    } else if (fitMode_) {
        applyZoom(fitZoom(imageSize_, viewport()->size()), true);
        centerOn(scene_->sceneRect().center());
    }
}

void CanvasView::patchImage(const QRect& region, const QImage& subImage) {
    if (region.isEmpty() || subImage.isNull()) {
        return;
    }
    pixmapItem_->patchImage(region, subImage);
}

QSize CanvasView::imageSize() const noexcept {
    return imageSize_;
}

double CanvasView::zoom() const noexcept {
    return zoom_;
}

bool CanvasView::isFitMode() const noexcept {
    return fitMode_;
}

CanvasView::Tool CanvasView::activeTool() const noexcept {
    return activeTool_;
}

void CanvasView::setActiveTool(Tool tool) {
    const bool changed = activeTool_ != tool;
    activeTool_ = tool;
    // A tool change ends any in-progress drag.
    if (selecting_) {
        selecting_ = false;
        emit selectionDragCancelled();
    }
    if (shapeDragging_ || !polygonPoints_.empty() || pendingShape_.has_value()) {
        cancelShapeCreation();
    }
    drawing_ = false;
    panning_ = false;
    cropDragging_ = false;
    cropHandle_ = CropHandle::None;
    setCursor(tool == Tool::Text ? Qt::IBeamCursor : Qt::CrossCursor);
    selectionItem_->hide();
    updateCropItem();
    if (changed) {
        emit toolChanged();
    }
}

void CanvasView::setShapeTemplate(const core::ShapeLayerData& shape) {
    shapeTemplate_ = shape;
}

void CanvasView::setSelection(const core::Selection& selection) {
    selectionItem_->setSelection(selection);
    selectionItem_->setVisible(!selection.isEmpty());
}

void CanvasView::setCropRect(const QRectF& rect) {
    const QRectF clamped = clampCropRect(rect);
    if (clamped.isEmpty()) {
        clearCrop();
        return;
    }
    hasCrop_ = true;
    cropItem_->setCropRect(clamped);
    updateCropItem();
}

void CanvasView::clearCrop() {
    hasCrop_ = false;
    cropDragging_ = false;
    cropHandle_ = CropHandle::None;
    cropItem_->setCropRect(QRectF());
    updateCropItem();
}

QRectF CanvasView::cropRect() const noexcept {
    return hasCrop_ ? cropItem_->cropRect() : QRectF();
}

bool CanvasView::hasCrop() const noexcept {
    return hasCrop_;
}

QRectF CanvasView::clampCropRect(const QRectF& rect) const {
    if (imageSize_.isEmpty() || rect.isNull()) {
        return {};
    }
    const QRectF bounds(0.0, 0.0,
                        static_cast<double>(imageSize_.width()),
                        static_cast<double>(imageSize_.height()));
    const QRectF clipped = rect.normalized().intersected(bounds);
    if (clipped.width() <= 0.0 || clipped.height() <= 0.0) {
        return {};
    }
    return clipped;
}

CanvasView::CropHandle CanvasView::cropHandleAt(const QPointF& point) const {
    if (!hasCrop_ || imageSize_.isEmpty()) {
        return CropHandle::None;
    }
    const QRectF rect = cropRect();
    if (rect.isEmpty()) {
        return CropHandle::None;
    }

    const double tolerance = 9.0 / std::max(zoom_, 1.0e-6);
    const bool nearLeft = std::abs(point.x() - rect.left()) <= tolerance;
    const bool nearRight = std::abs(point.x() - rect.right()) <= tolerance;
    const bool nearTop = std::abs(point.y() - rect.top()) <= tolerance;
    const bool nearBottom = std::abs(point.y() - rect.bottom()) <= tolerance;
    const bool verticallyInside =
        point.y() >= rect.top() - tolerance &&
        point.y() <= rect.bottom() + tolerance;
    const bool horizontallyInside =
        point.x() >= rect.left() - tolerance &&
        point.x() <= rect.right() + tolerance;

    if (nearLeft && nearTop) {
        return CropHandle::TopLeft;
    }
    if (nearRight && nearTop) {
        return CropHandle::TopRight;
    }
    if (nearLeft && nearBottom) {
        return CropHandle::BottomLeft;
    }
    if (nearRight && nearBottom) {
        return CropHandle::BottomRight;
    }
    if (nearLeft && verticallyInside) {
        return CropHandle::Left;
    }
    if (nearRight && verticallyInside) {
        return CropHandle::Right;
    }
    if (nearTop && horizontallyInside) {
        return CropHandle::Top;
    }
    if (nearBottom && horizontallyInside) {
        return CropHandle::Bottom;
    }
    if (rect.contains(point)) {
        return CropHandle::Move;
    }
    return CropHandle::None;
}

void CanvasView::updateCropItem() {
    cropItem_->setCanvasRect(
        QRectF(0.0, 0.0,
               static_cast<double>(imageSize_.width()),
               static_cast<double>(imageSize_.height()))
    );
    cropItem_->setVisible(hasCrop_ && activeTool_ == Tool::Crop);
}

core::ShapeLayerData CanvasView::buildShapeFromDrag(
    const QPointF& start,
    const QPointF& end
) const {
    core::ShapeLayerData shape = shapeTemplate_;
    shape.type = shapeTypeForTool(activeTool_);
    if (core::isLineShape(shape.type)) {
        core::setLineGeometry(shape, start.x(), start.y(), end.x(), end.y());
    } else if (shape.type != core::ShapeType::Polygon) {
        core::setRectGeometry(
            shape,
            start.x(),
            start.y(),
            end.x() - start.x(),
            end.y() - start.y()
        );
    }
    return shape;
}

void CanvasView::updateShapePreview() {
    if (pendingShape_.has_value()) {
        shapePreviewItem_->setShape(pendingShape_);
        shapePreviewItem_->show();
    } else {
        shapePreviewItem_->setShape(std::nullopt);
        shapePreviewItem_->hide();
    }
}

void CanvasView::cancelShapeCreation() {
    shapeDragging_ = false;
    pendingShape_.reset();
    polygonPoints_.clear();
    updateShapePreview();
}

void CanvasView::finishPolygon() {
    if (activeTool_ != Tool::ShapePolygon || polygonPoints_.size() < 3) {
        cancelShapeCreation();
        return;
    }
    core::ShapeLayerData shape = shapeTemplate_;
    shape.type = core::ShapeType::Polygon;
    std::vector<core::ShapePoint> points;
    points.reserve(polygonPoints_.size());
    for (const QPointF& point : polygonPoints_) {
        points.push_back(core::ShapePoint{point.x(), point.y()});
    }
    core::setPolygonGeometry(shape, std::move(points));
    cancelShapeCreation();
    emit shapeCreationRequested(shape);
}

double CanvasView::clampZoom(double zoom) noexcept {
    if (!std::isfinite(zoom)) {
        return 1.0;
    }
    return std::clamp(zoom, minZoom, maxZoom);
}

double CanvasView::fitZoom(
    const QSize& imageSize,
    const QSize& viewportSize,
    int margin
) noexcept {
    if (imageSize.width() <= 0 || imageSize.height() <= 0 ||
        viewportSize.width() <= 0 || viewportSize.height() <= 0) {
        return 1.0;
    }

    const double availableWidth = static_cast<double>(viewportSize.width()) - 2.0 * margin;
    const double availableHeight = static_cast<double>(viewportSize.height()) - 2.0 * margin;
    if (availableWidth <= 0.0 || availableHeight <= 0.0) {
        return 1.0;
    }

    const double scaleX = availableWidth / imageSize.width();
    const double scaleY = availableHeight / imageSize.height();
    return clampZoom(std::min(scaleX, scaleY));
}

void CanvasView::applyZoom(double zoom, bool fitMode) {
    zoom_ = clampZoom(zoom);
    fitMode_ = fitMode;
    QTransform transform;
    transform.scale(zoom_, zoom_);
    setTransform(transform);
}

void CanvasView::updateSceneBounds() {
    const QRectF bounds(0, 0, imageSize_.width(), imageSize_.height());
    scene_->setSceneRect(bounds);
    checkerboard_->setRect(bounds);
    cropItem_->setCanvasRect(bounds);
}

void CanvasView::setZoom(double zoom) {
    applyZoom(zoom, false);
}

void CanvasView::zoomIn() {
    applyZoom(zoom_ * 1.25, false);
}

void CanvasView::zoomOut() {
    applyZoom(zoom_ / 1.25, false);
}

void CanvasView::fitToWindow() {
    if (imageSize_.isEmpty()) {
        return;
    }
    const QSize viewportSize = viewport()->size();
    applyZoom(fitZoom(imageSize_, viewportSize), true);
    centerOn(scene_->sceneRect().center());
}

void CanvasView::resetZoom() {
    applyZoom(1.0, false);
}

void CanvasView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (fitMode_ && !imageSize_.isEmpty()) {
        applyZoom(fitZoom(imageSize_, viewport()->size()), true);
        centerOn(scene_->sceneRect().center());
    }
}

void CanvasView::showEvent(QShowEvent* event) {
    QGraphicsView::showEvent(event);
    if (fitMode_ && !imageSize_.isEmpty()) {
        applyZoom(fitZoom(imageSize_, viewport()->size()), true);
        centerOn(scene_->sceneRect().center());
    }
}

QPointF CanvasView::documentPosition(const QPointF& viewportPosition) const {
    bool invertible = false;
    const QTransform inverse = viewportTransform().inverted(&invertible);
    if (!invertible) {
        return {};
    }
    return inverse.map(viewportPosition);
}

void CanvasView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (imageSize_.isEmpty()) {
            event->ignore();
            return;
        }
        setFocus(Qt::MouseFocusReason);
        const QPointF documentPos = documentPosition(event->position());
        switch (activeTool_) {
        case Tool::Brush:
            drawing_ = true;
            emit strokeStarted(documentPos);
            break;
        case Tool::SelectRectangle:
        case Tool::SelectEllipse:
            selecting_ = true;
            selectionStart_ = documentPos;
            emit selectionDragStarted(selectionStart_);
            break;
        case Tool::BucketFill:
            emit bucketFillRequested(documentPos);
            break;
        case Tool::Text:
            emit textPlacementRequested(documentPos);
            break;
        case Tool::ShapeRectangle:
        case Tool::ShapeRoundedRectangle:
        case Tool::ShapeEllipse:
        case Tool::ShapeLine:
        case Tool::ShapeArrow:
            shapeDragging_ = true;
            shapeStart_ = documentPos;
            pendingShape_ = buildShapeFromDrag(documentPos, documentPos);
            updateShapePreview();
            break;
        case Tool::ShapePolygon: {
            polygonPoints_.push_back(documentPos);
            core::ShapeLayerData shape = shapeTemplate_;
            shape.type = core::ShapeType::Polygon;
            core::setPolygonGeometry(
                shape,
                std::vector<core::ShapePoint>{
                    core::ShapePoint{documentPos.x(), documentPos.y()}
                }
            );
            pendingShape_ = shape;
            updateShapePreview();
            break;
        }
        case Tool::Crop: {
            cropStartPoint_ = documentPos;
            cropStartRect_ = hasCrop_ ? cropRect() : QRectF();
            cropHandle_ = cropHandleAt(documentPos);
            if (cropHandle_ == CropHandle::None) {
                cropHandle_ = CropHandle::New;
                hasCrop_ = true;
                cropItem_->setCropRect(
                    QRectF(documentPos, QSizeF(0.0, 0.0))
                );
            }
            cropDragging_ = true;
            updateCropItem();
            emit cropRectChanged(cropRect());
            break;
        }
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastPanPoint_ = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        if (activeTool_ == Tool::Crop && hasCrop_) {
            cropDragging_ = false;
            clearCrop();
            emit cropRectChanged(QRectF());
            emit cropCancelRequested();
            event->accept();
            return;
        }
        if (selecting_) {
            selecting_ = false;
            emit selectionDragCancelled();
            event->accept();
            return;
        }
        if (drawing_) {
            drawing_ = false;
            emit strokeCancelled();
            event->accept();
            return;
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const QPoint current = event->position().toPoint();
        const QPoint delta = current - lastPanPoint_;
        lastPanPoint_ = current;
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - delta.x()
        );
        verticalScrollBar()->setValue(
            verticalScrollBar()->value() - delta.y()
        );
        event->accept();
        return;
    }

    if (cropDragging_) {
        const QPointF point = documentPosition(event->position());
        QRectF rect;
        switch (cropHandle_) {
        case CropHandle::New:
            rect = QRectF(cropStartPoint_, point).normalized();
            break;
        case CropHandle::Move: {
            const double deltaX = point.x() - cropStartPoint_.x();
            const double deltaY = point.y() - cropStartPoint_.y();
            rect = cropStartRect_.translated(deltaX, deltaY);
            const double maxX = std::max(
                0.0,
                static_cast<double>(imageSize_.width()) - rect.width()
            );
            const double maxY = std::max(
                0.0,
                static_cast<double>(imageSize_.height()) - rect.height()
            );
            rect.moveLeft(std::clamp(rect.left(), 0.0, maxX));
            rect.moveTop(std::clamp(rect.top(), 0.0, maxY));
            break;
        }
        default: {
            double left = cropStartRect_.left();
            double top = cropStartRect_.top();
            double right = cropStartRect_.right();
            double bottom = cropStartRect_.bottom();
            if (cropHandle_ == CropHandle::Left ||
                cropHandle_ == CropHandle::TopLeft ||
                cropHandle_ == CropHandle::BottomLeft) {
                left = point.x();
            }
            if (cropHandle_ == CropHandle::Right ||
                cropHandle_ == CropHandle::TopRight ||
                cropHandle_ == CropHandle::BottomRight) {
                right = point.x();
            }
            if (cropHandle_ == CropHandle::Top ||
                cropHandle_ == CropHandle::TopLeft ||
                cropHandle_ == CropHandle::TopRight) {
                top = point.y();
            }
            if (cropHandle_ == CropHandle::Bottom ||
                cropHandle_ == CropHandle::BottomLeft ||
                cropHandle_ == CropHandle::BottomRight) {
                bottom = point.y();
            }
            rect = QRectF(QPointF(left, top), QPointF(right, bottom))
                       .normalized();
            break;
        }
        }

        const QRectF clamped = clampCropRect(rect);
        if (clamped.isEmpty()) {
            cropItem_->setCropRect(QRectF());
            emit cropRectChanged(QRectF());
        } else {
            cropItem_->setCropRect(clamped);
            emit cropRectChanged(clamped);
        }
        updateCropItem();
        event->accept();
        return;
    }

    if (shapeDragging_) {
        pendingShape_ = buildShapeFromDrag(
            shapeStart_, documentPosition(event->position())
        );
        updateShapePreview();
        event->accept();
        return;
    }

    if (activeTool_ == Tool::ShapePolygon && !polygonPoints_.empty()) {
        pendingShape_ = buildShapeFromDrag(
            polygonPoints_.front(), documentPosition(event->position())
        );
        std::vector<core::ShapePoint> points;
        points.reserve(polygonPoints_.size() + 1);
        for (const QPointF& point : polygonPoints_) {
            points.push_back(core::ShapePoint{point.x(), point.y()});
        }
        const QPointF cursor = documentPosition(event->position());
        points.push_back(core::ShapePoint{cursor.x(), cursor.y()});
        core::setPolygonGeometry(*pendingShape_, std::move(points));
        updateShapePreview();
        event->accept();
        return;
    }

    if (selecting_) {
        emit selectionDragMoved(documentPosition(event->position()));
        event->accept();
        return;
    }

    if (drawing_) {
        emit strokeMoved(documentPosition(event->position()));
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && cropDragging_) {
        cropDragging_ = false;
        cropHandle_ = CropHandle::None;
        const QRectF rect = cropRect();
        if (rect.width() < 1.0 || rect.height() < 1.0) {
            clearCrop();
            emit cropRectChanged(QRectF());
        } else {
            emit cropRectChanged(rect);
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && shapeDragging_) {
        shapeDragging_ = false;
        const core::ShapeLayerData shape = buildShapeFromDrag(
            shapeStart_, documentPosition(event->position())
        );
        pendingShape_.reset();
        updateShapePreview();
        emit shapeCreationRequested(shape);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && selecting_) {
        selecting_ = false;
        emit selectionDragCompleted(selectionStart_,
                                    documentPosition(event->position()));
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && drawing_) {
        drawing_ = false;
        emit strokeEnded();
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton && panning_) {
        panning_ = false;
        setCursor(Qt::CrossCursor);
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::keyPressEvent(QKeyEvent* event) {
    if (activeTool_ == Tool::Crop &&
        (event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter)) {
        if (hasCrop_) {
            emit cropApplyRequested();
            event->accept();
            return;
        }
    }
    if (activeTool_ == Tool::ShapePolygon &&
        (event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter)) {
        finishPolygon();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (shapeDragging_ || !polygonPoints_.empty() ||
            pendingShape_.has_value()) {
            cancelShapeCreation();
            event->accept();
            return;
        }
        if (activeTool_ == Tool::Crop && hasCrop_) {
            clearCrop();
            emit cropRectChanged(QRectF());
            emit cropCancelRequested();
            event->accept();
            return;
        }
        if (selecting_) {
            selecting_ = false;
            emit selectionDragCancelled();
            event->accept();
            return;
        }
        if (drawing_) {
            drawing_ = false;
            emit strokeCancelled();
            event->accept();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::wheelEvent(QWheelEvent* event) {
    if (imageSize_.isEmpty()) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    const auto previousAnchor = transformationAnchor();
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    const double factor = std::pow(1.0015, static_cast<double>(delta));
    applyZoom(zoom_ * factor, false);
    setTransformationAnchor(previousAnchor);
    event->accept();
}

} // namespace glyma::app
