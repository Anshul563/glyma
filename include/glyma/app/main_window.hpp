#pragma once

#include <QColor>
#include <QMainWindow>

#include "glyma/app/app_controller.hpp"
#include "glyma/app/document_lifecycle.hpp"

class QAction;
class QActionGroup;
class QCloseEvent;
class QLabel;
class QSpinBox;
class QToolButton;

namespace glyma::app {

class CanvasView;
class LayerDock;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    [[nodiscard]] AppController& controller() noexcept;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createToolOptions();
    void createStatusBar();
    void createDocks();
    void connectSignals();
    void applyStyle();
    void refreshCanvas();
    void refreshLayers();
    void refreshStatus();
    void updateActions();
    void updateTitle();
    void applyBrushSettings();
    void updateColorButton();
    void chooseBrushColor();

    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onExportPng();
    void onAbout();

    [[nodiscard]] bool prepareForDestructiveAction();
    [[nodiscard]] bool saveCurrentDocument();
    [[nodiscard]] bool saveDocumentAs();
    [[nodiscard]] UnsavedChoice promptUnsavedChanges();

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

    QActionGroup* toolGroup_{nullptr};
    QAction* brushToolAction_{nullptr};
    QAction* eraserToolAction_{nullptr};
    QToolButton* colorButton_{nullptr};
    QSpinBox* brushSizeSpin_{nullptr};
    QSpinBox* brushOpacitySpin_{nullptr};
    QSpinBox* brushHardnessSpin_{nullptr};
    QColor brushColor_{0, 0, 0, 255};
};

} // namespace glyma::app
