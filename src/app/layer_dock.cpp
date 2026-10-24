#include "glyma/app/layer_dock.hpp"

#include <cmath>

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>

namespace glyma::app {
namespace {

constexpr int layerIdRole = Qt::UserRole;
constexpr int opacityRole = Qt::UserRole + 1;

} // namespace

LayerDock::LayerDock(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("layerDock"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("layersList"));
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setAlternatingRowColors(false);
    list_->setUniformItemSizes(true);
    list_->setToolTip(tr("Top layer first. Check a layer to show it."));
    layout->addWidget(list_, 1);

    auto* opacityRow = new QHBoxLayout();
    opacityRow->setContentsMargins(0, 0, 0, 0);
    auto* opacityLabel = new QLabel(tr("Opacity"), this);
    opacity_ = new QSpinBox(this);
    opacity_->setObjectName(QStringLiteral("opacitySpin"));
    opacity_->setRange(0, 100);
    opacity_->setSuffix(QStringLiteral("%"));
    opacity_->setKeyboardTracking(false);
    opacity_->setEnabled(false);
    opacityRow->addWidget(opacityLabel);
    opacityRow->addWidget(opacity_, 1);
    layout->addLayout(opacityRow);

    connect(
        list_, &QListWidget::currentRowChanged,
        this, &LayerDock::handleCurrentRowChanged
    );
    connect(
        list_, &QListWidget::itemChanged,
        this, &LayerDock::handleItemChanged
    );
    connect(
        opacity_, &QSpinBox::valueChanged,
        this, [this](int value) {
            if (updating_ || selectedId_ == 0) {
                return;
            }
            emit opacityChanged(selectedId_, value);
        }
    );
}

void LayerDock::refresh(const core::LayerStack& stack, core::LayerId selectedId) {
    updating_ = true;
    const QSignalBlocker listBlocker(list_);
    const QSignalBlocker opacityBlocker(opacity_);

    list_->clear();

    QListWidgetItem* selectedItem = nullptr;
    const auto& layers = stack.layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        auto* item = new QListWidgetItem(QString::fromStdString(it->name()), list_);
        item->setFlags(
            (item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable
        );
        item->setCheckState(it->visible() ? Qt::Checked : Qt::Unchecked);
        item->setData(layerIdRole, QVariant::fromValue<qulonglong>(it->id()));
        item->setData(
            opacityRole,
            static_cast<int>(std::lround(it->opacity() * 100.0F))
        );
        if (it->id() == selectedId) {
            selectedItem = item;
        }
    }

    selectedId_ = selectedItem != nullptr ? selectedId : 0;
    list_->setCurrentItem(selectedItem);
    updateOpacityControls(selectedItem);
    updating_ = false;
}

void LayerDock::setSelectedLayer(core::LayerId id) {
    if (id == selectedId_ && list_->currentItem() != nullptr) {
        return;
    }

    QListWidgetItem* target = nullptr;
    for (int row = 0; row < list_->count(); ++row) {
        QListWidgetItem* item = list_->item(row);
        if (item->data(layerIdRole).toULongLong() == id) {
            target = item;
            break;
        }
    }

    updating_ = true;
    const QSignalBlocker blocker(list_);
    const QSignalBlocker opacityBlocker(opacity_);
    list_->setCurrentItem(target);
    selectedId_ = target != nullptr
        ? target->data(layerIdRole).toULongLong()
        : 0;
    updateOpacityControls(target);
    updating_ = false;
}

void LayerDock::handleCurrentRowChanged(int row) {
    if (updating_) {
        return;
    }

    QListWidgetItem* item = row >= 0 ? list_->item(row) : nullptr;
    if (item == nullptr) {
        selectedId_ = 0;
        updateOpacityControls(nullptr);
        return;
    }

    selectedId_ = item->data(layerIdRole).toULongLong();
    updateOpacityControls(item);
    emit layerSelected(selectedId_);
}

void LayerDock::handleItemChanged(QListWidgetItem* item) {
    if (updating_ || item == nullptr) {
        return;
    }

    const auto id = item->data(layerIdRole).toULongLong();
    if (id == 0) {
        return;
    }
    emit visibilityToggled(id, item->checkState() == Qt::Checked);
}

void LayerDock::updateOpacityControls(QListWidgetItem* item) {
    const QSignalBlocker blocker(opacity_);
    if (item == nullptr) {
        opacity_->setValue(0);
        opacity_->setEnabled(false);
        return;
    }
    opacity_->setValue(item->data(opacityRole).toInt());
    opacity_->setEnabled(true);
}

} // namespace glyma::app
