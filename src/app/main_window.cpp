#include "glyma/app/main_window.hpp"

#include <cstdint>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
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

#include "glyma/app/canvas_view.hpp"
#include "glyma/app/layer_dock.hpp"
#include "glyma/app/qt_image.hpp"

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

    aboutAction_ = makeAction(
        this, tr("&About Glyma"), QKeySequence(), QStringLiteral("actionHelpAbout")
    );

    newAction_->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    openAction_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    saveAction_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    undoAction_->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    redoAction_->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    exitAction_->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));

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
    connect(aboutAction_, &QAction::triggered, this, [this] { onAbout(); });
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

    connect(brushToolAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            controller_.setBrushMode(core::BrushMode::Paint);
        }
    });
    connect(eraserToolAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            controller_.setBrushMode(core::BrushMode::Erase);
        }
    });
    connect(colorButton_, &QToolButton::clicked, this, [this] {
        chooseBrushColor();
    });

    brushToolAction_->setChecked(true);
    updateColorButton();
    applyBrushSettings();
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

    connect(&controller_, &AppController::documentChanged, this, [this] {
        refreshCanvas();
        refreshStatus();
        updateTitle();
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
}

void MainWindow::closeEvent(QCloseEvent* event) {
    controller_.endBrushStroke();
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
