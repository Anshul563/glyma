#pragma once

#include <QMainWindow>

#include "glyma/app/app_controller.hpp"

class QAction;
class QLabel;

namespace glyma::app {

class CanvasView;
class LayerDock;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    [[nodiscard]] AppController& controller() noexcept;

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void createDocks();
    void connectSignals();
    void applyStyle();
    void refreshCanvas();
    void refreshLayers();
    void refreshStatus();
    void updateActions();
    void updateTitle();

    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onExportPng();
    void onAbout();

    void showError(const QString& message);

    AppController controller_;
    CanvasView* canvas_{nullptr};
    LayerDock* layerDock_{nullptr};
    QLabel* statusInfo_{nullptr};

    QAction* newAction_{nullptr};
    QAction* openAction_{nullptr};
    QAction* saveAction_{nullptr};
    QAction* saveAsAction_{nullptr};
    QAction* exportPngAction_{nullptr};
    QAction* exitAction_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* zoomInAction_{nullptr};
    QAction* zoomOutAction_{nullptr};
    QAction* fitAction_{nullptr};
    QAction* resetZoomAction_{nullptr};
    QAction* aboutAction_{nullptr};
};

} // namespace glyma::app
