#include "glyma/app/main_window.hpp"

#include <cstdint>

#include <QAction>
#include <QApplication>
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
#include <QSize>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>

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
    connect(&controller_, &AppController::errorOccurred, this, [this](const QString& message) {
        showError(message);
    });

    connect(layerDock_, &LayerDock::layerSelected, this, [this](quint64 layerId) {
        controller_.selectLayer(static_cast<core::LayerId>(layerId));
    });
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
    setWindowTitle(tr("Glyma - %1").arg(name));
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

    layout->addRow(tr("Name:"), nameEdit);
    layout->addRow(tr("Width:"), widthSpin);
    layout->addRow(tr("Height:"), heightSpin);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog
    );
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const std::uint64_t pixels =
        static_cast<std::uint64_t>(widthSpin->value()) *
        static_cast<std::uint64_t>(heightSpin->value());
    if (pixels > core::Document::maxPixelCount) {
        QMessageBox::warning(
            this,
            tr("New Document"),
            tr("The requested canvas exceeds the %1 megapixel limit.")
                .arg(core::Document::maxPixelCount / (1024 * 1024))
        );
        return;
    }

    if (controller_.newDocument(
            nameEdit->text(),
            static_cast<std::uint32_t>(widthSpin->value()),
            static_cast<std::uint32_t>(heightSpin->value())
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
    if (controller_.openFile(path)) {
        statusBar()->showMessage(tr("Opened %1").arg(QFileInfo(path).fileName()), 3000);
    }
}

void MainWindow::onSave() {
    if (controller_.filePath().isEmpty()) {
        onSaveAs();
        return;
    }
    if (controller_.save(controller_.filePath())) {
        statusBar()->showMessage(
            tr("Saved %1").arg(QFileInfo(controller_.filePath()).fileName()), 3000
        );
    }
}

void MainWindow::onSaveAs() {
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
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".glyma");
    }
    if (controller_.save(path)) {
        statusBar()->showMessage(
            tr("Saved %1").arg(QFileInfo(path).fileName()), 3000
        );
    }
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
