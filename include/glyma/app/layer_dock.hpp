#pragma once

#include <QString>
#include <QWidget>

#include "glyma/core/layer_stack.hpp"

class QListWidget;
class QListWidgetItem;
class QSpinBox;
class QToolButton;

namespace glyma::app {

// Dockable panel showing the document's layer stack. Layer state is always
// read from the engine; the panel only caches presentation values per item.
class LayerDock : public QWidget {
    Q_OBJECT

public:
    explicit LayerDock(QWidget* parent = nullptr);

    void refresh(const core::LayerStack& stack, core::LayerId selectedId);
    void setSelectedLayer(core::LayerId id);

signals:
    void layerSelected(quint64 layerId);
    void addLayerRequested();
    void deleteLayerRequested();
    void moveLayerUpRequested();
    void moveLayerDownRequested();
    void visibilityToggled(quint64 layerId, bool visible);
    void opacityChanged(quint64 layerId, int percent);
    void layerRenamed(quint64 layerId, const QString& name);

private:
    void handleCurrentRowChanged(int row);
    void handleItemChanged(QListWidgetItem* item);
    void updateOpacityControls(QListWidgetItem* item);
    void updateActionButtons();

    QListWidget* list_{nullptr};
    QSpinBox* opacity_{nullptr};
    QToolButton* addButton_{nullptr};
    QToolButton* deleteButton_{nullptr};
    QToolButton* upButton_{nullptr};
    QToolButton* downButton_{nullptr};
    bool updating_{false};
    quint64 selectedId_{0};
};

} // namespace glyma::app
