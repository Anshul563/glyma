#include "glyma/app/layer_dock.hpp"

#include <cmath>

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

namespace glyma::app {
namespace {

constexpr int layerIdRole = Qt::UserRole;
constexpr int opacityRole = Qt::UserRole + 1;
constexpr int nameRole = Qt::UserRole + 2;

QToolButton* makeToolButton(
    QWidget* parent,
    const QString& objectName,
    const QString& toolTip,
    const QIcon& icon
) {
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setToolTip(toolTip);
    button->setIcon(icon);
    button->setAutoRaise(true);
    button->setFixedSize(26, 26);
    return button;
}

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
    list_->setEditTriggers(
        QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
    );
    list_->setToolTip(tr("Top layer first. Double-click or press F2 to rename."));
    layout->addWidget(list_, 1);

    auto* actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(4);

    addButton_ = makeToolButton(
        this,
        QStringLiteral("layerAddButton"),
        tr("Add transparent layer"),
        style()->standardIcon(QStyle::SP_FileDialogNewFolder)
    );
    deleteButton_ = makeToolButton(
        this,
        QStringLiteral("layerDeleteButton"),
        tr("Delete selected layer"),
        style()->standardIcon(QStyle::SP_TrashIcon)
    );
    upButton_ = makeToolButton(
        this,
        QStringLiteral("layerUpButton"),
        tr("Move selected layer up"),
        style()->standardIcon(QStyle::SP_ArrowUp)
    );
    downButton_ = makeToolButton(
        this,
        QStringLiteral("layerDownButton"),
        tr("Move selected layer down"),
        style()->standardIcon(QStyle::SP_ArrowDown)
    );

    actionRow->addWidget(addButton_);
    actionRow->addWidget(deleteButton_);
    actionRow->addWidget(upButton_);
    actionRow->addWidget(downButton_);
    actionRow->addStretch(1);
    layout->addLayout(actionRow);

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
    connect(addButton_, &QToolButton::clicked, this, &LayerDock::addLayerRequested);
    connect(
        deleteButton_, &QToolButton::clicked,
        this, &LayerDock::deleteLayerRequested
    );
    connect(
        upButton_, &QToolButton::clicked,
        this, &LayerDock::moveLayerUpRequested
    );
    connect(
        downButton_, &QToolButton::clicked,
        this, &LayerDock::moveLayerDownRequested
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
            Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable |
            Qt::ItemIsEditable
        );
        item->setCheckState(it->visible() ? Qt::Checked : Qt::Unchecked);
        item->setData(layerIdRole, QVariant::fromValue<qulonglong>(it->id()));
        item->setData(
            opacityRole,
            static_cast<int>(std::lround(it->opacity() * 100.0F))
        );
        item->setData(nameRole, QString::fromStdString(it->name()));
        if (it->id() == selectedId) {
            selectedItem = item;
        }
    }

    selectedId_ = selectedItem != nullptr ? selectedId : 0;
    list_->setCurrentItem(selectedItem);
    updateOpacityControls(selectedItem);
    updateActionButtons();
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
    updateActionButtons();
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
        updateActionButtons();
        return;
    }

    selectedId_ = item->data(layerIdRole).toULongLong();
    updateOpacityControls(item);
    updateActionButtons();
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

    const QString storedName = item->data(nameRole).toString();
    if (item->text() != storedName) {
        const QString newName = item->text().trimmed();
        if (newName.isEmpty() || newName == storedName) {
            const QSignalBlocker blocker(list_);
            item->setText(storedName);
            return;
        }
        if (newName != item->text()) {
            const QSignalBlocker blocker(list_);
            item->setText(newName);
        }
        emit layerRenamed(id, newName);
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

void LayerDock::updateActionButtons() {
    QListWidgetItem* item = list_->currentItem();
    const bool hasSelection = item != nullptr;

    deleteButton_->setEnabled(hasSelection);
    upButton_->setEnabled(
        hasSelection && list_->row(item) > 0
    );
    downButton_->setEnabled(
        hasSelection && list_->row(item) + 1 < list_->count()
    );
}

} // namespace glyma::app
