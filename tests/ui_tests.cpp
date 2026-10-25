#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QImage>
#include <QListWidget>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPointF>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QtGlobal>

#include "glyma/app/adjustment_dialog.hpp"
#include "glyma/app/canvas_view.hpp"
#include "glyma/app/icon_manager.hpp"
#include "glyma/app/layer_dock.hpp"
#include "glyma/app/main_window.hpp"
#include "glyma/app/qt_image.hpp"
#include "glyma/app/shape_properties_panel.hpp"
#include "glyma/app/shape_renderer.hpp"
#include "glyma/app/text_renderer.hpp"
#include "glyma/app/typography_panel.hpp"
#include "glyma/core/adjustments.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/fill.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/selection.hpp"
#include "glyma/core/shape_layer.hpp"
#include "glyma/core/text_layer.hpp"
#include "glyma/core/transform.hpp"

namespace {

using glyma::app::AdjustmentDialog;
using glyma::app::AppController;
using glyma::app::CanvasView;
using glyma::app::IconManager;
using glyma::app::MainWindow;
using glyma::app::ShapePropertiesPanel;
using glyma::app::ShapeRenderer;
using glyma::app::TextRenderer;
using glyma::app::TypographyPanel;
using glyma::app::toQImage;
using glyma::core::AdjustmentSettings;
using glyma::core::BrushMode;
using glyma::core::BrushSettings;
using glyma::core::Document;
using glyma::core::FillSettings;
using glyma::core::ImageBuffer;
using glyma::core::Layer;
using glyma::core::LayerId;
using glyma::core::Pixel;
using glyma::core::Selection;
using glyma::core::ShapeLayerData;
using glyma::core::ShapeType;
using glyma::core::TextHorizontalAlign;
using glyma::core::TextLayerData;
using glyma::core::TextStyle;

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

void testBrushToolControls() {
    MainWindow window;
    auto* brush = window.findChild<QAction*>(QStringLiteral("actionToolBrush"));
    auto* eraser = window.findChild<QAction*>(QStringLiteral("actionToolEraser"));
    auto* color = window.findChild<QToolButton*>(QStringLiteral("toolColorButton"));
    auto* size = window.findChild<QSpinBox*>(QStringLiteral("brushSizeSpin"));
    auto* opacity = window.findChild<QSpinBox*>(QStringLiteral("brushOpacitySpin"));
    auto* hardness = window.findChild<QSpinBox*>(QStringLiteral("brushHardnessSpin"));
    assert(brush != nullptr);
    assert(eraser != nullptr);
    assert(color != nullptr);
    assert(size != nullptr);
    assert(opacity != nullptr);
    assert(hardness != nullptr);

    assert(brush->isCheckable());
    assert(brush->isChecked());
    assert(!eraser->isChecked());
    assert(size->value() == 20);
    assert(opacity->value() == 100);
    assert(hardness->value() == 100);

    const BrushSettings defaults = window.controller().brushSettings();
    assert(std::abs(defaults.diameter - 20.0) < 1e-9);
    assert(std::abs(defaults.opacity - 1.0F) < 1e-6F);
    assert(std::abs(defaults.hardness - 1.0F) < 1e-6F);
    assert(defaults.color == (Pixel{0, 0, 0, 255}));
    assert(window.controller().brushMode() == BrushMode::Paint);

    eraser->trigger();
    QCoreApplication::processEvents();
    assert(eraser->isChecked());
    assert(!brush->isChecked());
    assert(window.controller().brushMode() == BrushMode::Erase);

    brush->trigger();
    QCoreApplication::processEvents();
    assert(brush->isChecked());
    assert(window.controller().brushMode() == BrushMode::Paint);

    size->setValue(7);
    opacity->setValue(40);
    hardness->setValue(25);
    QCoreApplication::processEvents();
    const BrushSettings updated = window.controller().brushSettings();
    assert(std::abs(updated.diameter - 7.0) < 1e-9);
    assert(std::abs(updated.opacity - 0.4F) < 1e-6F);
    assert(std::abs(updated.hardness - 0.25F) < 1e-6F);
}

void testControllerBrushIntegration() {
    AppController controller;
    assert(controller.newDocument(QStringLiteral("Brush"), 8, 8));
    const LayerId firstId = controller.selectedLayerId();
    Layer* first = controller.document().findLayer(firstId);
    assert(first != nullptr);
    const ImageBuffer firstBefore = first->buffer();

    BrushSettings settings;
    settings.diameter = 2.0;
    settings.color = Pixel{255, 0, 0, 255};
    settings.opacity = 1.0F;
    settings.hardness = 1.0F;
    assert(controller.setBrushSettings(settings));
    controller.setBrushMode(BrushMode::Paint);

    assert(controller.beginBrushStroke(2.5, 2.5));
    assert(controller.isBrushStrokeActive());
    assert(controller.updateBrushStroke(5.5, 2.5));
    assert(controller.endBrushStroke());
    assert(!controller.isBrushStrokeActive());
    assert(controller.canUndo());
    assert(controller.isModified());
    assert(first->buffer().getPixel(2, 2) == (Pixel{255, 0, 0, 255}));
    assert(first->buffer().getPixel(5, 2).a != 0);
    const ImageBuffer firstPainted = first->buffer();

    assert(controller.undo());
    assert(first->buffer().pixels() == firstBefore.pixels());
    assert(controller.redo());
    assert(first->buffer().pixels() == firstPainted.pixels());

    const ImageBuffer firstCommitted = first->buffer();
    assert(controller.beginBrushStroke(1.5, 6.5));
    assert(controller.updateBrushStroke(6.5, 6.5));
    assert(controller.cancelBrushStroke());
    assert(first->buffer().pixels() == firstCommitted.pixels());

    controller.setLayerVisibility(firstId, false);
    const ImageBuffer hidden = first->buffer();
    assert(!controller.beginBrushStroke(3.5, 3.5));
    assert(!controller.isBrushStrokeActive());
    assert(first->buffer().pixels() == hidden.pixels());
    controller.setLayerVisibility(firstId, true);

    controller.addLayer();
    const LayerId topId = controller.selectedLayerId();
    assert(topId != firstId);
    Layer* top = controller.document().findLayer(topId);
    Layer* bottom = controller.document().findLayer(firstId);
    assert(top != nullptr);
    assert(bottom != nullptr);
    const ImageBuffer bottomBefore = bottom->buffer();
    const ImageBuffer topBefore = top->buffer();

    assert(controller.beginBrushStroke(4.5, 4.5));
    assert(controller.updateBrushStroke(5.5, 5.5));
    assert(controller.endBrushStroke());
    assert(bottom->buffer().pixels() == bottomBefore.pixels());
    assert(top->buffer().pixels() != topBefore.pixels());
}

void testCanvasDocumentMapping() {
    CanvasView view;
    view.resize(400, 300);

    QImage image(100, 80, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    view.setImage(image);
    view.setZoom(2.0);
    view.centerOn(50.0, 40.0);

    const QPointF scenePoint(20.5, 30.25);
    const QPointF viewportPoint = view.viewportTransform().map(scenePoint);
    const QPointF mapped = view.documentPosition(viewportPoint);
    assert(std::abs(mapped.x() - scenePoint.x()) < 1e-6);
    assert(std::abs(mapped.y() - scenePoint.y()) < 1e-6);
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

void testSelectionToolControls() {
    MainWindow window;
    auto* rectSel = window.findChild<QAction*>(
        QStringLiteral("actionToolRectangleSelect"));
    auto* ellipseSel = window.findChild<QAction*>(
        QStringLiteral("actionToolEllipseSelect"));
    auto* bucket = window.findChild<QAction*>(QStringLiteral("actionToolBucketFill"));
    auto* clearSel = window.findChild<QAction*>(QStringLiteral("actionSelectClear"));
    auto* invertSel = window.findChild<QAction*>(QStringLiteral("actionSelectInvert"));
    assert(rectSel != nullptr);
    assert(ellipseSel != nullptr);
    assert(bucket != nullptr);
    assert(clearSel != nullptr);
    assert(invertSel != nullptr);

    assert(rectSel->isCheckable());
    assert(ellipseSel->isCheckable());
    assert(bucket->isCheckable());
    assert(!rectSel->isChecked());
    assert(!ellipseSel->isChecked());
    assert(!bucket->isChecked());
    assert(window.controller().selection().isEmpty());

    auto* canvas = window.findChild<CanvasView*>(QStringLiteral("canvasView"));
    assert(canvas != nullptr);

    rectSel->trigger();
    QCoreApplication::processEvents();
    assert(rectSel->isChecked());
    assert(canvas->activeTool() == CanvasView::Tool::SelectRectangle);

    ellipseSel->trigger();
    QCoreApplication::processEvents();
    assert(ellipseSel->isChecked());
    assert(!rectSel->isChecked());
    assert(canvas->activeTool() == CanvasView::Tool::SelectEllipse);

    bucket->trigger();
    QCoreApplication::processEvents();
    assert(bucket->isChecked());
    assert(!ellipseSel->isChecked());
    assert(canvas->activeTool() == CanvasView::Tool::BucketFill);

    // Clear / invert are immediate (non-exclusive) actions.
    clearSel->trigger();
    QCoreApplication::processEvents();
    assert(window.controller().document().selection().isEmpty());

    invertSel->trigger();
    QCoreApplication::processEvents();
    assert(!window.controller().document().selection().isEmpty());
    double left = 0, top = 0, right = 0, bottom = 0;
    assert(window.controller().document().selection().bounds(
               left, top, right, bottom));
    const Document& document = window.controller().document();
    assert(std::abs(right - document.width()) < 1e-9);
    assert(std::abs(bottom - document.height()) < 1e-9);
}

void testControllerSelectionIntegration() {
    AppController controller;
    assert(controller.newDocument(QStringLiteral("Sel"), 8, 8));
    assert(controller.selection().isEmpty());
    assert(!controller.isSelecting());

    controller.beginSelection(Selection::Type::Rectangle, 2.0, 2.0);
    assert(controller.isSelecting());
    assert(controller.selectionType() == Selection::Type::Rectangle);

    controller.updateSelection(5.0, 5.0);
    const Selection& live = controller.selection();
    assert(!live.isEmpty());
    double left = 0, top = 0, right = 0, bottom = 0;
    assert(live.bounds(left, top, right, bottom));
    assert(std::abs(left - 2.0) < 1e-9);
    assert(std::abs(top - 2.0) < 1e-9);
    assert(std::abs(right - 5.0) < 1e-9);
    assert(std::abs(bottom - 5.0) < 1e-9);

    // Committing pushes one undoable command.
    controller.endSelection();
    assert(!controller.isSelecting());
    assert(controller.canUndo());
    const Selection& committed = controller.document().selection();
    assert(!committed.isEmpty());
    assert(committed.bounds(left, top, right, bottom));
    assert(std::abs(right - 5.0) < 1e-9);

    assert(controller.undo());
    assert(controller.document().selection().isEmpty());

    assert(controller.redo());
    assert(!controller.document().selection().isEmpty());
    assert(controller.document().selection().bounds(left, top, right, bottom));
    assert(std::abs(right - 5.0) < 1e-9);

    // Reverse-direction drag normalizes the geometry.
    controller.beginSelection(Selection::Type::Rectangle, 7.0, 7.0);
    controller.updateSelection(1.0, 1.0);
    controller.endSelection();
    assert(controller.document().selection().bounds(left, top, right, bottom));
    assert(std::abs(left - 1.0) < 1e-9);
    assert(std::abs(top - 1.0) < 1e-9);
    assert(std::abs(right - 7.0) < 1e-9);
    assert(std::abs(bottom - 7.0) < 1e-9);
}

void testBucketFillIntegration() {
    AppController controller;
    assert(controller.newDocument(QStringLiteral("Fill"), 4, 4));
    const LayerId id = controller.selectedLayerId();
    Layer* layer = controller.document().findLayer(id);
    assert(layer != nullptr);
    layer->buffer().clear(Pixel{100, 100, 100, 255});

    FillSettings settings;
    settings.color = Pixel{255, 255, 255, 255};
    settings.opacity = 1.0F;
    settings.tolerance = 0;
    assert(controller.setFillSettings(settings));

    assert(controller.bucketFill(2.0, 2.0));
    assert(controller.canUndo());
    assert(layer->buffer().getPixel(2, 2) == (Pixel{255, 255, 255, 255}));

    assert(controller.undo());
    assert(layer->buffer().getPixel(2, 2) == (Pixel{100, 100, 100, 255}));

    assert(controller.redo());
    assert(layer->buffer().getPixel(2, 2) == (Pixel{255, 255, 255, 255}));

    // With an active selection, only selected pixels are filled.
    layer->buffer().clear(Pixel{100, 100, 100, 255});
    controller.beginSelection(Selection::Type::Rectangle, 0.0, 0.0);
    controller.updateSelection(2.0, 2.0);
    controller.endSelection();
    assert(controller.document().selection().pixelSelected(1, 1));
    assert(!controller.document().selection().pixelSelected(3, 3));

    assert(controller.bucketFill(1.0, 1.0));
    assert(layer->buffer().getPixel(1, 1) == (Pixel{255, 255, 255, 255}));
    assert(layer->buffer().getPixel(3, 3) == (Pixel{100, 100, 100, 255}));
}

void testSelectionOverlayDoesNotModifyPixels() {
    MainWindow window;
    auto* canvas = window.findChild<CanvasView*>(QStringLiteral("canvasView"));
    assert(canvas != nullptr);

    const Document& document = window.controller().document();
    const auto before =
        document.layerStack().layers().front().buffer().pixels();

    Selection selection;
    selection.setDocumentSize(8, 8);
    selection.setRectangle(1.0, 1.0, 4.0, 4.0);
    canvas->setSelection(selection);

    assert(!selection.isEmpty());
    assert(document.layerStack().layers().front().buffer().pixels() == before);
}

void testMainWindowAdjustmentAction() {
    MainWindow window;
    auto* action = window.findChild<QAction*>(QStringLiteral("actionImageAdjustments"));
    assert(action != nullptr);
    assert(action->isEnabled());
    assert(!action->shortcut().isEmpty());
}

void testAdjustmentDialogControls() {
    AdjustmentDialog dialog;

    auto* brightness = dialog.findChild<QSlider*>(QStringLiteral("adjustBrightnessSlider"));
    auto* blur = dialog.findChild<QSlider*>(QStringLiteral("adjustBlurSlider"));
    auto* grayscale = dialog.findChild<QCheckBox*>(QStringLiteral("adjustGrayscaleCheck"));
    assert(brightness != nullptr);
    assert(blur != nullptr);
    assert(grayscale != nullptr);

    int emitted = 0;
    AdjustmentSettings last;
    QObject::connect(
        &dialog, &AdjustmentDialog::settingsChanged,
        [&](const AdjustmentSettings& settings) {
            ++emitted;
            last = settings;
        }
    );

    assert(dialog.settings().isDefault());

    brightness->setValue(50);
    assert(emitted == 1);
    assert(std::abs(last.brightness - 0.5F) < 1e-6F);

    grayscale->setChecked(true);
    assert(emitted == 2);
    assert(last.grayscale);

    // setSettings must not re-emit (signals blocked during programmatic update).
    AdjustmentSettings custom = dialog.settings();
    dialog.setSettings(custom);
    assert(emitted == 2);
    assert(dialog.settings().grayscale);

    // Programmatic reset also stays silent; state returns to defaults.
    dialog.setSettings(AdjustmentSettings{});
    assert(emitted == 2);
    assert(dialog.settings().isDefault());
}

void testControllerAdjustmentIntegration() {
    AppController controller;
    assert(controller.newDocument(QStringLiteral("Adjust"), 6, 6));
    const LayerId id = controller.selectedLayerId();
    Layer* layer = controller.document().findLayer(id);
    assert(layer != nullptr);
    layer->buffer().clear(Pixel{100, 100, 100, 255});
    assert(!controller.canUndo());

    assert(controller.beginAdjustment());
    assert(controller.isAdjusting());
    assert(layer->buffer().getPixel(0, 0) == (Pixel{100, 100, 100, 255}));

    AdjustmentSettings settings;
    settings.brightness = 0.5F;
    assert(controller.updateAdjustment(settings));
    assert(controller.adjustmentSettings().brightness == settings.brightness);
    // Preview must not push history.
    assert(!controller.canUndo());
    const Pixel preview = layer->buffer().getPixel(0, 0);
    assert(preview != (Pixel{100, 100, 100, 255}));

    assert(controller.applyAdjustment());
    assert(!controller.isAdjusting());
    assert(controller.canUndo());
    assert(layer->buffer().getPixel(0, 0) == preview);

    assert(controller.undo());
    assert(layer->buffer().getPixel(0, 0) == (Pixel{100, 100, 100, 255}));

    assert(controller.redo());
    assert(layer->buffer().getPixel(0, 0) == preview);
}

void testControllerAdjustmentCancelRestores() {
    AppController controller;
    assert(controller.newDocument(QStringLiteral("Cancel"), 4, 4));
    const LayerId id = controller.selectedLayerId();
    Layer* layer = controller.document().findLayer(id);
    assert(layer != nullptr);
    layer->buffer().clear(Pixel{40, 80, 120, 255});

    assert(controller.beginAdjustment());
    AdjustmentSettings settings;
    settings.saturation = -1.0F;
    settings.blurSigma = 2.0F;
    assert(controller.updateAdjustment(settings));
    assert(layer->buffer().getPixel(1, 1) != (Pixel{40, 80, 120, 255}));

    controller.cancelAdjustment();
    assert(!controller.isAdjusting());
    assert(layer->buffer().getPixel(1, 1) == (Pixel{40, 80, 120, 255}));
    assert(!controller.canUndo());

    // Identity settings produce no command on apply.
    assert(controller.beginAdjustment());
    assert(controller.updateAdjustment(AdjustmentSettings{}));
    assert(!controller.applyAdjustment());
    assert(!controller.canUndo());
}

void testControllerTransformIntegration() {
    AppController controller;
    assert(controller.newDocument(QStringLiteral("Transform"), 8, 6));
    const LayerId id = controller.selectedLayerId();
    Layer* layer = controller.document().findLayer(id);
    assert(layer != nullptr);
    layer->buffer().clear(Pixel{30, 60, 90, 255});

    // Rotating the canvas 90 degrees clockwise swaps its dimensions and is
    // recorded as a single undoable command.
    assert(controller.rotateCanvas90(true));
    assert(controller.document().width() == 6);
    assert(controller.document().height() == 8);
    assert(controller.undo());
    assert(controller.document().width() == 8);
    assert(controller.document().height() == 6);
    assert(controller.redo());
    assert(controller.document().width() == 6);

    assert(controller.flipCanvas(true));
    assert(controller.canUndo());

    assert(controller.resizeCanvas(12, 10, glyma::core::Anchor::Center));
    assert(controller.document().width() == 12);
    assert(controller.document().height() == 10);

    assert(controller.resizeImage(6, 5));
    assert(controller.document().width() == 6);
    assert(controller.document().height() == 5);

    assert(controller.crop(glyma::core::CropRect{1, 1, 4, 3}));
    assert(controller.document().width() == 4);
    assert(controller.document().height() == 3);

    // Layer-level transforms stay undoable.
    assert(controller.translateLayer(id, 2, 1));
    assert(controller.scaleLayer(id, 3, 2));
    assert(controller.transformLayer(
        id, glyma::core::LayerTransform::Rotate90Clockwise));
    assert(controller.rotateLayer(id, 45.0));
    assert(controller.document().findLayer(id) != nullptr);

    // Invalid requests are rejected without adding history entries.
    assert(!controller.crop(glyma::core::CropRect{0, 0, 0, 0}));
    assert(!controller.translateLayer(id, 0, 0));
    assert(!controller.rotateCanvas(0.0));
    const std::uint32_t width = controller.document().width();
    const std::uint32_t height = controller.document().height();
    assert(!controller.resizeCanvas(
        width, height, glyma::core::Anchor::Center));
    assert(!controller.transformLayer(999999, glyma::core::LayerTransform::Rotate180));
    assert(controller.document().width() == width);
    assert(controller.document().height() == height);
}

void testMainWindowTransformActions() {
    MainWindow window;

    const char* names[] = {
        "actionImageCrop",
        "actionImageCropApply",
        "actionImageCropCancel",
        "actionCanvasSize",
        "actionImageResize",
        "actionImageRotateCW",
        "actionImageRotateCCW",
        "actionImageRotate180",
        "actionImageRotateFree",
        "actionImageFlipHorizontal",
        "actionImageFlipVertical",
        "actionLayerRotateCW",
        "actionLayerRotateCCW",
        "actionLayerRotate180",
        "actionLayerRotateFree",
        "actionLayerFlipHorizontal",
        "actionLayerFlipVertical",
        "actionLayerScale",
        "actionLayerMove",
    };
    for (const char* name : names) {
        assert(window.findChild<QAction*>(QString::fromLatin1(name)) != nullptr);
    }

    auto* crop = window.findChild<QAction*>(QStringLiteral("actionImageCrop"));
    assert(crop->isCheckable());

    AppController& controller = window.controller();
    const std::uint32_t width = controller.document().width();
    const std::uint32_t height = controller.document().height();

    auto* rotateCW =
        window.findChild<QAction*>(QStringLiteral("actionImageRotateCW"));
    rotateCW->trigger();
    assert(controller.document().width() == height);
    assert(controller.document().height() == width);
    assert(controller.canUndo());

    auto* flip =
        window.findChild<QAction*>(QStringLiteral("actionImageFlipVertical"));
    flip->trigger();
    assert(controller.canUndo());

    auto* canvas = window.findChild<CanvasView*>(QStringLiteral("canvasView"));
    assert(canvas != nullptr);
    canvas->setCropRect(QRectF(1.0, 1.0, 4.0, 4.0));
    assert(canvas->hasCrop());

    // The Apply action is wired to the canvas request signal; emitting the
    // signal exercises the same path as clicking the toolbar button.
    assert(QMetaObject::invokeMethod(
        canvas, "cropApplyRequested", Qt::DirectConnection));
    assert(!canvas->hasCrop());
    assert(controller.document().width() == 4);
    assert(controller.document().height() == 4);
}

std::size_t opaquePixelCount(const ImageBuffer& buffer) {
    std::size_t count = 0;
    for (std::uint32_t y = 0; y < buffer.height(); ++y) {
        for (std::uint32_t x = 0; x < buffer.width(); ++x) {
            if (buffer.getPixel(x, y).a > 0) {
                ++count;
            }
        }
    }
    return count;
}

void testTextRendererLayout() {
    // Auto-sized text measures a positive box and rasterizes visible glyphs.
    TextLayerData autoData;
    autoData.text = "Hi";
    autoData.style.fontSize = 32.0F;
    const auto autoResult = TextRenderer::render(autoData);
    assert(autoResult.boxWidth > 1);
    assert(autoResult.boxHeight > 1);
    assert(autoResult.image.width() == autoResult.boxWidth);
    assert(autoResult.image.height() == autoResult.boxHeight);
    assert(opaquePixelCount(autoResult.image) > 0);

    // Multiple lines are taller than a single line.
    TextLayerData multi = autoData;
    multi.text = "Hi\nThere";
    const auto multiResult = TextRenderer::render(multi);
    assert(multiResult.boxHeight > autoResult.boxHeight);
    assert(opaquePixelCount(multiResult.image) > opaquePixelCount(autoResult.image));

    // A fixed box is honoured exactly.
    TextLayerData fixed;
    fixed.text = "Boxed";
    fixed.autoSize = false;
    fixed.boxWidth = 64;
    fixed.boxHeight = 40;
    const auto fixedResult = TextRenderer::render(fixed);
    assert(fixedResult.boxWidth == 64);
    assert(fixedResult.boxHeight == 40);
    assert(fixedResult.image.width() == 64);
    assert(fixedResult.image.height() == 40);

    // Empty text still yields a valid 1x1-or-larger buffer with no glyphs.
    TextLayerData empty;
    empty.text.clear();
    const auto emptyResult = TextRenderer::render(empty);
    assert(emptyResult.boxWidth >= 1);
    assert(emptyResult.boxHeight >= 1);
    assert(opaquePixelCount(emptyResult.image) == 0);
}

void testTextRendererHorizontalPosition() {
    // Verify right-aligned text sits further right than left-aligned text.
    TextLayerData data;
    data.text = "I";
    data.autoSize = false;
    data.boxWidth = 200;
    data.boxHeight = 40;
    data.style.fontSize = 24.0F;

    data.style.horizontalAlign = TextHorizontalAlign::Left;
    const auto left = TextRenderer::render(data);
    data.style.horizontalAlign = TextHorizontalAlign::Right;
    const auto right = TextRenderer::render(data);

    const auto centreOfMass = [](const ImageBuffer& image) {
        double sum = 0.0;
        double weighted = 0.0;
        for (std::uint32_t x = 0; x < image.width(); ++x) {
            for (std::uint32_t y = 0; y < image.height(); ++y) {
                const double alpha = image.getPixel(x, y).a;
                sum += alpha;
                weighted += alpha * static_cast<double>(x);
            }
        }
        return sum > 0.0 ? weighted / sum : 0.0;
    };

    assert(centreOfMass(right.image) > centreOfMass(left.image) + 1.0);
}

void testControllerTextIntegration() {
    AppController controller;
    assert(controller.newDocument(QStringLiteral("Text"), 48, 48));
    assert(controller.document().layerCount() == 1);

    TextLayerData data;
    data.text = "Hi";
    data.style.fontSize = 28.0F;
    assert(controller.addTextLayer(data, 4, 5));
    assert(controller.document().layerCount() == 2);

    const LayerId id = controller.selectedLayerId();
    const Layer* layer = controller.document().findLayer(id);
    assert(layer != nullptr);
    assert(layer->isText());
    assert(layer->x() == 4 && layer->y() == 5);

    const TextLayerData* stored = controller.textData(id);
    assert(stored != nullptr);
    assert(stored->text == "Hi");

    // Pixel tools skip text layers (their cache is renderer-owned).
    assert(!controller.beginBrushStroke(6.0, 6.0));
    assert(!controller.bucketFill(6.0, 6.0));

    // The controller refreshes the render cache, so compositing shows glyphs.
    const ImageBuffer rendered = controller.render();
    assert(opaquePixelCount(rendered) > 0);

    // Editing typography is undoable.
    TextLayerData edited = *stored;
    edited.style.fontSize = 40.0F;
    edited.text = "Hello";
    assert(controller.setTextData(id, edited));
    assert(controller.textData(id)->style.fontSize == 40.0F);
    assert(controller.undo());
    assert(controller.textData(id)->text == "Hi");
    assert(controller.redo());
    assert(controller.textData(id)->text == "Hello");

    // Rasterizing drops editability but keeps the rendered pixels.
    assert(controller.rasterizeTextLayer(id));
    assert(controller.textData(id) == nullptr);
    assert(!controller.document().findLayer(id)->isText());
    assert(controller.undo());
    assert(controller.document().findLayer(id)->isText());

    // Invalid payloads are rejected without touching history.
    TextLayerData invalid;
    invalid.style.fontSize = 0.0F;
    assert(!controller.addTextLayer(invalid));
    assert(!controller.setTextData(id, invalid));

    // Undo the typography edit, then the original add.
    assert(controller.undo());
    assert(controller.undo());
    assert(controller.document().layerCount() == 1);
}

void testMainWindowTextTool() {
    MainWindow window;
    auto* textAction =
        window.findChild<QAction*>(QStringLiteral("actionToolText"));
    assert(textAction != nullptr);
    assert(textAction->isCheckable());

    auto* canvas = window.findChild<CanvasView*>(QStringLiteral("canvasView"));
    assert(canvas != nullptr);

    textAction->trigger();
    QCoreApplication::processEvents();
    assert(textAction->isChecked());
    assert(canvas->activeTool() == CanvasView::Tool::Text);

    auto* panel =
        window.findChild<TypographyPanel*>(QStringLiteral("typographyPanel"));
    assert(panel != nullptr);
    auto* content =
        window.findChild<QPlainTextEdit*>(QStringLiteral("textContentEdit"));
    assert(content != nullptr);

    AppController& controller = window.controller();
    const std::size_t before = controller.document().layerCount();

    // Placing text creates a layer at the clicked document position.
    assert(QMetaObject::invokeMethod(
        canvas, "textPlacementRequested", Qt::DirectConnection,
        Q_ARG(QPointF, QPointF(12.0, 20.0))));
    QCoreApplication::processEvents();
    assert(controller.document().layerCount() == before + 1);

    const LayerId id = controller.selectedLayerId();
    assert(controller.textData(id) != nullptr);
    assert(content->isEnabled());
    assert(content->toPlainText() == QStringLiteral("Text"));

    // Editing the content drives the controller through the panel signal.
    content->setPlainText(QStringLiteral("Caption"));
    QCoreApplication::processEvents();
    assert(controller.textData(id) != nullptr);
    assert(controller.textData(id)->text == "Caption");

    // The rasterize button converts the layer.
    auto* rasterize =
        window.findChild<QPushButton*>(QStringLiteral("textRasterizeButton"));
    assert(rasterize != nullptr);
    rasterize->click();
    QCoreApplication::processEvents();
    assert(!controller.document().findLayer(id)->isText());
}

void testShapeRendererProducesPixels() {
    ShapeLayerData shape;
    shape.type = ShapeType::Rectangle;
    glyma::core::setRectGeometry(shape, 5.0, 5.0, 20.0, 20.0);

    const glyma::app::ShapeRenderResult result = ShapeRenderer::render(shape);
    assert(result.image.width() > 1 && result.image.height() > 1);
    assert(opaquePixelCount(result.image) > 0);

    // Visual bounds include the stroke padding.
    assert(result.bounds.width > 20.0 && result.bounds.height > 20.0);
}

void testControllerShapeIntegration() {
    AppController controller;
    assert(controller.newDocument(QStringLiteral("Shapes"), 48, 48));
    assert(controller.document().layerCount() == 1);

    ShapeLayerData shape;
    shape.type = ShapeType::Ellipse;
    glyma::core::setRectGeometry(shape, 6.0, 6.0, 20.0, 20.0);
    assert(controller.addShapeLayer(shape));
    assert(controller.document().layerCount() == 2);

    const LayerId id = controller.selectedLayerId();
    const Layer* layer = controller.document().findLayer(id);
    assert(layer != nullptr);
    assert(layer->isVector());

    const ShapeLayerData* stored = controller.shapeData(id);
    assert(stored != nullptr);
    assert(stored->type == ShapeType::Ellipse);

    // Pixel tools skip vector layers (their cache is renderer-owned).
    assert(!controller.beginBrushStroke(10.0, 10.0));
    assert(!controller.bucketFill(10.0, 10.0));

    // The controller refreshes the render cache, so compositing shows the fill.
    const ImageBuffer rendered = controller.render();
    assert(opaquePixelCount(rendered) > 0);

    // Hit testing finds the shape.
    assert(controller.shapeLayerAt(16.0, 16.0, 0.0) == id);
    assert(controller.shapeLayerAt(100.0, 100.0, 0.0) == glyma::core::Layer::invalidId);

    // Editing the payload is undoable.
    ShapeLayerData edited = *stored;
    edited.rotation = 45.0;
    assert(controller.setShapeData(id, edited));
    assert(std::abs(controller.shapeData(id)->rotation - 45.0) < 1.0e-6);
    assert(controller.undo());
    assert(std::abs(controller.shapeData(id)->rotation) < 1.0e-6);
    assert(controller.redo());
    assert(std::abs(controller.shapeData(id)->rotation - 45.0) < 1.0e-6);

    // Rasterizing drops editability but keeps the rendered pixels.
    assert(controller.rasterizeShapeLayer(id));
    assert(controller.shapeData(id) == nullptr);
    assert(!controller.document().findLayer(id)->isVector());
    assert(controller.undo());
    assert(controller.document().findLayer(id)->isVector());

    // Invalid payloads are rejected without touching history.
    ShapeLayerData invalid;
    invalid.stroke.width = -1.0F;
    assert(!controller.addShapeLayer(invalid));
    assert(!controller.setShapeData(id, invalid));

    // Undo the edit, then the original add.
    assert(controller.undo());
    assert(controller.undo());
    assert(controller.document().layerCount() == 1);
}

void testMainWindowShapeTool() {
    MainWindow window;
    auto* shapeAction =
        window.findChild<QAction*>(QStringLiteral("actionToolShapeRectangle"));
    assert(shapeAction != nullptr);
    assert(shapeAction->isCheckable());

    auto* canvas = window.findChild<CanvasView*>(QStringLiteral("canvasView"));
    assert(canvas != nullptr);

    shapeAction->trigger();
    QCoreApplication::processEvents();
    assert(shapeAction->isChecked());
    assert(canvas->activeTool() == CanvasView::Tool::ShapeRectangle);

    auto* panel = window.findChild<ShapePropertiesPanel*>(
        QStringLiteral("shapePropertiesPanel")
    );
    assert(panel != nullptr);
    auto* rotation =
        window.findChild<QDoubleSpinBox*>(QStringLiteral("shapeRotationSpin"));
    assert(rotation != nullptr);

    AppController& controller = window.controller();
    const std::size_t before = controller.document().layerCount();

    ShapeLayerData shape;
    shape.type = ShapeType::Rectangle;
    glyma::core::setRectGeometry(shape, 4.0, 4.0, 20.0, 16.0);
    assert(QMetaObject::invokeMethod(
        canvas, "shapeCreationRequested", Qt::DirectConnection,
        Q_ARG(glyma::core::ShapeLayerData, shape)));
    QCoreApplication::processEvents();
    assert(controller.document().layerCount() == before + 1);

    const LayerId id = controller.selectedLayerId();
    assert(controller.shapeData(id) != nullptr);
    assert(rotation->isEnabled());

    // Editing the rotation drives the controller through the panel signal.
    rotation->setValue(45.0);
    QCoreApplication::processEvents();
    assert(controller.shapeData(id) != nullptr);
    assert(std::abs(controller.shapeData(id)->rotation - 45.0) < 1.0e-6);

    // The rasterize button converts the layer.
    auto* rasterize =
        window.findChild<QPushButton*>(QStringLiteral("shapeRasterizeButton"));
    assert(rasterize != nullptr);
    rasterize->click();
    QCoreApplication::processEvents();
    assert(!controller.document().findLayer(id)->isVector());
}

void testIconIntegration() {
    MainWindow window;

    const QStringList toolActions{
        QStringLiteral("actionToolBrush"),
        QStringLiteral("actionToolEraser"),
        QStringLiteral("actionToolRectangleSelect"),
        QStringLiteral("actionToolEllipseSelect"),
        QStringLiteral("actionToolBucketFill"),
        QStringLiteral("actionToolText"),
        QStringLiteral("actionToolShapeRectangle"),
        QStringLiteral("actionToolShapeRoundedRectangle"),
        QStringLiteral("actionToolShapeEllipse"),
        QStringLiteral("actionToolShapeLine"),
        QStringLiteral("actionToolShapeArrow"),
        QStringLiteral("actionToolShapePolygon"),
        QStringLiteral("actionImageCrop"),
        QStringLiteral("actionSelectClear"),
        QStringLiteral("actionSelectInvert"),
    };
    for (const QString& name : toolActions) {
        auto* action = window.findChild<QAction*>(name);
        assert(action != nullptr);
        assert(!action->icon().isNull());
        assert(!action->toolTip().isEmpty());
    }

    const QStringList fileEditActions{
        QStringLiteral("actionFileNew"),
        QStringLiteral("actionFileOpen"),
        QStringLiteral("actionFileSave"),
        QStringLiteral("actionFileSaveAs"),
        QStringLiteral("actionFileExportPng"),
        QStringLiteral("actionFileExit"),
        QStringLiteral("actionEditUndo"),
        QStringLiteral("actionEditRedo"),
        QStringLiteral("actionViewZoomIn"),
        QStringLiteral("actionViewZoomOut"),
        QStringLiteral("actionViewFit"),
        QStringLiteral("actionViewResetZoom"),
        QStringLiteral("actionImageAdjustments"),
        QStringLiteral("actionHelpAbout"),
    };
    for (const QString& name : fileEditActions) {
        auto* action = window.findChild<QAction*>(name);
        assert(action != nullptr);
        assert(!action->icon().isNull());
    }

    auto* mainToolBar = window.findChild<QToolBar*>(QStringLiteral("mainToolBar"));
    auto* toolsToolBar = window.findChild<QToolBar*>(QStringLiteral("toolsToolBar"));
    assert(mainToolBar != nullptr);
    assert(toolsToolBar != nullptr);
    assert(mainToolBar->iconSize() == IconManager::toolbarIconSize());
    assert(toolsToolBar->iconSize() == IconManager::toolbarIconSize());

    for (const QString& name : {
             QStringLiteral("layerAddButton"),
             QStringLiteral("layerDeleteButton"),
             QStringLiteral("layerUpButton"),
             QStringLiteral("layerDownButton"),
         }) {
        auto* button = window.findChild<QToolButton*>(name);
        assert(button != nullptr);
        assert(!button->icon().isNull());
        assert(button->iconSize() == IconManager::panelIconSize());
    }

    assert(!window.windowIcon().isNull());

    // Rendering the tool bar must actually paint the icon pixels.
    const QPixmap rendered = toolsToolBar->grab();
    const QImage image = rendered.toImage();
    int opaque = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                ++opaque;
            }
        }
    }
    assert(opaque > 0);
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

    qRegisterMetaType<glyma::core::ShapeLayerData>(
        "glyma::core::ShapeLayerData"
    );

    testBufferConversion();
    testConversionOwnsPixels();
    testFitCalculations();
    testZoomLimits();
    testCanvasZoomAndFit();
    testMainWindowConstruction();
    testNewDocumentOptions();
    testModifiedStateTracking();
    testLayerManagement();
    testBrushToolControls();
    testControllerBrushIntegration();
    testCanvasDocumentMapping();
    testSelectionToolControls();
    testControllerSelectionIntegration();
    testBucketFillIntegration();
    testSelectionOverlayDoesNotModifyPixels();
    testMainWindowAdjustmentAction();
    testAdjustmentDialogControls();
    testControllerAdjustmentIntegration();
    testControllerAdjustmentCancelRestores();
    testControllerTransformIntegration();
    testMainWindowTransformActions();
    testTextRendererLayout();
    testTextRendererHorizontalPosition();
    testControllerTextIntegration();
    testMainWindowTextTool();
    testShapeRendererProducesPixels();
    testControllerShapeIntegration();
    testMainWindowShapeTool();
    testIconIntegration();
    testControllerFailurePaths();
    testControllerFileOperations();

    std::cout << "All Glyma UI tests passed\n";
    return 0;
}
