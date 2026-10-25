#include "glyma/app/typography_panel.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

namespace glyma::app {
namespace {

struct WeightOption {
    const char* label;
    int value;
};

constexpr std::array<WeightOption, 9> weightOptions{{
    {"Thin 100", 100},
    {"Extra Light 200", 200},
    {"Light 300", 300},
    {"Regular 400", 400},
    {"Medium 500", 500},
    {"Semi Bold 600", 600},
    {"Bold 700", 700},
    {"Extra Bold 800", 800},
    {"Black 900", 900},
}};

int weightIndexFor(int weight) {
    int best = 3; // Regular
    int bestDistance = 1000000;
    for (int index = 0; index < static_cast<int>(weightOptions.size()); ++index) {
        const int distance = std::abs(weightOptions[index].value - weight);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = index;
        }
    }
    return best;
}

} // namespace

TypographyPanel::TypographyPanel(QWidget* parent)
    : QWidget(parent) {
    buildUi();
    setControlsEnabled(false);
}

void TypographyPanel::buildUi() {
    setObjectName(QStringLiteral("typographyPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    content_ = new QPlainTextEdit(this);
    content_->setObjectName(QStringLiteral("textContentEdit"));
    content_->setPlaceholderText(tr("Type your text"));
    content_->setFixedHeight(72);
    layout->addWidget(content_);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    family_ = new QFontComboBox(this);
    family_->setObjectName(QStringLiteral("textFamilyCombo"));
    form->addRow(tr("Font"), family_);

    size_ = new QDoubleSpinBox(this);
    size_->setObjectName(QStringLiteral("textSizeSpin"));
    size_->setRange(1.0, 100000.0);
    size_->setValue(32.0);
    size_->setDecimals(1);
    size_->setSuffix(QStringLiteral(" px"));
    size_->setKeyboardTracking(false);
    form->addRow(tr("Size"), size_);

    weight_ = new QComboBox(this);
    weight_->setObjectName(QStringLiteral("textWeightCombo"));
    for (const auto& option : weightOptions) {
        weight_->addItem(QString::fromUtf8(option.label), option.value);
    }
    weight_->setCurrentIndex(weightIndexFor(400));
    form->addRow(tr("Weight"), weight_);

    italic_ = new QCheckBox(tr("Italic"), this);
    italic_->setObjectName(QStringLiteral("textItalicCheck"));
    form->addRow(QString(), italic_);

    auto* colorRow = new QHBoxLayout();
    colorRow->setContentsMargins(0, 0, 0, 0);
    colorButton_ = new QToolButton(this);
    colorButton_->setObjectName(QStringLiteral("textColorButton"));
    colorButton_->setToolTip(tr("Text color"));
    colorButton_->setFixedSize(48, 22);
    colorRow->addWidget(colorButton_);
    colorRow->addStretch(1);
    form->addRow(tr("Color"), colorRow);

    horizontalAlign_ = new QComboBox(this);
    horizontalAlign_->setObjectName(QStringLiteral("textAlignHCombo"));
    horizontalAlign_->addItem(tr("Left"), static_cast<int>(core::TextHorizontalAlign::Left));
    horizontalAlign_->addItem(tr("Center"), static_cast<int>(core::TextHorizontalAlign::Center));
    horizontalAlign_->addItem(tr("Right"), static_cast<int>(core::TextHorizontalAlign::Right));
    form->addRow(tr("Align"), horizontalAlign_);

    verticalAlign_ = new QComboBox(this);
    verticalAlign_->setObjectName(QStringLiteral("textAlignVCombo"));
    verticalAlign_->addItem(tr("Top"), static_cast<int>(core::TextVerticalAlign::Top));
    verticalAlign_->addItem(tr("Middle"), static_cast<int>(core::TextVerticalAlign::Middle));
    verticalAlign_->addItem(tr("Bottom"), static_cast<int>(core::TextVerticalAlign::Bottom));
    form->addRow(tr("Baseline"), verticalAlign_);

    lineSpacing_ = new QDoubleSpinBox(this);
    lineSpacing_->setObjectName(QStringLiteral("textLineSpacingSpin"));
    lineSpacing_->setRange(0.1, 100.0);
    lineSpacing_->setSingleStep(0.1);
    lineSpacing_->setValue(1.0);
    lineSpacing_->setDecimals(2);
    lineSpacing_->setKeyboardTracking(false);
    form->addRow(tr("Line height"), lineSpacing_);

    letterSpacing_ = new QDoubleSpinBox(this);
    letterSpacing_->setObjectName(QStringLiteral("textLetterSpacingSpin"));
    letterSpacing_->setRange(-1000.0, 1000.0);
    letterSpacing_->setSingleStep(0.5);
    letterSpacing_->setValue(0.0);
    letterSpacing_->setDecimals(1);
    letterSpacing_->setSuffix(QStringLiteral(" px"));
    letterSpacing_->setKeyboardTracking(false);
    form->addRow(tr("Letter spacing"), letterSpacing_);

    wordWrap_ = new QCheckBox(tr("Word wrap"), this);
    wordWrap_->setObjectName(QStringLiteral("textWordWrapCheck"));
    form->addRow(QString(), wordWrap_);

    autoSize_ = new QCheckBox(tr("Auto size box"), this);
    autoSize_->setObjectName(QStringLiteral("textAutoSizeCheck"));
    autoSize_->setChecked(true);
    form->addRow(QString(), autoSize_);

    boxWidth_ = new QSpinBox(this);
    boxWidth_->setObjectName(QStringLiteral("textBoxWidthSpin"));
    boxWidth_->setRange(1, static_cast<int>(core::TextLayerData::maxBoxDimension));
    boxWidth_->setValue(200);
    boxWidth_->setKeyboardTracking(false);
    form->addRow(tr("Box width"), boxWidth_);

    boxHeight_ = new QSpinBox(this);
    boxHeight_->setObjectName(QStringLiteral("textBoxHeightSpin"));
    boxHeight_->setRange(1, static_cast<int>(core::TextLayerData::maxBoxDimension));
    boxHeight_->setValue(80);
    boxHeight_->setKeyboardTracking(false);
    form->addRow(tr("Box height"), boxHeight_);

    layout->addLayout(form);

    rasterize_ = new QPushButton(tr("Rasterize text"), this);
    rasterize_->setObjectName(QStringLiteral("textRasterizeButton"));
    layout->addWidget(rasterize_);
    layout->addStretch(1);

    connect(content_, &QPlainTextEdit::textChanged, this, [this] { emitEdited(); });
    connect(family_, &QFontComboBox::currentFontChanged, this, [this](const QFont&) {
        emitEdited();
    });
    connect(size_, &QDoubleSpinBox::valueChanged, this, [this](double) { emitEdited(); });
    connect(weight_, &QComboBox::currentIndexChanged, this, [this](int) { emitEdited(); });
    connect(italic_, &QCheckBox::toggled, this, [this](bool) { emitEdited(); });
    connect(
        horizontalAlign_, &QComboBox::currentIndexChanged,
        this, [this](int) { emitEdited(); }
    );
    connect(
        verticalAlign_, &QComboBox::currentIndexChanged,
        this, [this](int) { emitEdited(); }
    );
    connect(
        lineSpacing_, &QDoubleSpinBox::valueChanged,
        this, [this](double) { emitEdited(); }
    );
    connect(
        letterSpacing_, &QDoubleSpinBox::valueChanged,
        this, [this](double) { emitEdited(); }
    );
    connect(wordWrap_, &QCheckBox::toggled, this, [this](bool) { emitEdited(); });
    connect(autoSize_, &QCheckBox::toggled, this, [this](bool checked) {
        boxWidth_->setEnabled(hasLayer_ && !checked);
        boxHeight_->setEnabled(hasLayer_ && !checked);
        emitEdited();
    });
    connect(boxWidth_, &QSpinBox::valueChanged, this, [this](int) { emitEdited(); });
    connect(boxHeight_, &QSpinBox::valueChanged, this, [this](int) { emitEdited(); });
    connect(colorButton_, &QToolButton::clicked, this, [this] { chooseColor(); });
    connect(rasterize_, &QPushButton::clicked, this, &TypographyPanel::rasterizeRequested);

    updateColorButton();
}

void TypographyPanel::setControlsEnabled(bool enabled) {
    content_->setEnabled(enabled);
    family_->setEnabled(enabled);
    size_->setEnabled(enabled);
    weight_->setEnabled(enabled);
    italic_->setEnabled(enabled);
    colorButton_->setEnabled(enabled);
    horizontalAlign_->setEnabled(enabled);
    verticalAlign_->setEnabled(enabled);
    lineSpacing_->setEnabled(enabled);
    letterSpacing_->setEnabled(enabled);
    wordWrap_->setEnabled(enabled);
    autoSize_->setEnabled(enabled);
    boxWidth_->setEnabled(enabled && !autoSize_->isChecked());
    boxHeight_->setEnabled(enabled && !autoSize_->isChecked());
    rasterize_->setEnabled(enabled);
}

void TypographyPanel::setTextData(const core::TextLayerData* data) {
    updating_ = true;
    const QSignalBlocker blockContent(content_);
    const QSignalBlocker blockFamily(family_);
    const QSignalBlocker blockSize(size_);
    const QSignalBlocker blockWeight(weight_);
    const QSignalBlocker blockItalic(italic_);
    const QSignalBlocker blockH(horizontalAlign_);
    const QSignalBlocker blockV(verticalAlign_);
    const QSignalBlocker blockLine(lineSpacing_);
    const QSignalBlocker blockLetter(letterSpacing_);
    const QSignalBlocker blockWrap(wordWrap_);
    const QSignalBlocker blockAuto(autoSize_);
    const QSignalBlocker blockBoxW(boxWidth_);
    const QSignalBlocker blockBoxH(boxHeight_);

    hasLayer_ = data != nullptr;
    if (data != nullptr) {
        content_->setPlainText(QString::fromStdString(data->text));
        family_->setCurrentFont(QFont(QString::fromStdString(data->style.fontFamily)));
        size_->setValue(static_cast<double>(data->style.fontSize));
        weight_->setCurrentIndex(weightIndexFor(data->style.fontWeight));
        italic_->setChecked(data->style.italic);
        color_ = QColor(
            data->style.color.r,
            data->style.color.g,
            data->style.color.b,
            data->style.color.a
        );
        horizontalAlign_->setCurrentIndex(
            static_cast<int>(data->style.horizontalAlign)
        );
        verticalAlign_->setCurrentIndex(
            static_cast<int>(data->style.verticalAlign)
        );
        lineSpacing_->setValue(static_cast<double>(data->style.lineSpacing));
        letterSpacing_->setValue(static_cast<double>(data->style.letterSpacing));
        wordWrap_->setChecked(data->style.wordWrap);
        autoSize_->setChecked(data->autoSize);
        boxWidth_->setValue(
            data->boxWidth > 0 ? static_cast<int>(data->boxWidth) : 1
        );
        boxHeight_->setValue(
            data->boxHeight > 0 ? static_cast<int>(data->boxHeight) : 1
        );
    }

    setControlsEnabled(hasLayer_);
    updateColorButton();
    updating_ = false;
}

core::TextLayerData TypographyPanel::textData() const {
    core::TextLayerData data;
    data.text = content_->toPlainText().toStdString();

    QString family = family_->currentFont().family();
    if (family.isEmpty()) {
        family = QStringLiteral("Sans Serif");
    }
    data.style.fontFamily = family.toStdString();
    data.style.fontSize = static_cast<float>(size_->value());
    data.style.fontWeight = weight_->currentData().toInt();
    data.style.italic = italic_->isChecked();
    data.style.color = core::Pixel{
        static_cast<std::uint8_t>(color_.red()),
        static_cast<std::uint8_t>(color_.green()),
        static_cast<std::uint8_t>(color_.blue()),
        static_cast<std::uint8_t>(color_.alpha()),
    };
    data.style.horizontalAlign = static_cast<core::TextHorizontalAlign>(
        horizontalAlign_->currentData().toInt()
    );
    data.style.verticalAlign = static_cast<core::TextVerticalAlign>(
        verticalAlign_->currentData().toInt()
    );
    data.style.lineSpacing = static_cast<float>(lineSpacing_->value());
    data.style.letterSpacing = static_cast<float>(letterSpacing_->value());
    data.style.wordWrap = wordWrap_->isChecked();
    data.autoSize = autoSize_->isChecked();
    data.boxWidth = static_cast<std::uint32_t>(boxWidth_->value());
    data.boxHeight = static_cast<std::uint32_t>(boxHeight_->value());
    return data;
}

void TypographyPanel::emitEdited() {
    if (updating_ || !hasLayer_) {
        return;
    }
    emit textEdited(textData());
}

void TypographyPanel::chooseColor() {
    const QColor chosen = QColorDialog::getColor(
        color_, this, tr("Text Color"), QColorDialog::ShowAlphaChannel
    );
    if (!chosen.isValid()) {
        return;
    }
    color_ = chosen;
    updateColorButton();
    emitEdited();
}

void TypographyPanel::updateColorButton() {
    colorButton_->setStyleSheet(
        QStringLiteral("background-color: rgba(%1,%2,%3,%4); border: 1px solid #808080;")
            .arg(color_.red())
            .arg(color_.green())
            .arg(color_.blue())
            .arg(color_.alpha())
    );
}

} // namespace glyma::app
