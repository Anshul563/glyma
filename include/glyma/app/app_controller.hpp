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
#include "glyma/core/shape_commands.hpp"
#include "glyma/core/shape_layer.hpp"
#include "glyma/core/text_commands.hpp"
#include "glyma/core/text_layer.hpp"
#include "glyma/core/transform.hpp"

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
    // Render a sub-rectangle of the composited canvas (clipped to bounds). Used
    // by the canvas to repaint only the region touched by an incremental edit.
    [[nodiscard]] core::ImageBuffer renderRegion(
        int x,
        int y,
        int width,
        int height
    ) const;
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

    // Document transformation API. Each successful call records exactly one
    // undoable command. Invalid requests return false and leave the document
    // untouched (invalid input is also reported through errorOccurred).
    bool crop(const core::CropRect& rect);
    bool resizeCanvas(
        std::uint32_t width,
        std::uint32_t height,
        core::Anchor anchor = core::Anchor::Center
    );
    bool resizeImage(std::uint32_t width, std::uint32_t height);
    bool rotateCanvas(core::DocumentRotation rotation);
    bool rotateCanvas(double degrees);
    bool rotateCanvas90(bool clockwise);
    bool rotateCanvas180();
    bool flipCanvas(bool horizontal);

    // Layer transformation API. Rotations preserve the layer centre.
    bool transformLayer(core::LayerId id, core::LayerTransform transform);
    bool rotateLayer(core::LayerId id, double degrees);
    bool scaleLayer(
        core::LayerId id,
        std::uint32_t width,
        std::uint32_t height
    );
    bool translateLayer(
        core::LayerId id,
        std::int32_t deltaX,
        std::int32_t deltaY
    );

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

    // Text layer API. Adding, editing, and rasterizing each record exactly one
    // undoable command. Text layer buffers hold a regenerable render cache that
    // is refreshed automatically after every document change.
    bool addTextLayer(
        const core::TextLayerData& data,
        std::int32_t x = 0,
        std::int32_t y = 0
    );
    bool setTextData(core::LayerId id, const core::TextLayerData& data);
    bool rasterizeTextLayer(core::LayerId id);
    [[nodiscard]] const core::TextLayerData* textData(core::LayerId id) const
        noexcept;

    // Text editing session. While a session is open, updateTextData() applies
    // content/typography changes live for preview but records nothing in
    // history, so a whole typing run (or a burst of typography tweaks) collapses
    // into one undoable change on commitTextEdit(). cancelTextEdit() restores
    // the exact payload captured at beginTextEdit(). Starting a new session (or
    // committing/cancelling) closes any previous one.
    bool beginTextEdit(core::LayerId id);
    bool updateTextData(core::LayerId id, const core::TextLayerData& data);
    bool commitTextEdit();
    void cancelTextEdit();
    [[nodiscard]] bool isEditingText() const noexcept;
    [[nodiscard]] core::LayerId textEditLayerId() const noexcept;
    // Hit test the topmost text layer whose rendered cache is opaque at the
    // document position. Returns Layer::invalidId when nothing is hit.
    [[nodiscard]] core::LayerId textLayerAt(
        double documentX,
        double documentY
    ) const noexcept;

    // Vector shape layer API. Adding, editing, and rasterizing each record
    // exactly one undoable command. Vector layer buffers hold a regenerable
    // render cache that is refreshed automatically after every document change.
    bool addShapeLayer(const core::ShapeLayerData& data);
    bool setShapeData(core::LayerId id, const core::ShapeLayerData& data);
    bool rasterizeShapeLayer(core::LayerId id);
    [[nodiscard]] const core::ShapeLayerData* shapeData(core::LayerId id) const
        noexcept;
    // Hit test the topmost vector layer at a document position. Returns
    // core::Layer::invalidId when no vector layer is hit.
    [[nodiscard]] core::LayerId shapeLayerAt(
        double documentX,
        double documentY,
        double tolerance
    ) const noexcept;
    void renameLayer(core::LayerId id, const QString& name);
    void moveLayerUp(core::LayerId id);
    void moveLayerDown(core::LayerId id);
    void setLayerVisibility(core::LayerId id, bool visible);
    void setLayerOpacity(core::LayerId id, float opacity);

signals:
    void documentChanged();
    // Emitted for incremental edits (brush strokes) so the canvas can repaint
    // only the touched region instead of recompositing the whole document. The
    // rectangle is in document pixel coordinates, already clipped to bounds.
    void documentRegionChanged(int x, int y, int width, int height);
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
    void refreshTextCaches();
    void refreshShapeCaches();
    void refreshModifiedState();
    void emitBrushRegion();
    // Close any open text-editing session before a history-mutating operation.
    void commitPendingTextEdit();
    bool commitTransform(core::CommandPtr command);
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
    std::optional<core::TextLayerData> textEditOriginal_;
    core::LayerId textEditLayerId_{core::Layer::invalidId};
    bool editingText_{false};
    core::LayerId selectedLayerId_{core::Layer::invalidId};
    QString filePath_;
    std::uint64_t savedRevision_{0};
    bool unsavedContent_{false};
    bool modified_{false};
};

} // namespace glyma::app
