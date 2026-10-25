#include "glyma/app/shape_properties_panel.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

#include "glyma/app/icon_manager.hpp"

namespace glyma::app {
namespace {

QColor toQColor(core::Pixel pixel) {
    return QColor(pixel.r, pixel.g, pixel.b, pixel.a);
}

core::Pixel toPixel(const QColor& color) {
    return core::Pixel{
        static_cast<std::uint8_t>(color.red()),
        static_cast<std::uint8_t>(color.green()),
        static_cast<std::uint8_t>(color.blue()),
        static_cast<std::uint8_t>(color.alpha()),
    };
}

} // namespace

ShapePropertiesPanel::ShapePropertiesPanel(QWidget* parent)
    : QWidget(parent) {
    buildUi();
    setControlsEnabled(false);
}

void ShapePropertiesPanel::buildUi() {
    setObjectName(QStringLiteral("shapePropertiesPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    type_ = new QComboBox(this);
    type_->setObjectName(QStringLiteral("shapeTypeCombo"));
    type_->addItem(tr("Rectangle"), static_cast<int>(core::ShapeType::Rectangle));
    type_->addItem(tr("Rounded rectangle"), static_cast<int>(core::ShapeType::RoundedRectangle));
    type_->addItem(tr("Ellipse"), static_cast<int>(core::ShapeType::Ellipse));
    type_->addItem(tr("Line"), static_cast<int>(core::ShapeType::Line));
    type_->addItem(tr("Arrow"), static_cast<int>(core::ShapeType::Arrow));
    type_->addItem(tr("Polygon"), static_cast<int>(core::ShapeType::Polygon));
    form->addRow(tr("Shape"), type_);

    auto* fillRow = new QHBoxLayout();
    fillRow->setContentsMargins(0, 0, 0, 0);
    fillEnabled_ = new QCheckBox(tr("Fill"), this);
    fillEnabled_->setObjectName(QStringLiteral("shapeFillCheck"));
    fillRow->addWidget(fillEnabled_);
    fillColor_ = new QToolButton(this);
    fillColor_->setObjectName(QStringLiteral("shapeFillColorButton"));
    fillColor_->setToolTip(tr("Fill color"));
    fillColor_->setFixedSize(48, 22);
    fillRow->addWidget(fillColor_);
    fillRow->addStretch(1);
    form->addRow(QString(), fillRow);

    auto* strokeRow = new QHBoxLayout();
    strokeRow->setContentsMargins(0, 0, 0, 0);
    strokeEnabled_ = new QCheckBox(tr("Stroke"), this);
    strokeEnabled_->setObjectName(QStringLiteral("shapeStrokeCheck"));
    strokeRow->addWidget(strokeEnabled_);
    strokeColor_ = new QToolButton(this);
    strokeColor_->setObjectName(QStringLiteral("shapeStrokeColorButton"));
    strokeColor_->setToolTip(tr("Stroke color"));
    strokeColor_->setFixedSize(48, 22);
    strokeRow->addWidget(strokeColor_);
    strokeRow->addStretch(1);
    form->addRow(QString(), strokeRow);

    strokeWidth_ = new QDoubleSpinBox(this);
    strokeWidth_->setObjectName(QStringLiteral("shapeStrokeWidthSpin"));
    strokeWidth_->setRange(0.0, static_cast<double>(core::ShapeLayerData::maxStrokeWidth));
    strokeWidth_->setValue(2.0);
    strokeWidth_->setDecimals(1);
    strokeWidth_->setSuffix(QStringLiteral(" px"));
    strokeWidth_->setKeyboardTracking(false);
    form->addRow(tr("Stroke width"), strokeWidth_);

    strokeStyle_ = new QComboBox(this);
    strokeStyle_->setObjectName(QStringLiteral("shapeStrokeStyleCombo"));
    strokeStyle_->addItem(tr("Solid"), static_cast<int>(core::StrokeStyle::Solid));
    strokeStyle_->addItem(tr("Dashed"), static_cast<int>(core::StrokeStyle::Dashed));
    strokeStyle_->addItem(tr("Dotted"), static_cast<int>(core::StrokeStyle::Dotted));
    form->addRow(tr("Stroke style"), strokeStyle_);

    cornerRadius_ = new QDoubleSpinBox(this);
    cornerRadius_->setObjectName(QStringLiteral("shapeCornerRadiusSpin"));
    cornerRadius_->setRange(0.0, core::ShapeLayerData::maxDimension);
    cornerRadius_->setValue(12.0);
    cornerRadius_->setDecimals(1);
    cornerRadius_->setSuffix(QStringLiteral(" px"));
    cornerRadius_->setKeyboardTracking(false);
    form->addRow(tr("Corner radius"), cornerRadius_);

    rotation_ = new QDoubleSpinBox(this);
    rotation_->setObjectName(QStringLiteral("shapeRotationSpin"));
    rotation_->setRange(-360.0, 360.0);
    rotation_->setValue(0.0);
    rotation_->setDecimals(1);
    rotation_->setSuffix(QStringLiteral(" deg"));
    rotation_->setWrapping(true);
    rotation_->setKeyboardTracking(false);
    form->addRow(tr("Rotation"), rotation_);

    auto* arrowRow = new QHBoxLayout();
    arrowRow->setContentsMargins(0, 0, 0, 0);
    arrowStart_ = new QCheckBox(tr("Start"), this);
    arrowStart_->setObjectName(QStringLiteral("shapeArrowStartCheck"));
    arrowRow->addWidget(arrowStart_);
    arrowEnd_ = new QCheckBox(tr("End"), this);
    arrowEnd_->setObjectName(QStringLiteral("shapeArrowEndCheck"));
    arrowEnd_->setChecked(true);
    arrowRow->addWidget(arrowEnd_);
    arrowRow->addStretch(1);
    form->addRow(tr("Arrowheads"), arrowRow);

    arrowLength_ = new QDoubleSpinBox(this);
    arrowLength_->setObjectName(QStringLiteral("shapeArrowLengthSpin"));
    arrowLength_->setRange(0.0, static_cast<double>(core::ShapeLayerData::maxStrokeWidth));
    arrowLength_->setValue(14.0);
    arrowLength_->setDecimals(1);
    arrowLength_->setKeyboardTracking(false);
    form->addRow(tr("Head length"), arrowLength_);

    arrowWidth_ = new QDoubleSpinBox(this);
    arrowWidth_->setObjectName(QStringLiteral("shapeArrowWidthSpin"));
    arrowWidth_->setRange(0.0, static_cast<double>(core::ShapeLayerData::maxStrokeWidth));
    arrowWidth_->setValue(10.0);
    arrowWidth_->setDecimals(1);
    arrowWidth_->setKeyboardTracking(false);
    form->addRow(tr("Head width"), arrowWidth_);

    layout->addLayout(form);

    rasterize_ = new QPushButton(tr("Rasterize shape"), this);
    rasterize_->setIcon(IconManager::icon(QStringLiteral("rasterize")));
    rasterize_->setIconSize(IconManager::panelIconSize());
    rasterize_->setToolTip(tr("Bake the shape into layer pixels"));
    rasterize_->setObjectName(QStringLiteral("shapeRasterizeButton"));
    layout->addWidget(rasterize_);
    layout->addStretch(1);

    connect(type_, &QComboBox::currentIndexChanged, this, [this](int) {
        updateTypeControls();
        emitEdited();
    });
    connect(fillEnabled_, &QCheckBox::toggled, this, [this](bool) { emitEdited(); });
    connect(strokeEnabled_, &QCheckBox::toggled, this, [this](bool) { emitEdited(); });
    connect(
        strokeWidth_, &QDoubleSpinBox::valueChanged,
        this, [this](double) { emitEdited(); }
    );
    connect(
        strokeStyle_, &QComboBox::currentIndexChanged,
        this, [this](int) { emitEdited(); }
    );
    connect(
        cornerRadius_, &QDoubleSpinBox::valueChanged,
        this, [this](double) { emitEdited(); }
    );
    connect(rotation_, &QDoubleSpinBox::valueChanged, this, [this](double) { emitEdited(); });
    connect(arrowStart_, &QCheckBox::toggled, this, [this](bool) { emitEdited(); });
    connect(arrowEnd_, &QCheckBox::toggled, this, [this](bool) { emitEdited(); });
    connect(
        arrowLength_, &QDoubleSpinBox::valueChanged,
        this, [this](double) { emitEdited(); }
    );
    connect(
        arrowWidth_, &QDoubleSpinBox::valueChanged,
        this, [this](double) { emitEdited(); }
    );
    connect(fillColor_, &QToolButton::clicked, this, [this] { chooseFillColor(); });
    connect(strokeColor_, &QToolButton::clicked, this, [this] { chooseStrokeColor(); });
    connect(rasterize_, &QPushButton::clicked, this, &ShapePropertiesPanel::rasterizeRequested);

    updateColorButtons();
    updateTypeControls();
}

void ShapePropertiesPanel::setControlsEnabled(bool enabled) {
    type_->setEnabled(enabled);
    fillEnabled_->setEnabled(enabled);
    fillColor_->setEnabled(enabled);
    strokeEnabled_->setEnabled(enabled);
    strokeColor_->setEnabled(enabled);
    strokeWidth_->setEnabled(enabled);
    strokeStyle_->setEnabled(enabled);
    cornerRadius_->setEnabled(enabled);
    rotation_->setEnabled(enabled);
    arrowStart_->setEnabled(enabled);
    arrowEnd_->setEnabled(enabled);
    arrowLength_->setEnabled(enabled);
    arrowWidth_->setEnabled(enabled);
    rasterize_->setEnabled(enabled);
}

void ShapePropertiesPanel::updateTypeControls() {
    const auto shapeType = static_cast<core::ShapeType>(type_->currentData().toInt());
    const bool isRounded = shapeType == core::ShapeType::RoundedRectangle;
    const bool isArrow = shapeType == core::ShapeType::Arrow;
    const bool isClosed = core::isBoxShape(shapeType) ||
                          shapeType == core::ShapeType::Polygon;

    cornerRadius_->setEnabled(hasLayer_ && isRounded);
    arrowStart_->setEnabled(hasLayer_ && isArrow);
    arrowEnd_->setEnabled(hasLayer_ && isArrow);
    arrowLength_->setEnabled(hasLayer_ && isArrow);
    arrowWidth_->setEnabled(hasLayer_ && isArrow);
    fillEnabled_->setEnabled(hasLayer_ && isClosed);
    fillColor_->setEnabled(hasLayer_ && isClosed && fillEnabled_->isChecked());
}

void ShapePropertiesPanel::setShapeData(const core::ShapeLayerData* data) {
    updating_ = true;
    const QSignalBlocker blockType(type_);
    const QSignalBlocker blockFill(fillEnabled_);
    const QSignalBlocker blockStroke(strokeEnabled_);
    const QSignalBlocker blockWidth(strokeWidth_);
    const QSignalBlocker blockStyle(strokeStyle_);
    const QSignalBlocker blockCorner(cornerRadius_);
    const QSignalBlocker blockRotation(rotation_);
    const QSignalBlocker blockArrowStart(arrowStart_);
    const QSignalBlocker blockArrowEnd(arrowEnd_);
    const QSignalBlocker blockArrowLength(arrowLength_);
    const QSignalBlocker blockArrowWidth(arrowWidth_);

    hasLayer_ = data != nullptr;
    if (data != nullptr) {
        base_ = *data;
        type_->setCurrentIndex(type_->findData(static_cast<int>(data->type)));
        fillEnabled_->setChecked(data->fill.enabled);
        fillColorValue_ = toQColor(data->fill.color);
        strokeEnabled_->setChecked(data->stroke.enabled);
        strokeColorValue_ = toQColor(data->stroke.color);
        strokeWidth_->setValue(static_cast<double>(data->stroke.width));
        strokeStyle_->setCurrentIndex(
            strokeStyle_->findData(static_cast<int>(data->stroke.style))
        );
        cornerRadius_->setValue(static_cast<double>(data->cornerRadius));
        rotation_->setValue(data->rotation);
        arrowStart_->setChecked(data->arrow.start);
        arrowEnd_->setChecked(data->arrow.end);
        arrowLength_->setValue(static_cast<double>(data->arrow.length));
        arrowWidth_->setValue(static_cast<double>(data->arrow.width));
    }

    setControlsEnabled(hasLayer_);
    updateTypeControls();
    updateColorButtons();
    updating_ = false;
}

core::ShapeLayerData ShapePropertiesPanel::shapeData() const {
    core::ShapeLayerData data = base_;
    data.type = static_cast<core::ShapeType>(type_->currentData().toInt());
    data.fill.enabled = fillEnabled_->isChecked();
    data.fill.color = toPixel(fillColorValue_);
    data.stroke.enabled = strokeEnabled_->isChecked();
    data.stroke.color = toPixel(strokeColorValue_);
    data.stroke.width = static_cast<float>(strokeWidth_->value());
    data.stroke.style = static_cast<core::StrokeStyle>(strokeStyle_->currentData().toInt());
    data.cornerRadius = static_cast<float>(cornerRadius_->value());
    data.rotation = rotation_->value();
    data.arrow.start = arrowStart_->isChecked();
    data.arrow.end = arrowEnd_->isChecked();
    data.arrow.length = static_cast<float>(arrowLength_->value());
    data.arrow.width = static_cast<float>(arrowWidth_->value());
    core::sanitizeShape(data);
    return data;
}

void ShapePropertiesPanel::emitEdited() {
    if (updating_ || !hasLayer_) {
        return;
    }
    emit shapeEdited(shapeData());
}

void ShapePropertiesPanel::chooseFillColor() {
    const QColor chosen = QColorDialog::getColor(
        fillColorValue_, this, tr("Fill Color"), QColorDialog::ShowAlphaChannel
    );
    if (!chosen.isValid()) {
        return;
    }
    fillColorValue_ = chosen;
    updateColorButtons();
    emitEdited();
}

void ShapePropertiesPanel::chooseStrokeColor() {
    const QColor chosen = QColorDialog::getColor(
        strokeColorValue_, this, tr("Stroke Color"), QColorDialog::ShowAlphaChannel
    );
    if (!chosen.isValid()) {
        return;
    }
    strokeColorValue_ = chosen;
    updateColorButtons();
    emitEdited();
}

void ShapePropertiesPanel::updateColorButtons() {
    const auto style = [](const QColor& color) {
        return QStringLiteral(
                   "background-color: rgba(%1,%2,%3,%4); border: 1px solid #808080;"
               )
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.alpha());
    };
    fillColor_->setStyleSheet(style(fillColorValue_));
    strokeColor_->setStyleSheet(style(strokeColorValue_));
}

} // namespace glyma::app
