#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "glyma/core/shape_layer.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QToolButton;

namespace glyma::app {

// Dockable editor for the selected vector layer. The panel is purely a view
// over a ShapeLayerData payload: it never stores engine state and emits a
// fully-formed payload whenever the user changes a control. Geometry (position,
// size, endpoints, vertices) is owned by the canvas; this panel edits style,
// rotation, corner radius, and arrowheads.
class ShapePropertiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit ShapePropertiesPanel(QWidget* parent = nullptr);

    // Load a shape layer payload. Passing nullptr disables the panel and shows
    // a hint that a vector layer must be selected.
    void setShapeData(const core::ShapeLayerData* data);

    // Assemble a payload from the loaded geometry plus the current controls.
    [[nodiscard]] core::ShapeLayerData shapeData() const;

signals:
    void shapeEdited(const core::ShapeLayerData& data);
    void rasterizeRequested();

private:
    void buildUi();
    void emitEdited();
    void chooseFillColor();
    void chooseStrokeColor();
    void updateColorButtons();
    void updateTypeControls();
    void setControlsEnabled(bool enabled);

    QComboBox* type_{nullptr};
    QCheckBox* fillEnabled_{nullptr};
    QToolButton* fillColor_{nullptr};
    QCheckBox* strokeEnabled_{nullptr};
    QToolButton* strokeColor_{nullptr};
    QDoubleSpinBox* strokeWidth_{nullptr};
    QComboBox* strokeStyle_{nullptr};
    QDoubleSpinBox* cornerRadius_{nullptr};
    QDoubleSpinBox* rotation_{nullptr};
    QCheckBox* arrowStart_{nullptr};
    QCheckBox* arrowEnd_{nullptr};
    QDoubleSpinBox* arrowLength_{nullptr};
    QDoubleSpinBox* arrowWidth_{nullptr};
    QPushButton* rasterize_{nullptr};

    core::ShapeLayerData base_;
    QColor fillColorValue_{210, 210, 214, 255};
    QColor strokeColorValue_{24, 24, 28, 255};
    bool updating_{false};
    bool hasLayer_{false};
};

} // namespace glyma::app
