#pragma once

#include <QDialog>

#include "glyma/core/adjustments.hpp"

class QCheckBox;
class QLabel;
class QSlider;

namespace glyma::app {

// Compact adjustment panel with live preview controls.
//
// The dialog owns no document state: it exposes the current parameter values
// through settings() and emits settingsChanged() whenever a control moves.
// The owning window drives preview, apply, and cancel on the controller.
class AdjustmentDialog : public QDialog {
    Q_OBJECT

public:
    explicit AdjustmentDialog(QWidget* parent = nullptr);

    [[nodiscard]] core::AdjustmentSettings settings() const;
    void setSettings(const core::AdjustmentSettings& settings);

signals:
    void settingsChanged(const core::AdjustmentSettings& settings);

private slots:
    void onControlChanged();
    void resetToDefaults();

private:
    void refreshLabels();

    QSlider* exposureSlider_{nullptr};
    QSlider* brightnessSlider_{nullptr};
    QSlider* contrastSlider_{nullptr};
    QSlider* saturationSlider_{nullptr};
    QSlider* blurSlider_{nullptr};
    QSlider* sharpenSlider_{nullptr};
    QCheckBox* grayscaleCheck_{nullptr};

    QLabel* exposureValue_{nullptr};
    QLabel* brightnessValue_{nullptr};
    QLabel* contrastValue_{nullptr};
    QLabel* saturationValue_{nullptr};
    QLabel* blurValue_{nullptr};
    QLabel* sharpenValue_{nullptr};
};

} // namespace glyma::app
