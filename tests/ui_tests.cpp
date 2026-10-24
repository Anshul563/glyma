#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QListWidget>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QtGlobal>

#include "glyma/app/canvas_view.hpp"
#include "glyma/app/layer_dock.hpp"
#include "glyma/app/main_window.hpp"
#include "glyma/app/qt_image.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"

namespace {

using glyma::app::CanvasView;
using glyma::app::MainWindow;
using glyma::app::toQImage;
using glyma::core::Document;
using glyma::core::ImageBuffer;
using glyma::core::LayerId;
using glyma::core::Pixel;

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) < 1e-9;
}

void testBufferConversion() {
    ImageBuffer buffer(3, 2);
    buffer.setPixel(0, 0, Pixel{10, 20, 30, 40});
    buffer.setPixel(1, 0, Pixel{50, 60, 70, 80});
    buffer.setPixel(2, 0, Pixel{90, 100, 110, 120});
    buffer.setPixel(0, 1, Pixel{1, 2, 3, 4});
    buffer.setPixel(1, 1, Pixel{5, 6, 7, 8});
    buffer.setPixel(2, 1, Pixel{9, 10, 11, 12});

    const QImage image = toQImage(buffer);
    assert(!image.isNull());
    assert(image.width() == 3);
    assert(image.height() == 2);
    assert(image.format() == QImage::Format_RGBA8888);

    const QColor first = image.pixelColor(0, 0);
    assert(first.red() == 10);
    assert(first.green() == 20);
    assert(first.blue() == 30);
    assert(first.alpha() == 40);

    const QColor last = image.pixelColor(2, 1);
    assert(last.red() == 9);
    assert(last.green() == 10);
    assert(last.blue() == 11);
    assert(last.alpha() == 12);
}

void testConversionOwnsPixels() {
    QImage image;
    {
        ImageBuffer buffer(2, 1);
        buffer.setPixel(0, 0, Pixel{255, 0, 0, 255});
        buffer.setPixel(1, 0, Pixel{0, 255, 0, 128});
        image = toQImage(buffer);
    }

    // The source buffer is destroyed; the image must still own its data.
    assert(image.pixelColor(0, 0) == QColor(255, 0, 0, 255));
    assert(image.pixelColor(1, 0) == QColor(0, 255, 0, 128));
}

void testFitCalculations() {
    const double fit = CanvasView::fitZoom(QSize(1000, 500), QSize(500, 500), 20);
    assert(nearlyEqual(fit, 0.46));

    assert(nearlyEqual(CanvasView::fitZoom(QSize(0, 10), QSize(500, 500)), 1.0));
    assert(nearlyEqual(CanvasView::fitZoom(QSize(10, 10), QSize(0, 500)), 1.0));
    assert(nearlyEqual(CanvasView::fitZoom(QSize(10, 10), QSize(30, 30), 20), 1.0));
    assert(nearlyEqual(CanvasView::fitZoom(QSize(100000, 1000), QSize(500, 500)), 0.05));
}

void testZoomLimits() {
    assert(nearlyEqual(CanvasView::clampZoom(0.0), CanvasView::minZoom));
    assert(nearlyEqual(CanvasView::clampZoom(1000.0), CanvasView::maxZoom));
    assert(nearlyEqual(CanvasView::clampZoom(1.0), 1.0));
    assert(nearlyEqual(CanvasView::clampZoom(std::nan("")), 1.0));
}

void testCanvasZoomAndFit() {
    CanvasView view;
    view.resize(400, 300);

    QImage image(1000, 500, QImage::Format_RGBA8888);
    image.fill(QColor(20, 30, 40, 255));
    view.setImage(image);

    assert(view.imageSize() == QSize(1000, 500));
    assert(view.isFitMode());
    assert(view.zoom() < 1.0);

    view.resetZoom();
    assert(!view.isFitMode());
    assert(nearlyEqual(view.zoom(), 1.0));

    view.zoomIn();
    assert(view.zoom() > 1.0);
    view.zoomOut();
    assert(nearlyEqual(view.zoom(), 1.0));

    view.fitToWindow();
    assert(view.isFitMode());
    assert(view.zoom() < 1.0);

    // Refreshing the same-size image keeps the current zoom and pan.
    view.resetZoom();
    view.setZoom(2.0);
    view.setImage(image);
    assert(!view.isFitMode());
    assert(nearlyEqual(view.zoom(), 2.0));

    view.setZoom(1.0e9);
    assert(nearlyEqual(view.zoom(), CanvasView::maxZoom));
    view.setZoom(0.0);
    assert(nearlyEqual(view.zoom(), CanvasView::minZoom));
}

void testMainWindowConstruction() {
    MainWindow window;

    assert(window.windowTitle().contains(QStringLiteral("Glyma")));
    auto* canvas = window.findChild<CanvasView*>(QStringLiteral("canvasView"));
    auto* layers = window.findChild<QListWidget*>(QStringLiteral("layersList"));
    auto* undo = window.findChild<QAction*>(QStringLiteral("actionEditUndo"));
    auto* redo = window.findChild<QAction*>(QStringLiteral("actionEditRedo"));
    auto* opacity = window.findChild<QSpinBox*>(QStringLiteral("opacitySpin"));
    assert(canvas != nullptr);
    assert(layers != nullptr);
    assert(undo != nullptr);
    assert(redo != nullptr);
    assert(opacity != nullptr);

    const Document& document = window.controller().document();
    assert(document.width() == 1280);
    assert(document.height() == 720);
    assert(document.layerCount() == 1);
    assert(canvas->imageSize() == QSize(1280, 720));
    assert(layers->count() == 1);
    assert(!undo->isEnabled());
    assert(!redo->isEnabled());
    assert(opacity->isEnabled());

    const LayerId id = document.layerStack().layers().front().id();
    window.controller().setLayerVisibility(id, false);
    assert(!document.findLayer(id)->visible());
    assert(undo->isEnabled());
    assert(layers->item(0)->checkState() == Qt::Unchecked);

    window.controller().undo();
    assert(document.findLayer(id)->visible());
    assert(layers->item(0)->checkState() == Qt::Checked);

    // Exercise the dock -> controller -> document -> dock round trip.
    layers->item(0)->setCheckState(Qt::Unchecked);
    QCoreApplication::processEvents();
    assert(!document.findLayer(id)->visible());
    assert(layers->item(0)->checkState() == Qt::Unchecked);

    layers->item(0)->setCheckState(Qt::Checked);
    QCoreApplication::processEvents();
    assert(document.findLayer(id)->visible());

    auto* item = layers->item(0);
    assert(item->text() == QStringLiteral("Layer 1"));
    assert(layers->currentItem() == item);

    opacity->setValue(40);
    QCoreApplication::processEvents();
    assert(std::abs(document.findLayer(id)->opacity() - 0.4F) < 0.001F);
    assert(std::abs(opacity->value() - 40) <= 1);
}

void testControllerFileOperations() {
    MainWindow window;
    QTemporaryDir directory;
    assert(directory.isValid());

    const QString projectPath = directory.filePath(QStringLiteral("scene.glyma"));
    assert(window.controller().save(projectPath));
    assert(QFileInfo::exists(projectPath));
    assert(window.controller().filePath() == QFileInfo(projectPath).absoluteFilePath());

    const LayerId id = window.controller().document().layerStack().layers().front().id();
    window.controller().setLayerVisibility(id, false);
    assert(!window.controller().document().findLayer(id)->visible());

    assert(window.controller().openFile(projectPath));
    assert(window.controller().document().findLayer(id) != nullptr);
    assert(window.controller().document().findLayer(id)->visible());
    assert(window.controller().document().layerCount() == 1);

    const QString pngPath = directory.filePath(QStringLiteral("scene.png"));
    assert(window.controller().exportPng(pngPath));
    assert(QFileInfo::exists(pngPath));

    assert(window.controller().openFile(pngPath));
    assert(window.controller().filePath().isEmpty());
    assert(window.controller().document().width() == 1280);
    assert(window.controller().document().height() == 720);
    assert(window.controller().document().layerCount() == 1);
}

} // namespace

int main(int argc, char* argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") &&
        qEnvironmentVariableIsEmpty("DISPLAY") &&
        qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Glyma UI Tests"));

    testBufferConversion();
    testConversionOwnsPixels();
    testFitCalculations();
    testZoomLimits();
    testCanvasZoomAndFit();
    testMainWindowConstruction();
    testControllerFileOperations();

    std::cout << "All Glyma UI tests passed\n";
    return 0;
}
