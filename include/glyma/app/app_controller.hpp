#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include <QObject>
#include <QString>

#include "glyma/core/adjustments.hpp"
#include "glyma/core/brush.hpp"
#include "glyma/core/brush_engine.hpp"
#include "glyma/core/command_history.hpp"
#include "glyma/core/commands.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/fill.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/selection.hpp"

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

    [[nodiscard]] const core::BrushSettings& brushSettings() const noexcept;
    bool setBrushSettings(const core::BrushSettings& settings);
    [[nodiscard]] core::BrushMode brushMode() const noexcept;
    void setBrushMode(core::BrushMode mode);

    // Drawing API. Positions are document-space coordinates; the controller
    // maps them into the selected layer and rejects hidden/missing layers.
    bool beginBrushStroke(double documentX, double documentY);
    bool updateBrushStroke(double documentX, double documentY);
    bool endBrushStroke();
    bool cancelBrushStroke();
    [[nodiscard]] bool isBrushStrokeActive() const noexcept;

    // Selection API. Positions are document-space coordinates. While a drag is
    // in progress the live selection is held by the controller; committing it
    // pushes an undoable SelectCommand onto the history.
    [[nodiscard]] const core::Selection& selection() const noexcept;
    [[nodiscard]] bool isSelecting() const noexcept;
    [[nodiscard]] core::Selection::Type selectionType() const noexcept;

    void beginSelection(core::Selection::Type type, double documentX, double documentY);
    void updateSelection(double documentX, double documentY);
    void endSelection();
    void cancelSelection();
    void clearSelection();
    void invertSelection();

    // Fill API.
    [[nodiscard]] const core::FillSettings& fillSettings() const noexcept;
    bool setFillSettings(const core::FillSettings& settings);
    // Fill the contiguous region containing the document position on the
    // selected layer, respecting the active selection. Returns true when a
    // non-empty fill was recorded as one undoable command.
    bool bucketFill(double documentX, double documentY);

    // Adjustment session API (destructive raster edits with live preview).
    //
    // A session starts from a stable snapshot of the selected layer. Each
    // update recomputes the preview from that snapshot (never from the previous
    // preview, so repeated slider changes do not accumulate), applies it to the
    // document for display, and records nothing in history. Apply commits the
    // current result as exactly one undoable command; cancel restores the
    // snapshot and leaves history untouched.
    [[nodiscard]] bool isAdjusting() const noexcept;
    [[nodiscard]] const core::AdjustmentSettings& adjustmentSettings() const noexcept;
    // Snapshot the selected visible layer. Returns false when no suitable layer
    // is selected or a session is already active.
    bool beginAdjustment();
    bool updateAdjustment(const core::AdjustmentSettings& settings);
    // Commit the preview as one command. Returns false (without touching
    // history) when nothing changed.
    bool applyAdjustment();
    // Discard the preview, restoring the layer exactly.
    void cancelAdjustment();

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
    void pixelSelectionChanged();
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
    void discardAdjustment();
    [[nodiscard]] std::string nextLayerName() const;

    std::unique_ptr<core::Document> document_;
    std::unique_ptr<core::BrushStroke> brushStroke_;
    core::BrushSettings brushSettings_;
    core::BrushMode brushMode_{core::BrushMode::Paint};
    double strokeOffsetX_{0.0};
    double strokeOffsetY_{0.0};
    std::optional<core::Selection> pendingSelection_;
    core::Selection::Type pendingSelectionType_{core::Selection::Type::None};
    bool selecting_{false};
    double dragStartX_{0.0};
    double dragStartY_{0.0};
    core::CommandHistory history_;
    core::FillSettings fillSettings_;
    std::unique_ptr<core::ImageBuffer> adjustmentBase_;
    core::LayerId adjustmentLayerId_{core::Layer::invalidId};
    core::AdjustmentSettings adjustmentSettings_;
    bool adjusting_{false};
    core::LayerId selectedLayerId_{core::Layer::invalidId};
    QString filePath_;
    std::uint64_t savedRevision_{0};
    bool unsavedContent_{false};
    bool modified_{false};
};

} // namespace glyma::app
