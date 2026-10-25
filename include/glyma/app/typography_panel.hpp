#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "glyma/core/text_layer.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QToolButton;

namespace glyma::app {

// Dockable typography editor for the selected text layer. The panel is purely
// a view over a TextLayerData payload: it never stores engine state and emits
// a fully-formed payload whenever the user changes a control.
class TypographyPanel : public QWidget {
    Q_OBJECT

public:
    explicit TypographyPanel(QWidget* parent = nullptr);

    // Load a text layer payload. Passing nullptr disables the panel and shows
    // a hint that a text layer must be selected.
    void setTextData(const core::TextLayerData* data);

    // Assemble a payload from the current control values. Font family falls
    // back to a default when empty.
    [[nodiscard]] core::TextLayerData textData() const;

signals:
    void textEdited(const core::TextLayerData& data);
    void rasterizeRequested();

private:
    void buildUi();
    void emitEdited();
    void chooseColor();
    void updateColorButton();
    void setControlsEnabled(bool enabled);

    QPlainTextEdit* content_{nullptr};
    QFontComboBox* family_{nullptr};
    QDoubleSpinBox* size_{nullptr};
    QComboBox* weight_{nullptr};
    QCheckBox* italic_{nullptr};
    QToolButton* colorButton_{nullptr};
    QComboBox* horizontalAlign_{nullptr};
    QComboBox* verticalAlign_{nullptr};
    QDoubleSpinBox* lineSpacing_{nullptr};
    QDoubleSpinBox* letterSpacing_{nullptr};
    QCheckBox* wordWrap_{nullptr};
    QCheckBox* autoSize_{nullptr};
    QSpinBox* boxWidth_{nullptr};
    QSpinBox* boxHeight_{nullptr};
    QPushButton* rasterize_{nullptr};

    QColor color_{0, 0, 0, 255};
    bool updating_{false};
    bool hasLayer_{false};
};

} // namespace glyma::app
