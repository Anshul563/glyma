#pragma once

#include <cstdint>
#include <memory>

#include <QObject>
#include <QString>

#include "glyma/core/command_history.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"

namespace glyma::app {

// Owns the active document and its undo history, and exposes guarded
// operations to the UI. Widgets never hold document state of their own.
class AppController : public QObject {
    Q_OBJECT

public:
    explicit AppController(QObject* parent = nullptr);

    [[nodiscard]] const core::Document& document() const noexcept;
    [[nodiscard]] core::Document& document() noexcept;
    [[nodiscard]] core::ImageBuffer render() const;
    [[nodiscard]] core::LayerId selectedLayerId() const noexcept;
    [[nodiscard]] const QString& filePath() const noexcept;

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] bool isModified() const noexcept;

    bool newDocument(
        const QString& name,
        std::uint32_t width,
        std::uint32_t height,
        bool solidBackground = false,
        core::Pixel backgroundColor = core::Pixel{255, 255, 255, 255}
    );
    bool openFile(const QString& path);
    bool save(const QString& path);
    bool exportPng(const QString& path);

    bool undo();
    bool redo();

    void selectLayer(core::LayerId id);
    void addLayer();
    void removeLayer(core::LayerId id);
    void renameLayer(core::LayerId id, const QString& name);
    void moveLayerUp(core::LayerId id);
    void moveLayerDown(core::LayerId id);
    void setLayerVisibility(core::LayerId id, bool visible);
    void setLayerOpacity(core::LayerId id, float opacity);

signals:
    void documentChanged();
    void layersChanged();
    void selectionChanged(quint64 layerId);
    void historyChanged();
    void filePathChanged(const QString& path);
    void errorOccurred(const QString& message);
    void modifiedChanged(bool modified);

private:
    void installDocument(
        std::unique_ptr<core::Document> document,
        const QString& path,
        bool modifiedOnLoad = false
    );
    void ensureSelection();
    void emitDocumentState(bool selectionMayHaveChanged);
    void refreshModifiedState();
    [[nodiscard]] std::string nextLayerName() const;

    std::unique_ptr<core::Document> document_;
    core::CommandHistory history_;
    core::LayerId selectedLayerId_{core::Layer::invalidId};
    QString filePath_;
    std::uint64_t savedRevision_{0};
    bool unsavedContent_{false};
    bool modified_{false};
};

} // namespace glyma::app
