#pragma once

#include <QGraphicsView>
#include <QPointF>
#include <QSize>

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

// Graphics-view based canvas with checkerboard transparency, zoom, panning,
// and fit-to-window support. The view displays a snapshot image; it never
// retains or mutates engine document storage.
class CanvasView : public QGraphicsView {
    Q_OBJECT

public:
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

signals:
    void strokeStarted(const QPointF& documentPosition);
    void strokeMoved(const QPointF& documentPosition);
    void strokeEnded();
    void strokeCancelled();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void applyZoom(double zoom, bool fitMode);
    void updateSceneBounds();

    QGraphicsScene* scene_{nullptr};
    CheckerboardItem* checkerboard_{nullptr};
    QGraphicsPixmapItem* pixmapItem_{nullptr};
    QSize imageSize_;
    double zoom_{1.0};
    bool fitMode_{false};
    bool drawing_{false};
    bool panning_{false};
    QPoint lastPanPoint_;
};

} // namespace glyma::app
