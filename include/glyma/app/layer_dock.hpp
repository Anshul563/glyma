#pragma once

#include <QWidget>

#include "glyma/core/layer_stack.hpp"

class QListWidget;
class QListWidgetItem;
class QSpinBox;

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
    void visibilityToggled(quint64 layerId, bool visible);
    void opacityChanged(quint64 layerId, int percent);

private:
    void handleCurrentRowChanged(int row);
    void handleItemChanged(QListWidgetItem* item);
    void updateOpacityControls(QListWidgetItem* item);

    QListWidget* list_{nullptr};
    QSpinBox* opacity_{nullptr};
    bool updating_{false};
    quint64 selectedId_{0};
};

} // namespace glyma::app
