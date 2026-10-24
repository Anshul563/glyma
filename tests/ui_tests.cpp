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
#include <QToolButton>
#include <QtGlobal>

#include "glyma/app/canvas_view.hpp"
#include "glyma/app/layer_dock.hpp"
#include "glyma/app/main_window.hpp"
#include "glyma/app/qt_image.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"

namespace {

using glyma::app::AppController;
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

void testNewDocumentOptions() {
    AppController controller;
    int errors = 0;
    QObject::connect(
        &controller, &AppController::errorOccurred,
        [&errors](const QString&) { ++errors; }
    );

    assert(controller.newDocument(
        QStringLiteral("Solid"), 4, 3, true, Pixel{10, 20, 30, 40}
    ));
    const Document& document = controller.document();
    assert(document.name() == "Solid");
    assert(document.width() == 4);
    assert(document.height() == 3);
    assert(document.layerCount() == 1);
    assert(document.hasSolidBackground());
    assert(document.backgroundColor() == (Pixel{10, 20, 30, 40}));
    assert(controller.render().getPixel(0, 0) == (Pixel{10, 20, 30, 40}));
    assert(!controller.isModified());

    assert(controller.newDocument(QStringLiteral("Clear"), 2, 2));
    assert(!controller.document().hasSolidBackground());
    assert(controller.document().backgroundColor() == (Pixel{0, 0, 0, 0}));

    assert(!controller.newDocument(QStringLiteral("Invalid"), 0, 10));
    assert(controller.document().name() == "Clear");
    assert(errors == 1);
}

void testModifiedStateTracking() {
    MainWindow window;
    AppController& controller = window.controller();
    assert(!controller.isModified());
    assert(!window.isWindowModified());

    controller.addLayer();
    QCoreApplication::processEvents();
    assert(controller.isModified());
    assert(window.isWindowModified());

    QTemporaryDir directory;
    assert(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("modified.glyma"));
    assert(controller.save(path));
    assert(!controller.isModified());
    assert(!window.isWindowModified());

    assert(controller.undo());
    QCoreApplication::processEvents();
    assert(controller.isModified());
    assert(window.isWindowModified());

    assert(controller.redo());
    QCoreApplication::processEvents();
    assert(!controller.isModified());
    assert(!window.isWindowModified());
}

void testLayerManagement() {
    MainWindow window;
    AppController& controller = window.controller();
    const Document& document = controller.document();
    auto* layers = window.findChild<QListWidget*>(QStringLiteral("layersList"));
    auto* add = window.findChild<QToolButton*>(QStringLiteral("layerAddButton"));
    auto* remove = window.findChild<QToolButton*>(QStringLiteral("layerDeleteButton"));
    auto* up = window.findChild<QToolButton*>(QStringLiteral("layerUpButton"));
    auto* down = window.findChild<QToolButton*>(QStringLiteral("layerDownButton"));
    assert(layers != nullptr);
    assert(add != nullptr);
    assert(remove != nullptr);
    assert(up != nullptr);
    assert(down != nullptr);

    const LayerId first = document.layerStack().layers().front().id();
    assert(document.layerCount() == 1);

    add->click();
    QCoreApplication::processEvents();
    assert(document.layerCount() == 2);
    const LayerId added = document.layerStack().layers().back().id();
    assert(added != first);
    assert(controller.selectedLayerId() == added);
    assert(document.layerStack().layers().back().name() == "Layer 2");
    assert(layers->item(0)->text() == QStringLiteral("Layer 2"));

    layers->item(0)->setText(QStringLiteral("Sky"));
    QCoreApplication::processEvents();
    assert(document.layerStack().layers().back().name() == "Sky");

    down->click();
    QCoreApplication::processEvents();
    assert(document.layerStack().layers().front().id() == added);
    assert(document.layerStack().layers().front().name() == "Sky");

    up->click();
    QCoreApplication::processEvents();
    assert(document.layerStack().layers().back().id() == added);

    remove->click();
    QCoreApplication::processEvents();
    assert(document.layerCount() == 1);
    assert(document.layerStack().layers().front().id() == first);
    assert(controller.selectedLayerId() == first);

    assert(controller.undo());
    QCoreApplication::processEvents();
    assert(document.layerCount() == 2);
    assert(document.layerStack().layers().back().id() == added);
    assert(document.layerStack().layers().back().name() == "Sky");

    assert(controller.redo());
    QCoreApplication::processEvents();
    assert(document.layerCount() == 1);
}

void testControllerFailurePaths() {
    AppController controller;
    int errors = 0;
    QObject::connect(
        &controller, &AppController::errorOccurred,
        [&errors](const QString&) { ++errors; }
    );
    QTemporaryDir directory;
    assert(directory.isValid());

    controller.addLayer();
    const std::size_t layerCount = controller.document().layerCount();
    const Document::Id documentId = controller.document().id();
    assert(controller.isModified());

    const QString missingSave =
        directory.filePath(QStringLiteral("missing/sub/scene.glyma"));
    assert(!controller.save(missingSave));
    assert(controller.isModified());
    assert(controller.filePath().isEmpty());

    assert(!controller.openFile(directory.filePath(QStringLiteral("missing.glyma"))));
    assert(controller.document().id() == documentId);
    assert(controller.document().layerCount() == layerCount);
    assert(controller.isModified());
    assert(errors == 2);
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
    // Imported pixels are not yet stored in a project file.
    assert(window.controller().isModified());

    const QString importedProject =
        directory.filePath(QStringLiteral("imported.glyma"));
    assert(window.controller().save(importedProject));
    assert(!window.controller().isModified());
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
    testNewDocumentOptions();
    testModifiedStateTracking();
    testLayerManagement();
    testControllerFailurePaths();
    testControllerFileOperations();

    std::cout << "All Glyma UI tests passed\n";
    return 0;
}
