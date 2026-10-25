#include "glyma/app/main_window.hpp"

#include <cmath>
#include <cstdint>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>

#include "glyma/app/adjustment_dialog.hpp"
#include "glyma/app/canvas_view.hpp"
#include "glyma/app/layer_dock.hpp"
#include "glyma/app/qt_image.hpp"
#include "glyma/core/transform.hpp"

namespace glyma::app {
namespace {

QAction* makeAction(
    QObject* parent,
    const QString& text,
    const QKeySequence& shortcut,
    const QString& objectName
) {
    auto* action = new QAction(text, parent);
    action->setObjectName(objectName);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    return action;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setObjectName(QStringLiteral("mainWindow"));
    resize(1280, 800);
    setMinimumSize(900, 600);

    canvas_ = new CanvasView(this);
    setCentralWidget(canvas_);

    createActions();
    createMenus();
    createToolBar();
    createStatusBar();
    createToolOptions();
    createDocks();
    connectSignals();
    applyStyle();

    refreshCanvas();
    refreshLayers();
    refreshStatus();
    updateActions();
    updateTitle();
}

AppController& MainWindow::controller() noexcept {
    return controller_;
}

void MainWindow::createActions() {
    newAction_ = makeAction(
        this, tr("&New..."), QKeySequence::New, QStringLiteral("actionFileNew")
    );
    openAction_ = makeAction(
        this, tr("&Open..."), QKeySequence::Open, QStringLiteral("actionFileOpen")
    );
    saveAction_ = makeAction(
        this, tr("&Save"), QKeySequence::Save, QStringLiteral("actionFileSave")
    );
    saveAsAction_ = makeAction(
        this,
        tr("Save &As..."),
        QKeySequence::SaveAs,
        QStringLiteral("actionFileSaveAs")
    );
    exportPngAction_ = makeAction(
        this,
        tr("&Export PNG..."),
        QKeySequence(QStringLiteral("Ctrl+Shift+E")),
        QStringLiteral("actionFileExportPng")
    );
    exitAction_ = makeAction(
        this, tr("E&xit"), QKeySequence::Quit, QStringLiteral("actionFileExit")
    );

    undoAction_ = makeAction(
        this, tr("&Undo"), QKeySequence::Undo, QStringLiteral("actionEditUndo")
    );
    redoAction_ = makeAction(
        this, tr("&Redo"), QKeySequence::Redo, QStringLiteral("actionEditRedo")
    );

    cropAction_ = makeAction(
        this, tr("&Crop"), QKeySequence(QStringLiteral("C")),
        QStringLiteral("actionImageCrop")
    );
    cropApplyAction_ = makeAction(
        this, tr("&Apply Crop"),
        QKeySequence(QStringLiteral("Ctrl+Return")),
        QStringLiteral("actionImageCropApply")
    );
    cropCancelAction_ = makeAction(
        this, tr("Cancel Crop"), QKeySequence(),
        QStringLiteral("actionImageCropCancel")
    );
    imageSizeAction_ = makeAction(
        this, tr("&Image Size..."), QKeySequence(QStringLiteral("Ctrl+Alt+I")),
        QStringLiteral("actionImageResize")
    );
    canvasSizeAction_ = makeAction(
        this, tr("&Canvas Size..."), QKeySequence(QStringLiteral("Ctrl+Alt+C")),
        QStringLiteral("actionCanvasSize")
    );
    rotateCanvasCWAction_ = makeAction(
        this, tr("Rotate Canvas 90\u00b0 Clockwise"), QKeySequence(),
        QStringLiteral("actionImageRotateCW")
    );
    rotateCanvasCCWAction_ = makeAction(
        this, tr("Rotate Canvas 90\u00b0 Counterclockwise"), QKeySequence(),
        QStringLiteral("actionImageRotateCCW")
    );
    rotateCanvas180Action_ = makeAction(
        this, tr("Rotate Canvas 180\u00b0"), QKeySequence(),
        QStringLiteral("actionImageRotate180")
    );
    rotateCanvasFreeAction_ = makeAction(
        this, tr("Rotate Canvas by Angle..."), QKeySequence(),
        QStringLiteral("actionImageRotateFree")
    );
    flipCanvasHorizontalAction_ = makeAction(
        this, tr("Flip Canvas Horizontal"), QKeySequence(),
        QStringLiteral("actionImageFlipHorizontal")
    );
    flipCanvasVerticalAction_ = makeAction(
        this, tr("Flip Canvas Vertical"), QKeySequence(),
        QStringLiteral("actionImageFlipVertical")
    );

    rotateLayerCWAction_ = makeAction(
        this, tr("Rotate Layer 90\u00b0 Clockwise"), QKeySequence(),
        QStringLiteral("actionLayerRotateCW")
    );
    rotateLayerCCWAction_ = makeAction(
        this, tr("Rotate Layer 90\u00b0 Counterclockwise"), QKeySequence(),
        QStringLiteral("actionLayerRotateCCW")
    );
    rotateLayer180Action_ = makeAction(
        this, tr("Rotate Layer 180\u00b0"), QKeySequence(),
        QStringLiteral("actionLayerRotate180")
    );
    rotateLayerFreeAction_ = makeAction(
        this, tr("Rotate Layer by Angle..."), QKeySequence(),
        QStringLiteral("actionLayerRotateFree")
    );
    flipLayerHorizontalAction_ = makeAction(
        this, tr("Flip Layer Horizontal"), QKeySequence(),
        QStringLiteral("actionLayerFlipHorizontal")
    );
    flipLayerVerticalAction_ = makeAction(
        this, tr("Flip Layer Vertical"), QKeySequence(),
        QStringLiteral("actionLayerFlipVertical")
    );
    scaleLayerAction_ = makeAction(
        this, tr("Scale Layer..."), QKeySequence(),
        QStringLiteral("actionLayerScale")
    );
    translateLayerAction_ = makeAction(
        this, tr("Move Layer..."), QKeySequence(),
        QStringLiteral("actionLayerMove")
    );

    zoomInAction_ = makeAction(
        this,
        tr("Zoom &In"),
        QKeySequence::ZoomIn,
        QStringLiteral("actionViewZoomIn")
    );
    zoomOutAction_ = makeAction(
        this,
        tr("Zoom &Out"),
        QKeySequence::ZoomOut,
        QStringLiteral("actionViewZoomOut")
    );
    fitAction_ = makeAction(
        this,
        tr("&Fit Canvas"),
        QKeySequence(QStringLiteral("Ctrl+0")),
        QStringLiteral("actionViewFit")
    );
    resetZoomAction_ = makeAction(
        this,
        tr("&Reset Zoom"),
        QKeySequence(QStringLiteral("Ctrl+1")),
        QStringLiteral("actionViewResetZoom")
    );

    imageAdjustmentsAction_ = makeAction(
        this,
        tr("A&djustments..."),
        QKeySequence(QStringLiteral("Ctrl+M")),
        QStringLiteral("actionImageAdjustments")
    );

    aboutAction_ = makeAction(
        this, tr("&About Glyma"), QKeySequence(), QStringLiteral("actionHelpAbout")
    );

    newAction_->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    openAction_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    saveAction_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    undoAction_->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    redoAction_->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    exitAction_->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    cropAction_->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    cropApplyAction_->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    cropCancelAction_->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));

    connect(newAction_, &QAction::triggered, this, [this] { onNew(); });
    connect(openAction_, &QAction::triggered, this, [this] { onOpen(); });
    connect(saveAction_, &QAction::triggered, this, [this] { onSave(); });
    connect(saveAsAction_, &QAction::triggered, this, [this] { onSaveAs(); });
    connect(exportPngAction_, &QAction::triggered, this, [this] { onExportPng(); });
    connect(exitAction_, &QAction::triggered, this, &QWidget::close);
    connect(undoAction_, &QAction::triggered, this, [this] { (void)controller_.undo(); });
    connect(redoAction_, &QAction::triggered, this, [this] { (void)controller_.redo(); });
    connect(zoomInAction_, &QAction::triggered, canvas_, &CanvasView::zoomIn);
    connect(zoomOutAction_, &QAction::triggered, canvas_, &CanvasView::zoomOut);
    connect(fitAction_, &QAction::triggered, canvas_, &CanvasView::fitToWindow);
    connect(resetZoomAction_, &QAction::triggered, canvas_, &CanvasView::resetZoom);
    connect(imageAdjustmentsAction_, &QAction::triggered, this, [this] { onAdjustments(); });
    connect(aboutAction_, &QAction::triggered, this, [this] { onAbout(); });

    connect(cropApplyAction_, &QAction::triggered, this, [this] { onCropApply(); });
    connect(cropCancelAction_, &QAction::triggered, this, [this] { onCropCancel(); });
    connect(canvasSizeAction_, &QAction::triggered, this, [this] { onCanvasSize(); });
    connect(imageSizeAction_, &QAction::triggered, this, [this] { onImageSize(); });
    connect(rotateCanvasCWAction_, &QAction::triggered, this, [this] {
        onRotateCanvas90(true);
    });
    connect(rotateCanvasCCWAction_, &QAction::triggered, this, [this] {
        onRotateCanvas90(false);
    });
    connect(rotateCanvas180Action_, &QAction::triggered, this, [this] {
        onRotateCanvas180();
    });
    connect(rotateCanvasFreeAction_, &QAction::triggered, this, [this] {
        onArbitraryRotateCanvas();
    });
    connect(flipCanvasHorizontalAction_, &QAction::triggered, this, [this] {
        onFlipCanvas(true);
    });
    connect(flipCanvasVerticalAction_, &QAction::triggered, this, [this] {
        onFlipCanvas(false);
    });
    connect(rotateLayerCWAction_, &QAction::triggered, this, [this] {
        onRotateLayer(core::LayerTransform::Rotate90Clockwise);
    });
    connect(rotateLayerCCWAction_, &QAction::triggered, this, [this] {
        onRotateLayer(core::LayerTransform::Rotate90CounterClockwise);
    });
    connect(rotateLayer180Action_, &QAction::triggered, this, [this] {
        onRotateLayer(core::LayerTransform::Rotate180);
    });
    connect(rotateLayerFreeAction_, &QAction::triggered, this, [this] {
        onArbitraryRotateLayer();
    });
    connect(flipLayerHorizontalAction_, &QAction::triggered, this, [this] {
        onFlipLayer(true);
    });
    connect(flipLayerVerticalAction_, &QAction::triggered, this, [this] {
        onFlipLayer(false);
    });
    connect(scaleLayerAction_, &QAction::triggered, this, [this] { onScaleLayer(); });
    connect(translateLayerAction_, &QAction::triggered, this, [this] {
        onTranslateLayer();
    });
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction_);
    fileMenu->addAction(openAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAction_);
    fileMenu->addAction(saveAsAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(exportPngAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction_);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(undoAction_);
    editMenu->addAction(redoAction_);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(zoomInAction_);
    viewMenu->addAction(zoomOutAction_);
    viewMenu->addSeparator();
    viewMenu->addAction(fitAction_);
    viewMenu->addAction(resetZoomAction_);

    QMenu* imageMenu = menuBar()->addMenu(tr("&Image"));
    imageMenu->addAction(imageAdjustmentsAction_);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction_);
}

void MainWindow::createToolBar() {
    auto* toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(16, 16));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    toolBar->addAction(newAction_);
    toolBar->addAction(openAction_);
    toolBar->addAction(saveAction_);
    toolBar->addSeparator();
    toolBar->addAction(undoAction_);
    toolBar->addAction(redoAction_);
    toolBar->addSeparator();
    toolBar->addAction(zoomOutAction_);
    toolBar->addAction(zoomInAction_);
    toolBar->addAction(fitAction_);
}

void MainWindow::createToolOptions() {
    auto* toolBar = addToolBar(tr("Tools"));
    toolBar->setObjectName(QStringLiteral("toolsToolBar"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(16, 16));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    toolGroup_ = new QActionGroup(this);
    toolGroup_->setExclusive(true);

    brushToolAction_ = makeAction(
        this, tr("&Brush"), QKeySequence(QStringLiteral("B")),
        QStringLiteral("actionToolBrush")
    );
    brushToolAction_->setCheckable(true);
    brushToolAction_->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));

    eraserToolAction_ = makeAction(
        this, tr("&Eraser"), QKeySequence(QStringLiteral("E")),
        QStringLiteral("actionToolEraser")
    );
    eraserToolAction_->setCheckable(true);
    eraserToolAction_->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));

    toolGroup_->addAction(brushToolAction_);
    toolGroup_->addAction(eraserToolAction_);
    toolBar->addAction(brushToolAction_);
    toolBar->addAction(eraserToolAction_);
    toolBar->addSeparator();

    rectangleSelectAction_ = makeAction(
        this, tr("&Rectangle Select"), QKeySequence(QStringLiteral("R")),
        QStringLiteral("actionToolRectangleSelect")
    );
    rectangleSelectAction_->setCheckable(true);
    rectangleSelectAction_->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogContentsView)
    );

    ellipseSelectAction_ = makeAction(
        this, tr("&Ellipse Select"), QKeySequence(),
        QStringLiteral("actionToolEllipseSelect")
    );
    ellipseSelectAction_->setCheckable(true);
    ellipseSelectAction_->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogDetailedView)
    );

    bucketFillAction_ = makeAction(
        this, tr("&Bucket Fill"), QKeySequence(),
        QStringLiteral("actionToolBucketFill")
    );
    bucketFillAction_->setCheckable(true);
    bucketFillAction_->setIcon(
        style()->standardIcon(QStyle::SP_DialogApplyButton)
    );

    cropAction_->setCheckable(true);

    toolGroup_->addAction(rectangleSelectAction_);
    toolGroup_->addAction(ellipseSelectAction_);
    toolGroup_->addAction(bucketFillAction_);
    toolGroup_->addAction(cropAction_);
    toolBar->addAction(rectangleSelectAction_);
    toolBar->addAction(ellipseSelectAction_);
    toolBar->addAction(bucketFillAction_);
    toolBar->addAction(cropAction_);
    toolBar->addAction(cropApplyAction_);
    toolBar->addAction(cropCancelAction_);
    toolBar->addSeparator();

    clearSelectionAction_ = makeAction(
        this, tr("&Clear Selection"), QKeySequence(),
        QStringLiteral("actionSelectClear")
    );
    clearSelectionAction_->setIcon(
        style()->standardIcon(QStyle::SP_DialogCancelButton)
    );
    invertSelectionAction_ = makeAction(
        this, tr("&Invert Selection"), QKeySequence(),
        QStringLiteral("actionSelectInvert")
    );
    invertSelectionAction_->setIcon(
        style()->standardIcon(QStyle::SP_DialogResetButton)
    );
    toolBar->addAction(clearSelectionAction_);
    toolBar->addAction(invertSelectionAction_);
    toolBar->addSeparator();

    colorButton_ = new QToolButton(toolBar);
    colorButton_->setObjectName(QStringLiteral("toolColorButton"));
    colorButton_->setAutoRaise(true);
    colorButton_->setToolTip(tr("Foreground color"));
    colorButton_->setIconSize(QSize(18, 18));
    toolBar->addWidget(new QLabel(tr("Color"), toolBar));
    toolBar->addWidget(colorButton_);
    toolBar->addSeparator();

    const auto addSpin = [this, toolBar](
                             const QString& label,
                             const QString& objectName,
                             int minimum,
                             int maximum,
                             int value,
                             const QString& suffix
                         ) {
        auto* spin = new QSpinBox(toolBar);
        spin->setObjectName(objectName);
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        spin->setSuffix(suffix);
        spin->setFixedWidth(72);
        spin->setKeyboardTracking(false);
        toolBar->addWidget(new QLabel(label, toolBar));
        toolBar->addWidget(spin);
        connect(spin, &QSpinBox::valueChanged, this, [this] {
            applyBrushSettings();
        });
        return spin;
    };

    brushSizeSpin_ = addSpin(
        tr("Size"), QStringLiteral("brushSizeSpin"),
        1, static_cast<int>(core::BrushSettings::maxDiameter), 20,
        QStringLiteral(" px")
    );
    brushOpacitySpin_ = addSpin(
        tr("Opacity"), QStringLiteral("brushOpacitySpin"),
        0, 100, 100, QStringLiteral("%")
    );
    brushHardnessSpin_ = addSpin(
        tr("Hardness"), QStringLiteral("brushHardnessSpin"),
        0, 100, 100, QStringLiteral("%")
    );

    fillToleranceSpin_ = addSpin(
        tr("Tolerance"), QStringLiteral("fillToleranceSpin"),
        0, core::FillSettings::maxTolerance, 0, QStringLiteral("")
    );
    fillToleranceSpin_->setToolTip(
        tr("Per-channel absolute difference tolerance for the fill seed match")
    );
    fillOpacitySpin_ = addSpin(
        tr("Opacity"), QStringLiteral("fillOpacitySpin"),
        0, 100, 100, QStringLiteral("%")
    );

    connect(brushToolAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            canvas_->setActiveTool(CanvasView::Tool::Brush);
            controller_.setBrushMode(core::BrushMode::Paint);
        }
    });
    connect(eraserToolAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            canvas_->setActiveTool(CanvasView::Tool::Brush);
            controller_.setBrushMode(core::BrushMode::Erase);
        }
    });
    connect(rectangleSelectAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            canvas_->setActiveTool(CanvasView::Tool::SelectRectangle);
        }
    });
    connect(ellipseSelectAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            canvas_->setActiveTool(CanvasView::Tool::SelectEllipse);
        }
    });
    connect(bucketFillAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            canvas_->setActiveTool(CanvasView::Tool::BucketFill);
        }
    });
    connect(cropAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            canvas_->setActiveTool(CanvasView::Tool::Crop);
        }
    });
    connect(clearSelectionAction_, &QAction::triggered, this, [this] {
        controller_.clearSelection();
    });
    connect(invertSelectionAction_, &QAction::triggered, this, [this] {
        controller_.invertSelection();
    });
    connect(colorButton_, &QToolButton::clicked, this, [this] {
        chooseBrushColor();
    });
    connect(fillToleranceSpin_, &QSpinBox::valueChanged, this, [this] {
        applyFillSettings();
    });
    connect(fillOpacitySpin_, &QSpinBox::valueChanged, this, [this] {
        applyFillSettings();
    });

    brushToolAction_->setChecked(true);
    updateColorButton();
    applyBrushSettings();
    applyFillSettings();
}

void MainWindow::createStatusBar() {
    statusInfo_ = new QLabel(this);
    statusInfo_->setObjectName(QStringLiteral("statusInfo"));
    statusBar()->addPermanentWidget(statusInfo_);
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createDocks() {
    auto* dock = new QDockWidget(tr("Layers"), this);
    dock->setObjectName(QStringLiteral("layersDock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    layerDock_ = new LayerDock(dock);
    layerDock_->setMinimumWidth(220);
    dock->setWidget(layerDock_);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    resizeDocks({dock}, {280}, Qt::Horizontal);
}

void MainWindow::connectSignals() {
    connect(canvas_, &CanvasView::strokeStarted, this, [this](const QPointF& position) {
        controller_.beginBrushStroke(position.x(), position.y());
    });
    connect(canvas_, &CanvasView::strokeMoved, this, [this](const QPointF& position) {
        controller_.updateBrushStroke(position.x(), position.y());
    });
    connect(canvas_, &CanvasView::strokeEnded, this, [this] {
        controller_.endBrushStroke();
    });
    connect(canvas_, &CanvasView::strokeCancelled, this, [this] {
        controller_.cancelBrushStroke();
    });

    connect(canvas_, &CanvasView::selectionDragStarted, this, [this](const QPointF& position) {
        const auto type = canvas_->activeTool() == CanvasView::Tool::SelectEllipse
            ? core::Selection::Type::Ellipse : core::Selection::Type::Rectangle;
        controller_.beginSelection(type, position.x(), position.y());
    });
    connect(canvas_, &CanvasView::selectionDragMoved, this, [this](const QPointF& position) {
        controller_.updateSelection(position.x(), position.y());
    });
    connect(canvas_, &CanvasView::selectionDragCompleted, this, [this] {
        controller_.endSelection();
    });
    connect(canvas_, &CanvasView::selectionDragCancelled, this, [this] {
        controller_.cancelSelection();
    });
    connect(canvas_, &CanvasView::bucketFillRequested, this, [this](const QPointF& position) {
        controller_.bucketFill(position.x(), position.y());
    });
    connect(canvas_, &CanvasView::cropRectChanged, this, [this](const QRectF&) {
        updateActions();
    });
    connect(canvas_, &CanvasView::cropApplyRequested, this, [this] {
        onCropApply();
    });
    connect(canvas_, &CanvasView::cropCancelRequested, this, [this] {
        onCropCancel();
    });

    connect(&controller_, &AppController::documentChanged, this, [this] {
        refreshCanvas();
        canvas_->setSelection(controller_.selection());
        refreshStatus();
        updateTitle();
    });
    connect(&controller_, &AppController::pixelSelectionChanged, this, [this] {
        canvas_->setSelection(controller_.selection());
    });
    connect(&controller_, &AppController::layersChanged, this, [this] {
        refreshLayers();
        refreshStatus();
        updateActions();
    });
    connect(
        &controller_,
        &AppController::selectionChanged,
        this,
        [this](quint64 layerId) {
            layerDock_->setSelectedLayer(static_cast<core::LayerId>(layerId));
        }
    );
    connect(&controller_, &AppController::historyChanged, this, [this] {
        updateActions();
    });
    connect(&controller_, &AppController::filePathChanged, this, [this] {
        updateTitle();
        refreshStatus();
    });
    connect(&controller_, &AppController::modifiedChanged, this, [this] {
        updateTitle();
    });
    connect(&controller_, &AppController::errorOccurred, this, [this](const QString& message) {
        showError(message);
    });

    connect(layerDock_, &LayerDock::layerSelected, this, [this](quint64 layerId) {
        controller_.selectLayer(static_cast<core::LayerId>(layerId));
    });
    connect(layerDock_, &LayerDock::addLayerRequested, this, [this] {
        controller_.addLayer();
    });
    connect(layerDock_, &LayerDock::deleteLayerRequested, this, [this] {
        controller_.removeLayer(controller_.selectedLayerId());
    });
    connect(layerDock_, &LayerDock::moveLayerUpRequested, this, [this] {
        controller_.moveLayerUp(controller_.selectedLayerId());
    });
    connect(layerDock_, &LayerDock::moveLayerDownRequested, this, [this] {
        controller_.moveLayerDown(controller_.selectedLayerId());
    });
    connect(
        layerDock_,
        &LayerDock::layerRenamed,
        this,
        [this](quint64 layerId, const QString& name) {
            controller_.renameLayer(static_cast<core::LayerId>(layerId), name);
        },
        Qt::QueuedConnection
    );
    connect(
        layerDock_,
        &LayerDock::visibilityToggled,
        this,
        [this](quint64 layerId, bool visible) {
            controller_.setLayerVisibility(
                static_cast<core::LayerId>(layerId), visible
            );
        },
        Qt::QueuedConnection
    );
    connect(
        layerDock_,
        &LayerDock::opacityChanged,
        this,
        [this](quint64 layerId, int percent) {
            controller_.setLayerOpacity(
                static_cast<core::LayerId>(layerId),
                static_cast<float>(percent) / 100.0F
            );
        },
        Qt::QueuedConnection
    );
}

void MainWindow::applyStyle() {
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QDialog {
            background-color: #202023;
            color: #e4e4e6;
        }
        QMenuBar {
            background-color: #26262a;
            color: #e4e4e6;
            border-bottom: 1px solid #343438;
        }
        QMenuBar::item {
            padding: 4px 8px;
            background: transparent;
        }
        QMenuBar::item:selected {
            background-color: #35353a;
        }
        QMenu {
            background-color: #2a2a2e;
            color: #e4e4e6;
            border: 1px solid #3a3a40;
        }
        QMenu::item {
            padding: 5px 24px 5px 20px;
        }
        QMenu::item:selected {
            background-color: #3c6da6;
        }
        QToolBar {
            background-color: #26262a;
            border: none;
            border-bottom: 1px solid #343438;
            spacing: 4px;
            padding: 4px 6px;
        }
        QToolButton {
            color: #e4e4e6;
            padding: 4px 6px;
            border: 1px solid transparent;
            border-radius: 3px;
        }
        QToolButton:hover {
            background-color: #35353a;
            border-color: #414147;
        }
        QToolButton:pressed {
            background-color: #2d2d32;
        }
        QToolButton:disabled {
            color: #707076;
        }
        QToolButton:checked {
            background-color: #3c6da6;
            border-color: #4b82c4;
            color: #ffffff;
        }
        QDockWidget {
            color: #e4e4e6;
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
        }
        QDockWidget::title {
            background-color: #26262a;
            padding: 6px 8px;
            border-bottom: 1px solid #343438;
        }
        QListWidget {
            background-color: #242427;
            color: #dedee1;
            border: 1px solid #343438;
            outline: none;
        }
        QListWidget::item {
            padding: 6px 4px;
            border-bottom: 1px solid #2d2d31;
        }
        QListWidget::item:selected {
            background-color: #3c6da6;
            color: #ffffff;
        }
        QSpinBox {
            background-color: #2b2b2f;
            color: #e4e4e6;
            border: 1px solid #3a3a40;
            padding: 3px;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #34343a;
            border: none;
            width: 16px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #414148;
        }
        QLineEdit, QComboBox {
            background-color: #2b2b2f;
            color: #e4e4e6;
            border: 1px solid #3a3a40;
            padding: 3px;
        }
        QComboBox QAbstractItemView {
            background-color: #2a2a2e;
            color: #e4e4e6;
            selection-background-color: #3c6da6;
        }
        QPushButton {
            background-color: #323238;
            color: #e4e4e6;
            border: 1px solid #414148;
            padding: 4px 10px;
            border-radius: 3px;
        }
        QPushButton:hover {
            background-color: #3c3c44;
        }
        QPushButton:pressed {
            background-color: #2c2c32;
        }
        QPushButton:disabled {
            color: #707076;
        }
        QStatusBar {
            background-color: #26262a;
            color: #a8a8ad;
            border-top: 1px solid #343438;
        }
        QStatusBar QLabel {
            color: #a8a8ad;
        }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #202023;
            border: none;
            width: 12px;
            height: 12px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #45454c;
            min-height: 24px;
            min-width: 24px;
            border-radius: 4px;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            width: 0;
            height: 0;
        }
    )"));
}

void MainWindow::refreshCanvas() {
    canvas_->setImage(toQImage(controller_.render()));
}

void MainWindow::refreshLayers() {
    layerDock_->refresh(
        controller_.document().layerStack(),
        controller_.selectedLayerId()
    );
}

void MainWindow::refreshStatus() {
    const auto& document = controller_.document();
    QString text = tr("%1 x %2 px  |  %3  |  RGBA8")
        .arg(document.width())
        .arg(document.height())
        .arg(
            document.layerCount() == 1
                ? tr("1 layer")
                : tr("%1 layers").arg(document.layerCount())
        );

    if (!controller_.filePath().isEmpty()) {
        text += tr("  |  %1").arg(QFileInfo(controller_.filePath()).fileName());
    }
    statusInfo_->setText(text);
}

void MainWindow::updateActions() {
    undoAction_->setEnabled(controller_.canUndo());
    redoAction_->setEnabled(controller_.canRedo());

    const QRectF crop = canvas_->cropRect();
    const bool hasUsableCrop =
        canvas_->hasCrop() &&
        crop.width() >= 1.0 &&
        crop.height() >= 1.0;
    cropApplyAction_->setEnabled(hasUsableCrop);
    cropCancelAction_->setEnabled(canvas_->hasCrop());

    const bool hasLayer =
        controller_.document().findLayer(
            controller_.selectedLayerId()
        ) != nullptr;
    rotateLayerCWAction_->setEnabled(hasLayer);
    rotateLayerCCWAction_->setEnabled(hasLayer);
    rotateLayer180Action_->setEnabled(hasLayer);
    rotateLayerFreeAction_->setEnabled(hasLayer);
    flipLayerHorizontalAction_->setEnabled(hasLayer);
    flipLayerVerticalAction_->setEnabled(hasLayer);
    scaleLayerAction_->setEnabled(hasLayer);
    translateLayerAction_->setEnabled(hasLayer);
}

void MainWindow::updateTitle() {
    const QString name = QString::fromStdString(controller_.document().name());
    setWindowTitle(tr("Glyma - %1[*]").arg(name));
    setWindowModified(controller_.isModified());
}

void MainWindow::applyBrushSettings() {
    core::BrushSettings settings;
    settings.diameter = static_cast<double>(brushSizeSpin_->value());
    settings.color = core::Pixel{
        static_cast<std::uint8_t>(brushColor_.red()),
        static_cast<std::uint8_t>(brushColor_.green()),
        static_cast<std::uint8_t>(brushColor_.blue()),
        static_cast<std::uint8_t>(brushColor_.alpha())
    };
    settings.opacity =
        static_cast<float>(brushOpacitySpin_->value()) / 100.0F;
    settings.hardness =
        static_cast<float>(brushHardnessSpin_->value()) / 100.0F;
    controller_.setBrushSettings(settings);
}

void MainWindow::applyFillSettings() {
    core::FillSettings settings;
    settings.color = core::Pixel{
        static_cast<std::uint8_t>(brushColor_.red()),
        static_cast<std::uint8_t>(brushColor_.green()),
        static_cast<std::uint8_t>(brushColor_.blue()),
        static_cast<std::uint8_t>(brushColor_.alpha())
    };
    settings.opacity =
        static_cast<float>(fillOpacitySpin_->value()) / 100.0F;
    settings.tolerance = fillToleranceSpin_->value();
    controller_.setFillSettings(settings);
}

void MainWindow::updateColorButton() {
    QPixmap swatch(18, 18);
    swatch.fill(Qt::transparent);
    QPainter painter(&swatch);
    painter.setPen(QColor(20, 20, 22));
    painter.setBrush(brushColor_);
    painter.drawRect(1, 1, 15, 15);
    painter.end();
    colorButton_->setIcon(QIcon(swatch));
}

void MainWindow::chooseBrushColor() {
    const QColor chosen = QColorDialog::getColor(
        brushColor_, this, tr("Brush Color"),
        QColorDialog::ShowAlphaChannel
    );
    if (!chosen.isValid()) {
        return;
    }
    brushColor_ = chosen;
    updateColorButton();
    applyBrushSettings();
    applyFillSettings();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    controller_.endBrushStroke();
    controller_.cancelAdjustment();
    controller_.cancelSelection();
    if (!prepareForDestructiveAction()) {
        event->ignore();
        return;
    }
    event->accept();
}

bool MainWindow::prepareForDestructiveAction() {
    return confirmUnsavedChanges(
        controller_.isModified(),
        [this] { return promptUnsavedChanges(); },
        [this] { return saveCurrentDocument(); }
    );
}

UnsavedChoice MainWindow::promptUnsavedChanges() {
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Unsaved Changes"));
    box.setText(
        tr("Save changes to \"%1\"?")
            .arg(QString::fromStdString(controller_.document().name()))
    );
    box.setInformativeText(
        tr("Your changes will be lost if you do not save them.")
    );
    box.setStandardButtons(
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );
    box.setDefaultButton(QMessageBox::Save);
    box.setEscapeButton(QMessageBox::Cancel);

    switch (box.exec()) {
    case QMessageBox::Save:
        return UnsavedChoice::Save;
    case QMessageBox::Discard:
        return UnsavedChoice::Discard;
    default:
        return UnsavedChoice::Cancel;
    }
}

void MainWindow::onNew() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("New Document"));
    auto* layout = new QFormLayout(&dialog);

    auto* nameEdit = new QLineEdit(QStringLiteral("Untitled"), &dialog);
    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(1, 16384);
    widthSpin->setValue(1280);
    widthSpin->setSuffix(tr(" px"));
    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(1, 16384);
    heightSpin->setValue(720);
    heightSpin->setSuffix(tr(" px"));

    auto* backgroundCombo = new QComboBox(&dialog);
    backgroundCombo->addItem(tr("Transparent"));
    backgroundCombo->addItem(tr("Solid Color"));

    QColor backgroundColor(255, 255, 255, 255);
    auto* colorButton = new QPushButton(&dialog);
    colorButton->setEnabled(false);
    const auto updateColorButton = [colorButton, &backgroundColor] {
        colorButton->setText(backgroundColor.name(QColor::HexRgb));
    };
    updateColorButton();

    connect(
        backgroundCombo, &QComboBox::currentIndexChanged,
        this, [colorButton](int index) {
            colorButton->setEnabled(index == 1);
        }
    );
    connect(colorButton, &QPushButton::clicked, this, [this, &backgroundColor, updateColorButton] {
        const QColor chosen = QColorDialog::getColor(
            backgroundColor, this, tr("Background Color"),
            QColorDialog::ShowAlphaChannel
        );
        if (chosen.isValid()) {
            backgroundColor = chosen;
            updateColorButton();
        }
    });

    layout->addRow(tr("Name:"), nameEdit);
    layout->addRow(tr("Width:"), widthSpin);
    layout->addRow(tr("Height:"), heightSpin);
    layout->addRow(tr("Background:"), backgroundCombo);
    layout->addRow(QString(), colorButton);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog
    );
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, &dialog, widthSpin, heightSpin] {
        const std::uint64_t pixels =
            static_cast<std::uint64_t>(widthSpin->value()) *
            static_cast<std::uint64_t>(heightSpin->value());
        if (pixels > core::Document::maxPixelCount) {
            QMessageBox::warning(
                &dialog,
                tr("New Document"),
                tr("The requested canvas exceeds the %1 megapixel limit.")
                    .arg(core::Document::maxPixelCount / (1024 * 1024))
            );
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (!prepareForDestructiveAction()) {
        return;
    }

    const bool solidBackground = backgroundCombo->currentIndex() == 1;
    const core::Pixel pixel{
        static_cast<std::uint8_t>(backgroundColor.red()),
        static_cast<std::uint8_t>(backgroundColor.green()),
        static_cast<std::uint8_t>(backgroundColor.blue()),
        static_cast<std::uint8_t>(backgroundColor.alpha())
    };
    if (controller_.newDocument(
            nameEdit->text(),
            static_cast<std::uint32_t>(widthSpin->value()),
            static_cast<std::uint32_t>(heightSpin->value()),
            solidBackground,
            pixel
        )) {
        statusBar()->showMessage(tr("Created a new document."), 3000);
    }
}

void MainWindow::onOpen() {
    const QString startDirectory = controller_.filePath().isEmpty()
        ? QDir::homePath()
        : QFileInfo(controller_.filePath()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Image or Project"),
        startDirectory,
        tr("Supported Files (*.glyma *.png);;Glyma Projects (*.glyma);;PNG Images (*.png);;All Files (*)")
    );
    if (path.isEmpty()) {
        return;
    }
    if (!prepareForDestructiveAction()) {
        return;
    }
    if (controller_.openFile(path)) {
        statusBar()->showMessage(tr("Opened %1").arg(QFileInfo(path).fileName()), 3000);
    }
}

void MainWindow::onSave() {
    (void)saveCurrentDocument();
}

void MainWindow::onSaveAs() {
    (void)saveDocumentAs();
}

bool MainWindow::saveCurrentDocument() {
    bool saved = false;
    if (controller_.filePath().isEmpty()) {
        saved = saveDocumentAs();
    } else {
        saved = controller_.save(controller_.filePath());
    }

    if (saved) {
        statusBar()->showMessage(
            tr("Saved %1").arg(QFileInfo(controller_.filePath()).fileName()), 3000
        );
    }
    return saved;
}

bool MainWindow::saveDocumentAs() {
    QString suggested = controller_.filePath();
    if (suggested.isEmpty()) {
        suggested = QDir(QDir::homePath()).filePath(
            QString::fromStdString(controller_.document().name()) +
            QStringLiteral(".glyma")
        );
    }

    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Project As"),
        suggested,
        tr("Glyma Project (*.glyma)")
    );
    if (path.isEmpty()) {
        return false;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".glyma");
    }

    const bool saved = controller_.save(path);
    if (saved) {
        statusBar()->showMessage(
            tr("Saved %1").arg(QFileInfo(path).fileName()), 3000
        );
    }
    return saved;
}

void MainWindow::onExportPng() {
    const QString directory = controller_.filePath().isEmpty()
        ? QDir::homePath()
        : QFileInfo(controller_.filePath()).absolutePath();
    const QString suggested = QDir(directory).filePath(
        QString::fromStdString(controller_.document().name()) +
        QStringLiteral(".png")
    );
    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Export PNG"),
        suggested,
        tr("PNG Image (*.png)")
    );
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".png");
    }
    if (controller_.exportPng(path)) {
        statusBar()->showMessage(
            tr("Exported %1").arg(QFileInfo(path).fileName()), 3000
        );
    }
}

void MainWindow::onAdjustments() {
    if (!controller_.beginAdjustment()) {
        showError(tr("Select a visible layer to adjust."));
        return;
    }

    AdjustmentDialog dialog(this);
    dialog.setSettings(controller_.adjustmentSettings());
    connect(
        &dialog,
        &AdjustmentDialog::settingsChanged,
        this,
        [this](const core::AdjustmentSettings& settings) {
            controller_.updateAdjustment(settings);
        }
    );

    const int result = dialog.exec();
    if (result == QDialog::Accepted) {
        if (controller_.applyAdjustment()) {
            statusBar()->showMessage(tr("Applied adjustments."), 3000);
        } else {
            statusBar()->showMessage(tr("No changes to apply."), 3000);
        }
    } else {
        controller_.cancelAdjustment();
    }
}

void MainWindow::onCropApply() {
    if (!canvas_->hasCrop()) {
        return;
    }
    const QRectF rect = canvas_->cropRect();
    const auto& document = controller_.document();
    const core::CropRect crop = core::cropRectFromPoints(
        rect.left(), rect.top(), rect.right(), rect.bottom(),
        document.width(), document.height()
    );
    if (crop.isEmpty()) {
        updateActions();
        return;
    }
    if (controller_.crop(crop)) {
        canvas_->clearCrop();
        statusBar()->showMessage(
            tr("Cropped to %1 x %2 px")
                .arg(document.width())
                .arg(document.height()),
            3000
        );
    }
    updateActions();
}

void MainWindow::onCropCancel() {
    canvas_->clearCrop();
    updateActions();
    statusBar()->showMessage(tr("Crop cancelled."), 2000);
}

void MainWindow::onCanvasSize() {
    const auto& document = controller_.document();
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Canvas Size"));
    auto* layout = new QFormLayout(&dialog);

    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setObjectName(QStringLiteral("canvasWidthSpin"));
    widthSpin->setRange(1, 100000);
    widthSpin->setValue(static_cast<int>(document.width()));
    widthSpin->setSuffix(tr(" px"));

    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setObjectName(QStringLiteral("canvasHeightSpin"));
    heightSpin->setRange(1, 100000);
    heightSpin->setValue(static_cast<int>(document.height()));
    heightSpin->setSuffix(tr(" px"));

    auto* anchorCombo = new QComboBox(&dialog);
    anchorCombo->setObjectName(QStringLiteral("canvasAnchorCombo"));
    anchorCombo->addItems({
        tr("Top Left"), tr("Top Center"), tr("Top Right"),
        tr("Center Left"), tr("Center"), tr("Center Right"),
        tr("Bottom Left"), tr("Bottom Center"), tr("Bottom Right"),
    });
    anchorCombo->setCurrentIndex(4);

    auto* preview = new QLabel(&dialog);
    preview->setObjectName(QStringLiteral("canvasSizePreview"));
    const auto updatePreview = [&document, widthSpin, heightSpin, preview] {
        const auto width = static_cast<std::uint32_t>(widthSpin->value());
        const auto height = static_cast<std::uint32_t>(heightSpin->value());
        const bool valid = core::isValidCanvasSize(width, height);
        preview->setText(
            valid
                ? tr("New canvas: %1 x %2 px (layers keep their pixel size)")
                      .arg(width)
                      .arg(height)
                : tr("Canvas exceeds the supported size limit.")
        );
    };
    QObject::connect(
        widthSpin, &QSpinBox::valueChanged, &dialog, updatePreview
    );
    QObject::connect(
        heightSpin, &QSpinBox::valueChanged, &dialog, updatePreview
    );
    updatePreview();

    layout->addRow(tr("Width:"), widthSpin);
    layout->addRow(tr("Height:"), heightSpin);
    layout->addRow(tr("Anchor:"), anchorCombo);
    layout->addRow(preview);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog
    );
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, widthSpin, heightSpin] {
        if (!core::isValidCanvasSize(
                static_cast<std::uint32_t>(widthSpin->value()),
                static_cast<std::uint32_t>(heightSpin->value()))) {
            QMessageBox::warning(
                &dialog, tr("Canvas Size"),
                tr("The requested canvas exceeds the supported size limit.")
            );
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto anchor = static_cast<core::Anchor>(anchorCombo->currentIndex());
    if (controller_.resizeCanvas(
            static_cast<std::uint32_t>(widthSpin->value()),
            static_cast<std::uint32_t>(heightSpin->value()),
            anchor
        )) {
        canvas_->clearCrop();
        statusBar()->showMessage(
            tr("Canvas resized to %1 x %2 px")
                .arg(widthSpin->value())
                .arg(heightSpin->value()),
            3000
        );
    }
}

void MainWindow::onImageSize() {
    const auto& document = controller_.document();
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Image Size"));
    auto* layout = new QFormLayout(&dialog);

    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setObjectName(QStringLiteral("imageWidthSpin"));
    widthSpin->setRange(1, 100000);
    widthSpin->setValue(static_cast<int>(document.width()));
    widthSpin->setSuffix(tr(" px"));

    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setObjectName(QStringLiteral("imageHeightSpin"));
    heightSpin->setRange(1, 100000);
    heightSpin->setValue(static_cast<int>(document.height()));
    heightSpin->setSuffix(tr(" px"));

    auto* maintainAspect = new QCheckBox(tr("Maintain aspect ratio"), &dialog);
    maintainAspect->setChecked(true);

    auto* preview = new QLabel(&dialog);
    preview->setObjectName(QStringLiteral("imageSizePreview"));
    const auto updatePreview = [widthSpin, heightSpin, preview] {
        const auto width = static_cast<std::uint32_t>(widthSpin->value());
        const auto height = static_cast<std::uint32_t>(heightSpin->value());
        const bool valid = core::isValidCanvasSize(width, height);
        preview->setText(
            valid
                ? tr("New image: %1 x %2 px (all layers resampled)")
                      .arg(width)
                      .arg(height)
                : tr("Image exceeds the supported size limit.")
        );
    };

    const double aspect =
        static_cast<double>(document.width()) /
        static_cast<double>(document.height());
    bool syncing = false;
    QObject::connect(
        widthSpin, &QSpinBox::valueChanged, &dialog,
        [widthSpin, heightSpin, maintainAspect, aspect, &syncing, updatePreview](int value) {
            if (syncing) {
                return;
            }
            if (maintainAspect->isChecked()) {
                syncing = true;
                heightSpin->setValue(std::max(
                    1,
                    static_cast<int>(std::lround(
                        static_cast<double>(value) / aspect
                    ))
                ));
                syncing = false;
            }
            updatePreview();
        }
    );
    QObject::connect(
        heightSpin, &QSpinBox::valueChanged, &dialog,
        [widthSpin, heightSpin, maintainAspect, aspect, &syncing, updatePreview](int value) {
            if (syncing) {
                return;
            }
            if (maintainAspect->isChecked()) {
                syncing = true;
                widthSpin->setValue(std::max(
                    1,
                    static_cast<int>(std::lround(
                        static_cast<double>(value) * aspect
                    ))
                ));
                syncing = false;
            }
            updatePreview();
        }
    );
    QObject::connect(
        maintainAspect, &QCheckBox::toggled, &dialog, updatePreview
    );
    updatePreview();

    layout->addRow(tr("Width:"), widthSpin);
    layout->addRow(tr("Height:"), heightSpin);
    layout->addRow(QString(), maintainAspect);
    layout->addRow(preview);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog
    );
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, widthSpin, heightSpin] {
        if (!core::isValidCanvasSize(
                static_cast<std::uint32_t>(widthSpin->value()),
                static_cast<std::uint32_t>(heightSpin->value()))) {
            QMessageBox::warning(
                &dialog, tr("Image Size"),
                tr("The requested image exceeds the supported size limit.")
            );
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (controller_.resizeImage(
            static_cast<std::uint32_t>(widthSpin->value()),
            static_cast<std::uint32_t>(heightSpin->value())
        )) {
        canvas_->clearCrop();
        statusBar()->showMessage(
            tr("Image resized to %1 x %2 px")
                .arg(widthSpin->value())
                .arg(heightSpin->value()),
            3000
        );
    }
}

void MainWindow::onArbitraryRotateCanvas() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Rotate Canvas"));
    auto* layout = new QFormLayout(&dialog);
    auto* angleSpin = new QDoubleSpinBox(&dialog);
    angleSpin->setObjectName(QStringLiteral("canvasAngleSpin"));
    angleSpin->setRange(-3600.0, 3600.0);
    angleSpin->setDecimals(2);
    angleSpin->setSuffix(tr(" degrees"));
    angleSpin->setValue(0.0);
    layout->addRow(tr("Angle:"), angleSpin);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog
    );
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, angleSpin] {
        if (std::abs(angleSpin->value()) < 1.0e-9) {
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (controller_.rotateCanvas(angleSpin->value())) {
        canvas_->clearCrop();
        statusBar()->showMessage(tr("Canvas rotated."), 3000);
    }
}

void MainWindow::onRotateCanvas90(bool clockwise) {
    if (controller_.rotateCanvas90(clockwise)) {
        canvas_->clearCrop();
        statusBar()->showMessage(tr("Canvas rotated 90 degrees."), 3000);
    }
}

void MainWindow::onRotateCanvas180() {
    if (controller_.rotateCanvas180()) {
        canvas_->clearCrop();
        statusBar()->showMessage(tr("Canvas rotated 180 degrees."), 3000);
    }
}

void MainWindow::onFlipCanvas(bool horizontal) {
    if (controller_.flipCanvas(horizontal)) {
        canvas_->clearCrop();
        statusBar()->showMessage(
            horizontal ? tr("Canvas flipped horizontally.")
                       : tr("Canvas flipped vertically."),
            3000
        );
    }
}

void MainWindow::onScaleLayer() {
    const core::LayerId id = controller_.selectedLayerId();
    const core::Layer* layer = controller_.document().findLayer(id);
    if (layer == nullptr) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Scale Layer"));
    auto* layout = new QFormLayout(&dialog);
    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setObjectName(QStringLiteral("layerWidthSpin"));
    widthSpin->setRange(1, 100000);
    widthSpin->setValue(static_cast<int>(layer->width()));
    widthSpin->setSuffix(tr(" px"));
    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setObjectName(QStringLiteral("layerHeightSpin"));
    heightSpin->setRange(1, 100000);
    heightSpin->setValue(static_cast<int>(layer->height()));
    heightSpin->setSuffix(tr(" px"));
    layout->addRow(tr("Width:"), widthSpin);
    layout->addRow(tr("Height:"), heightSpin);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog
    );
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, widthSpin, heightSpin] {
        if (!core::isValidCanvasSize(
                static_cast<std::uint32_t>(widthSpin->value()),
                static_cast<std::uint32_t>(heightSpin->value()))) {
            QMessageBox::warning(
                &dialog, tr("Scale Layer"),
                tr("The requested layer size is outside the supported range.")
            );
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (controller_.scaleLayer(
            id,
            static_cast<std::uint32_t>(widthSpin->value()),
            static_cast<std::uint32_t>(heightSpin->value())
        )) {
        statusBar()->showMessage(tr("Layer scaled."), 3000);
    }
}

void MainWindow::onRotateLayer(core::LayerTransform transform) {
    if (controller_.transformLayer(controller_.selectedLayerId(), transform)) {
        statusBar()->showMessage(tr("Layer transformed."), 3000);
    }
}

void MainWindow::onArbitraryRotateLayer() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Rotate Layer"));
    auto* layout = new QFormLayout(&dialog);
    auto* angleSpin = new QDoubleSpinBox(&dialog);
    angleSpin->setObjectName(QStringLiteral("layerAngleSpin"));
    angleSpin->setRange(-3600.0, 3600.0);
    angleSpin->setDecimals(2);
    angleSpin->setSuffix(tr(" degrees"));
    angleSpin->setValue(0.0);
    layout->addRow(tr("Angle:"), angleSpin);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog
    );
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, angleSpin] {
        if (std::abs(angleSpin->value()) < 1.0e-9) {
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (controller_.rotateLayer(
            controller_.selectedLayerId(), angleSpin->value()
        )) {
        statusBar()->showMessage(tr("Layer rotated."), 3000);
    }
}

void MainWindow::onFlipLayer(bool horizontal) {
    const core::LayerTransform transform = horizontal
        ? core::LayerTransform::FlipHorizontal
        : core::LayerTransform::FlipVertical;
    if (controller_.transformLayer(controller_.selectedLayerId(), transform)) {
        statusBar()->showMessage(
            horizontal ? tr("Layer flipped horizontally.")
                       : tr("Layer flipped vertically."),
            3000
        );
    }
}

void MainWindow::onTranslateLayer() {
    const core::LayerId id = controller_.selectedLayerId();
    const core::Layer* layer = controller_.document().findLayer(id);
    if (layer == nullptr) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Move Layer"));
    auto* layout = new QFormLayout(&dialog);
    auto* xSpin = new QSpinBox(&dialog);
    xSpin->setObjectName(QStringLiteral("layerXSpin"));
    xSpin->setRange(-1000000, 1000000);
    xSpin->setValue(layer->x());
    xSpin->setSuffix(tr(" px"));
    auto* ySpin = new QSpinBox(&dialog);
    ySpin->setObjectName(QStringLiteral("layerYSpin"));
    ySpin->setRange(-1000000, 1000000);
    ySpin->setValue(layer->y());
    ySpin->setSuffix(tr(" px"));
    layout->addRow(tr("X offset:"), xSpin);
    layout->addRow(tr("Y offset:"), ySpin);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog
    );
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const std::int32_t deltaX =
        static_cast<std::int32_t>(xSpin->value()) - layer->x();
    const std::int32_t deltaY =
        static_cast<std::int32_t>(ySpin->value()) - layer->y();
    if (controller_.translateLayer(id, deltaX, deltaY)) {
        statusBar()->showMessage(tr("Layer moved."), 3000);
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this,
        tr("About Glyma"),
        tr("<h3>Glyma %1</h3>"
           "<p>A native image editor built with a custom C++20 engine and Qt 6.</p>"
           "<p>Canvas rendering, layers, undo/redo, PNG I/O, and native .glyma "
           "projects are provided by the Glyma engine.</p>"
           "<p>Qt %2</p>")
            .arg(QApplication::applicationVersion(), qVersion())
    );
}

void MainWindow::showError(const QString& message) {
    QMessageBox::critical(this, tr("Glyma"), message);
}

} // namespace glyma::app
