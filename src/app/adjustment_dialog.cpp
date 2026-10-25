#include "glyma/app/adjustment_dialog.hpp"

#include <cmath>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

namespace glyma::app {

AdjustmentDialog::AdjustmentDialog(QWidget* parent)
    : QDialog(parent) {
    setObjectName(QStringLiteral("adjustmentDialog"));
    setWindowTitle(tr("Adjustments"));
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    const auto makeSlider = [this](int minimum, int maximum, const char* name) {
        auto* slider = new QSlider(Qt::Horizontal, this);
        slider->setObjectName(QString::fromLatin1(name));
        slider->setRange(minimum, maximum);
        slider->setValue(0);
        slider->setMinimumWidth(220);
        connect(
            slider, &QSlider::valueChanged,
            this, &AdjustmentDialog::onControlChanged
        );
        return slider;
    };

    const auto makeValue = [this]() {
        auto* label = new QLabel(this);
        label->setMinimumWidth(56);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return label;
    };

    const auto addRow = [this, form](
                            const QString& text,
                            QSlider* slider,
                            QLabel* value
                        ) {
        auto* container = new QWidget(this);
        auto* row = new QHBoxLayout(container);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(slider, 1);
        row->addWidget(value);
        form->addRow(text, container);
    };

    exposureSlider_ = makeSlider(-400, 400, "adjustExposureSlider");
    exposureValue_ = makeValue();
    addRow(tr("Exposure:"), exposureSlider_, exposureValue_);

    brightnessSlider_ = makeSlider(-100, 100, "adjustBrightnessSlider");
    brightnessValue_ = makeValue();
    addRow(tr("Brightness:"), brightnessSlider_, brightnessValue_);

    contrastSlider_ = makeSlider(-100, 100, "adjustContrastSlider");
    contrastValue_ = makeValue();
    addRow(tr("Contrast:"), contrastSlider_, contrastValue_);

    saturationSlider_ = makeSlider(-100, 100, "adjustSaturationSlider");
    saturationValue_ = makeValue();
    addRow(tr("Saturation:"), saturationSlider_, saturationValue_);

    blurSlider_ = makeSlider(0, 50, "adjustBlurSlider");
    blurValue_ = makeValue();
    addRow(tr("Blur radius:"), blurSlider_, blurValue_);

    sharpenSlider_ = makeSlider(0, 200, "adjustSharpenSlider");
    sharpenValue_ = makeValue();
    addRow(tr("Sharpen:"), sharpenSlider_, sharpenValue_);

    grayscaleCheck_ = new QCheckBox(tr("Grayscale"), this);
    grayscaleCheck_->setObjectName(QStringLiteral("adjustGrayscaleCheck"));
    connect(
        grayscaleCheck_, &QCheckBox::toggled,
        this, &AdjustmentDialog::onControlChanged
    );
    form->addRow(QString(), grayscaleCheck_);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this
    );
    auto* resetButton = buttons->addButton(
        tr("Reset"), QDialogButtonBox::ResetRole
    );
    connect(resetButton, &QPushButton::clicked, this, &AdjustmentDialog::resetToDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    refreshLabels();
}

core::AdjustmentSettings AdjustmentDialog::settings() const {
    core::AdjustmentSettings settings;
    settings.exposure = static_cast<float>(exposureSlider_->value()) / 100.0F;
    settings.brightness = static_cast<float>(brightnessSlider_->value()) / 100.0F;
    settings.contrast = static_cast<float>(contrastSlider_->value()) / 100.0F;
    settings.saturation = static_cast<float>(saturationSlider_->value()) / 100.0F;
    settings.blurSigma = static_cast<float>(blurSlider_->value());
    settings.sharpen = static_cast<float>(sharpenSlider_->value()) / 100.0F;
    settings.grayscale = grayscaleCheck_->isChecked();
    return settings;
}

void AdjustmentDialog::setSettings(const core::AdjustmentSettings& settings) {
    {
        const QSignalBlocker blockExposure(exposureSlider_);
        const QSignalBlocker blockBrightness(brightnessSlider_);
        const QSignalBlocker blockContrast(contrastSlider_);
        const QSignalBlocker blockSaturation(saturationSlider_);
        const QSignalBlocker blockBlur(blurSlider_);
        const QSignalBlocker blockSharpen(sharpenSlider_);
        const QSignalBlocker blockGrayscale(grayscaleCheck_);

        exposureSlider_->setValue(
            static_cast<int>(std::lround(settings.exposure * 100.0F))
        );
        brightnessSlider_->setValue(
            static_cast<int>(std::lround(settings.brightness * 100.0F))
        );
        contrastSlider_->setValue(
            static_cast<int>(std::lround(settings.contrast * 100.0F))
        );
        saturationSlider_->setValue(
            static_cast<int>(std::lround(settings.saturation * 100.0F))
        );
        blurSlider_->setValue(
            static_cast<int>(std::lround(settings.blurSigma))
        );
        sharpenSlider_->setValue(
            static_cast<int>(std::lround(settings.sharpen * 100.0F))
        );
        grayscaleCheck_->setChecked(settings.grayscale);
    }
    refreshLabels();
}

void AdjustmentDialog::onControlChanged() {
    refreshLabels();
    emit settingsChanged(settings());
}

void AdjustmentDialog::resetToDefaults() {
    setSettings(core::AdjustmentSettings{});
    emit settingsChanged(settings());
}

void AdjustmentDialog::refreshLabels() {
    exposureValue_->setText(
        QString::number(exposureSlider_->value() / 100.0, 'f', 2) +
        QStringLiteral(" EV")
    );
    brightnessValue_->setText(
        QString::number(brightnessSlider_->value()) + QStringLiteral("%")
    );
    contrastValue_->setText(
        QString::number(contrastSlider_->value()) + QStringLiteral("%")
    );
    saturationValue_->setText(
        QString::number(saturationSlider_->value()) + QStringLiteral("%")
    );
    blurValue_->setText(
        QString::number(blurSlider_->value()) + QStringLiteral(" px")
    );
    sharpenValue_->setText(
        QString::number(sharpenSlider_->value()) + QStringLiteral("%")
    );
}

} // namespace glyma::app
