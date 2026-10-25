#pragma once

#include <QColor>
#include <QHash>
#include <QKeySequence>
#include <QMainWindow>

#include "glyma/app/app_controller.hpp"
#include "glyma/app/document_lifecycle.hpp"
#include "glyma/core/fill.hpp"
#include "glyma/core/selection.hpp"

class QAction;
class QActionGroup;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QSpinBox;
class QToolBar;
class QToolButton;
class QWidgetAction;

namespace glyma::app {

class CanvasView;
class LayerDock;
class ShapePropertiesPanel;
class TypographyPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

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
    void arrangeDefaultWorkspace();
    void restoreWorkspace();
    void saveWorkspace();
    void resetWorkspace();
    void connectSignals();
    void applyStyle();
    void updateToolOptions();
    void setToolShortcutsSuppressed(bool suppressed);
    void refreshCanvas();
    void refreshCanvasRegion(int x, int y, int width, int height);
    void refreshLayers();
    void refreshTypography();
    void refreshShape();
    void refreshStatus();
    void updateActions();
    void updateTitle();
    void applyBrushSettings();
    void updateColorButton();
    void chooseBrushColor();
    void applyFillSettings();

    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onExportPng();
    void onAdjustments();
    void onAbout();

    void onCanvasSize();
    void onImageSize();
    void onArbitraryRotateCanvas();
    void onRotateCanvas90(bool clockwise);
    void onRotateCanvas180();
    void onFlipCanvas(bool horizontal);
    void onCropApply();
    void onCropCancel();
    void onScaleLayer();
    void onRotateLayer(core::LayerTransform transform);
    void onArbitraryRotateLayer();
    void onFlipLayer(bool horizontal);
    void onTranslateLayer();

    [[nodiscard]] bool prepareForDestructiveAction();
    [[nodiscard]] bool saveCurrentDocument();
    [[nodiscard]] bool saveDocumentAs();
    [[nodiscard]] UnsavedChoice promptUnsavedChanges();

    void showError(const QString& message);

    AppController controller_;
    CanvasView* canvas_{nullptr};
    LayerDock* layerDock_{nullptr};
    TypographyPanel* typographyPanel_{nullptr};
    ShapePropertiesPanel* shapePropertiesPanel_{nullptr};
    QDockWidget* layersDock_{nullptr};
    QDockWidget* typographyDock_{nullptr};
    QDockWidget* shapeDock_{nullptr};
    QToolBar* toolsToolBar_{nullptr};
    QToolBar* optionsToolBar_{nullptr};
    QWidgetAction* brushOptionsAction_{nullptr};
    QWidgetAction* fillOptionsAction_{nullptr};
    QWidgetAction* selectionOptionsAction_{nullptr};
    QWidgetAction* cropOptionsAction_{nullptr};
    QWidgetAction* hintOptionsAction_{nullptr};
    QLabel* hintLabel_{nullptr};
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
    QAction* imageAdjustmentsAction_{nullptr};
    QAction* resetWorkspaceAction_{nullptr};
    QAction* aboutAction_{nullptr};

    QAction* cropAction_{nullptr};
    QAction* cropApplyAction_{nullptr};
    QAction* cropCancelAction_{nullptr};
    QAction* canvasSizeAction_{nullptr};
    QAction* imageSizeAction_{nullptr};
    QAction* rotateCanvasCWAction_{nullptr};
    QAction* rotateCanvasCCWAction_{nullptr};
    QAction* rotateCanvas180Action_{nullptr};
    QAction* rotateCanvasFreeAction_{nullptr};
    QAction* flipCanvasHorizontalAction_{nullptr};
    QAction* flipCanvasVerticalAction_{nullptr};
    QAction* rotateLayerCWAction_{nullptr};
    QAction* rotateLayerCCWAction_{nullptr};
    QAction* rotateLayer180Action_{nullptr};
    QAction* rotateLayerFreeAction_{nullptr};
    QAction* flipLayerHorizontalAction_{nullptr};
    QAction* flipLayerVerticalAction_{nullptr};
    QAction* scaleLayerAction_{nullptr};
    QAction* translateLayerAction_{nullptr};

    QActionGroup* toolGroup_{nullptr};
    QAction* brushToolAction_{nullptr};
    QAction* eraserToolAction_{nullptr};
    QAction* rectangleSelectAction_{nullptr};
    QAction* ellipseSelectAction_{nullptr};
    QAction* bucketFillAction_{nullptr};
    QAction* textToolAction_{nullptr};
    QAction* shapeRectangleAction_{nullptr};
    QAction* shapeRoundedRectangleAction_{nullptr};
    QAction* shapeEllipseAction_{nullptr};
    QAction* shapeLineAction_{nullptr};
    QAction* shapeArrowAction_{nullptr};
    QAction* shapePolygonAction_{nullptr};
    QAction* clearSelectionAction_{nullptr};
    QAction* invertSelectionAction_{nullptr};
    QToolButton* colorButton_{nullptr};
    QToolButton* fillColorButton_{nullptr};
    QSpinBox* brushSizeSpin_{nullptr};
    QSpinBox* brushOpacitySpin_{nullptr};
    QSpinBox* brushHardnessSpin_{nullptr};
    QSpinBox* fillToleranceSpin_{nullptr};
    QSpinBox* fillOpacitySpin_{nullptr};
    QColor brushColor_{0, 0, 0, 255};

    // Tool shortcuts saved while the text editor owns focus, so single-key
    // shortcuts (B, E, R, T, U, C) do not fire mid-typing.
    QHash<QAction*, QKeySequence> suppressedShortcuts_;
};

} // namespace glyma::app
