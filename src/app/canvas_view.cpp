#include "glyma/app/canvas_view.hpp"

#include <algorithm>
#include <cmath>

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTransform>
#include <QWheelEvent>

namespace glyma::app {

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

    pixmapItem_ = new QGraphicsPixmapItem();
    pixmapItem_->setTransformationMode(Qt::SmoothTransformation);
    pixmapItem_->setZValue(0.0);
    scene_->addItem(pixmapItem_);

    selectionItem_ = new SelectionItem(core::Selection{});
    selectionItem_->hide();
    scene_->addItem(selectionItem_);

    checkerboard_->hide();
    pixmapItem_->hide();
}

void CanvasView::setImage(const QImage& image) {
    if (image.isNull()) {
        imageSize_ = {};
        checkerboard_->hide();
        pixmapItem_->hide();
        scene_->setSceneRect(QRectF());
        applyZoom(1.0, false);
        return;
    }

    const bool hadImage = !imageSize_.isEmpty();
    const bool sizeChanged = imageSize_ != image.size();

    imageSize_ = image.size();
    pixmapItem_->setPixmap(QPixmap::fromImage(image));
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
    activeTool_ = tool;
    // A tool change ends any in-progress drag.
    if (selecting_) {
        selecting_ = false;
        emit selectionDragCancelled();
    }
    drawing_ = false;
    panning_ = false;
    setCursor(Qt::CrossCursor);
    selectionItem_->hide();
}

void CanvasView::setSelection(const core::Selection& selection) {
    selectionItem_->setSelection(selection);
    selectionItem_->setVisible(!selection.isEmpty());
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
    if (event->key() == Qt::Key_Escape) {
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
