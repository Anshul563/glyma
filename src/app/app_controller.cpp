#include "glyma/app/app_controller.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>

#include <QFileInfo>

#include "glyma/core/commands.hpp"
#include "glyma/core/pixel_edit.hpp"
#include "glyma/io/document_io.hpp"
#include "glyma/io/png_reader.hpp"
#include "glyma/io/project_reader.hpp"
#include "glyma/io/project_writer.hpp"

namespace glyma::app {
namespace {

core::LayerId topLayerId(const core::Document& document) {
    const auto& layers = document.layerStack().layers();
    return layers.empty() ? core::Layer::invalidId : layers.back().id();
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent) {
    (void)newDocument(QStringLiteral("Untitled"), 1280, 720);
}

const core::Document& AppController::document() const noexcept {
    return *document_;
}

core::Document& AppController::document() noexcept {
    return *document_;
}

core::ImageBuffer AppController::render() const {
    return document_->render();
}

core::LayerId AppController::selectedLayerId() const noexcept {
    return selectedLayerId_;
}

const QString& AppController::filePath() const noexcept {
    return filePath_;
}

bool AppController::canUndo() const noexcept {
    return history_.canUndo();
}

bool AppController::canRedo() const noexcept {
    return history_.canRedo();
}

bool AppController::isModified() const noexcept {
    return unsavedContent_ || history_.currentRevision() != savedRevision_;
}

const core::BrushSettings& AppController::brushSettings() const noexcept {
    return brushSettings_;
}

bool AppController::setBrushSettings(const core::BrushSettings& settings) {
    if (!settings.isValid()) {
        return false;
    }
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    brushSettings_ = settings;
    return true;
}

core::BrushMode AppController::brushMode() const noexcept {
    return brushMode_;
}

void AppController::setBrushMode(core::BrushMode mode) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    brushMode_ = mode;
}

bool AppController::beginBrushStroke(double documentX, double documentY) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }

    core::Layer* layer = document_->findLayer(selectedLayerId_);
    if (layer == nullptr || !layer->visible()) {
        return false;
    }

    try {
        auto stroke = std::make_unique<core::BrushStroke>(
            layer->buffer(), layer->id(), brushSettings_, brushMode_
        );
        strokeOffsetX_ = static_cast<double>(layer->x());
        strokeOffsetY_ = static_cast<double>(layer->y());
        const bool changed = stroke->start(
            documentX - strokeOffsetX_,
            documentY - strokeOffsetY_
        );
        brushStroke_ = std::move(stroke);
        if (changed) {
            emit documentChanged();
        }
        return true;
    } catch (const std::exception&) {
        brushStroke_.reset();
        return false;
    }
}

bool AppController::updateBrushStroke(double documentX, double documentY) {
    if (!brushStroke_) {
        return false;
    }
    const bool changed = brushStroke_->lineTo(
        documentX - strokeOffsetX_,
        documentY - strokeOffsetY_
    );
    if (changed) {
        emit documentChanged();
    }
    return changed;
}

bool AppController::endBrushStroke() {
    if (!brushStroke_) {
        return false;
    }

    auto command = brushStroke_->finish();
    brushStroke_.reset();
    if (!command) {
        return false;
    }

    const bool recorded = history_.pushExecuted(std::move(command));
    if (recorded) {
        emitDocumentState(false);
    }
    return recorded;
}

bool AppController::cancelBrushStroke() {
    if (!brushStroke_) {
        return false;
    }

    const bool restored = brushStroke_->cancel();
    brushStroke_.reset();
    if (restored) {
        emit documentChanged();
    }
    return restored;
}

bool AppController::isBrushStrokeActive() const noexcept {
    return brushStroke_ != nullptr;
}

const core::Selection& AppController::selection() const noexcept {
    if (selecting_ && pendingSelection_.has_value()) {
        return *pendingSelection_;
    }
    return document_->selection();
}

bool AppController::isSelecting() const noexcept {
    return selecting_;
}

core::Selection::Type AppController::selectionType() const noexcept {
    if (selecting_) {
        return pendingSelectionType_;
    }
    return document_->selection().type();
}

void AppController::beginSelection(
    core::Selection::Type type,
    double documentX,
    double documentY
) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    selecting_ = true;
    pendingSelectionType_ = type;
    dragStartX_ = documentX;
    dragStartY_ = documentY;
    pendingSelection_.emplace();
    pendingSelection_->setDocumentSize(document_->width(), document_->height());
    emit pixelSelectionChanged();
}

void AppController::updateSelection(double documentX, double documentY) {
    if (!selecting_ || !pendingSelection_.has_value()) {
        return;
    }

    if (pendingSelectionType_ == core::Selection::Type::Ellipse) {
        pendingSelection_->setEllipse(
            dragStartX_,
            dragStartY_,
            documentX,
            documentY
        );
    } else {
        pendingSelection_->setRectangle(
            dragStartX_,
            dragStartY_,
            documentX,
            documentY
        );
    }
    emit pixelSelectionChanged();
}

void AppController::endSelection() {
    if (!selecting_) {
        return;
    }

    auto command = std::make_unique<core::SelectCommand>(
        document_->selection(),
        selection()
    );
    if (!history_.execute(std::move(command), *document_)) {
        selecting_ = false;
        pendingSelection_.reset();
        emit pixelSelectionChanged();
        return;
    }

    selecting_ = false;
    pendingSelection_.reset();
    emit pixelSelectionChanged();
    emitDocumentState(false);
}

void AppController::cancelSelection() {
    if (!selecting_) {
        return;
    }
    selecting_ = false;
    pendingSelection_.reset();
    emit pixelSelectionChanged();
}

void AppController::clearSelection() {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    auto command = std::make_unique<core::ClearSelectionCommand>(
        document_->selection()
    );
    if (!history_.execute(std::move(command), *document_)) {
        return;
    }
    emit pixelSelectionChanged();
    emitDocumentState(false);
}

void AppController::invertSelection() {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    auto command = std::make_unique<core::InvertSelectionCommand>(
        document_->selection()
    );
    if (!history_.execute(std::move(command), *document_)) {
        return;
    }
    emit pixelSelectionChanged();
    emitDocumentState(false);
}

const core::FillSettings& AppController::fillSettings() const noexcept {
    return fillSettings_;
}

bool AppController::setFillSettings(const core::FillSettings& settings) {
    if (!settings.isValid()) {
        return false;
    }
    fillSettings_ = settings;
    return true;
}

bool AppController::bucketFill(double documentX, double documentY) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }

    core::Layer* layer = document_->findLayer(selectedLayerId_);
    if (layer == nullptr || !layer->visible()) {
        return false;
    }

    const double bufferX = documentX - static_cast<double>(layer->x());
    const double bufferY = documentY - static_cast<double>(layer->y());
    const int seedX = static_cast<int>(std::lround(bufferX));
    const int seedY = static_cast<int>(std::lround(bufferY));

    const std::vector<core::PixelChange> changes = core::fillRegion(
        layer->buffer(),
        seedX,
        seedY,
        fillSettings_,
        document_->selection()
    );

    if (changes.empty()) {
        return false;
    }

    auto command = std::make_unique<core::FillCommand>(
        layer->id(),
        std::vector<core::PixelChange>(changes)
    );
    if (!history_.execute(std::move(command), *document_)) {
        return false;
    }
    emitDocumentState(false);
    return true;
}

bool AppController::isAdjusting() const noexcept {
    return adjusting_;
}

const core::AdjustmentSettings& AppController::adjustmentSettings() const noexcept {
    return adjustmentSettings_;
}

bool AppController::beginAdjustment() {
    if (adjusting_) {
        discardAdjustment();
    }
    if (brushStroke_) {
        (void)endBrushStroke();
    }

    core::Layer* layer = document_->findLayer(selectedLayerId_);
    if (layer == nullptr || !layer->visible()) {
        return false;
    }

    adjustmentBase_ = std::make_unique<core::ImageBuffer>(layer->buffer());
    adjustmentLayerId_ = layer->id();
    adjustmentSettings_ = {};
    adjusting_ = true;
    return true;
}

bool AppController::updateAdjustment(const core::AdjustmentSettings& settings) {
    if (!adjusting_ || !adjustmentBase_) {
        return false;
    }
    if (!settings.isValid()) {
        return false;
    }

    core::Layer* layer = document_->findLayer(adjustmentLayerId_);
    if (layer == nullptr) {
        discardAdjustment();
        return false;
    }

    const core::SelectionMask mask{
        &document_->selection(),
        static_cast<double>(layer->x()),
        static_cast<double>(layer->y())
    };

    std::unique_ptr<core::ImageBuffer> result;
    try {
        result = core::processImage(*adjustmentBase_, settings, mask);
    } catch (const std::exception&) {
        return false;
    }

    layer->buffer() = std::move(*result);
    adjustmentSettings_ = settings;
    emit documentChanged();
    return true;
}

bool AppController::applyAdjustment() {
    if (!adjusting_ || !adjustmentBase_) {
        return false;
    }

    core::Layer* layer = document_->findLayer(adjustmentLayerId_);
    if (layer == nullptr) {
        discardAdjustment();
        return false;
    }

    std::unique_ptr<core::PixelEditCommand> command;
    try {
        command = core::makePixelEditCommand(
            adjustmentLayerId_, *adjustmentBase_, layer->buffer()
        );
    } catch (const std::exception&) {
        command.reset();
    }

    // The preview has already been applied to the layer, so the command is
    // recorded as executed rather than re-applied.
    adjustmentBase_.reset();
    adjustmentLayerId_ = core::Layer::invalidId;
    adjustmentSettings_ = {};
    adjusting_ = false;

    if (!command) {
        // Nothing changed; the layer already matches the original snapshot.
        emit documentChanged();
        return false;
    }

    if (!history_.pushExecuted(std::move(command))) {
        emit documentChanged();
        return false;
    }
    emitDocumentState(false);
    return true;
}

void AppController::cancelAdjustment() {
    if (!adjusting_) {
        return;
    }
    core::Layer* layer = document_->findLayer(adjustmentLayerId_);
    if (layer != nullptr && adjustmentBase_) {
        layer->buffer() = std::move(*adjustmentBase_);
    }
    discardAdjustment();
    emit documentChanged();
}

void AppController::discardAdjustment() {
    adjustmentBase_.reset();
    adjustmentLayerId_ = core::Layer::invalidId;
    adjustmentSettings_ = {};
    adjusting_ = false;
}

void AppController::installDocument(
    std::unique_ptr<core::Document> document,
    const QString& path,
    bool modifiedOnLoad
) {
    if (brushStroke_) {
        (void)cancelBrushStroke();
    }
    cancelSelection();
    discardAdjustment();
    document_ = std::move(document);
    filePath_ = path;
    history_.clear();
    unsavedContent_ = modifiedOnLoad;
    savedRevision_ = history_.currentRevision();
    ensureSelection();
    emitDocumentState(true);
    emit filePathChanged(filePath_);
}

void AppController::ensureSelection() {
    if (document_->findLayer(selectedLayerId_) != nullptr) {
        return;
    }
    selectedLayerId_ = topLayerId(*document_);
}

void AppController::emitDocumentState(bool selectionMayHaveChanged) {
    emit documentChanged();
    emit layersChanged();
    if (selectionMayHaveChanged) {
        emit selectionChanged(static_cast<quint64>(selectedLayerId_));
    }
    emit historyChanged();
    refreshModifiedState();
}

void AppController::refreshModifiedState() {
    const bool modified = isModified();
    if (modified == modified_) {
        return;
    }
    modified_ = modified;
    emit modifiedChanged(modified_);
}

std::string AppController::nextLayerName() const {
    for (std::size_t index = 1;; ++index) {
        std::string candidate = "Layer " + std::to_string(index);
        const auto& layers = document_->layerStack().layers();
        const bool exists = std::any_of(
            layers.begin(),
            layers.end(),
            [&candidate](const core::Layer& layer) {
                return layer.name() == candidate;
            }
        );
        if (!exists) {
            return candidate;
        }
    }
}

bool AppController::newDocument(
    const QString& name,
    std::uint32_t width,
    std::uint32_t height,
    bool solidBackground,
    core::Pixel backgroundColor
) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();

    const std::uint64_t pixels =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (width == 0 || height == 0 || pixels > core::Document::maxPixelCount) {
        emit errorOccurred(tr("Invalid canvas size. Use at least 1 x 1 and at "
                              "most %1 megapixels.")
                               .arg(core::Document::maxPixelCount / (1024 * 1024)));
        return false;
    }

    const QString documentName = name.trimmed().isEmpty()
        ? QStringLiteral("Untitled")
        : name.trimmed();

    try {
        auto document = std::make_unique<core::Document>(
            documentName.toStdString(), width, height
        );
        if (solidBackground) {
            document->setSolidBackground(backgroundColor);
        }
        selectedLayerId_ = document->addCanvasLayer("Layer 1");
        installDocument(std::move(document), {});
        return true;
    } catch (const std::exception& error) {
        emit errorOccurred(QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::openFile(const QString& path) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        emit errorOccurred(tr("File does not exist: %1").arg(info.filePath()));
        return false;
    }

    const QString suffix = info.suffix().toLower();
    try {
        if (suffix == QStringLiteral("glyma")) {
            auto loaded = std::make_unique<core::Document>(
                core::ProjectReader::load(info.absoluteFilePath().toStdString())
            );
            selectedLayerId_ = topLayerId(*loaded);
            installDocument(std::move(loaded), info.absoluteFilePath());
            return true;
        }

        if (suffix == QStringLiteral("png")) {
            core::ImageBuffer image =
                core::PngReader::read(info.absoluteFilePath().toStdString());
            const std::uint64_t pixels =
                static_cast<std::uint64_t>(image.width()) * image.height();
            if (pixels > core::Document::maxPixelCount) {
                emit errorOccurred(
                    tr("Image exceeds the maximum canvas size of %1 megapixels.")
                        .arg(core::Document::maxPixelCount / (1024 * 1024))
                );
                return false;
            }

            QString name = info.completeBaseName();
            if (name.trimmed().isEmpty()) {
                name = QStringLiteral("Untitled");
            }

            auto document = std::make_unique<core::Document>(
                name.toStdString(), image.width(), image.height()
            );
            core::Layer layer("Background", image.width(), image.height());
            layer.buffer() = std::move(image);
            selectedLayerId_ = document->addLayer(std::move(layer));
            // A PNG import is an unsaved project until it is saved as .glyma.
            installDocument(std::move(document), {}, true);
            return true;
        }

        emit errorOccurred(tr("Unsupported file type: .%1").arg(suffix));
        return false;
    } catch (const std::exception& error) {
        emit errorOccurred(QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::save(const QString& path) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    cancelAdjustment();
    if (path.isEmpty()) {
        emit errorOccurred(tr("No save path was provided."));
        return false;
    }
    try {
        core::ProjectWriter::save(*document_, path.toStdString());
        filePath_ = QFileInfo(path).absoluteFilePath();
        unsavedContent_ = false;
        savedRevision_ = history_.currentRevision();
        emit filePathChanged(filePath_);
        refreshModifiedState();
        return true;
    } catch (const std::exception& error) {
        emit errorOccurred(tr("Could not save project: %1")
                               .arg(QString::fromUtf8(error.what())));
        return false;
    }
}

bool AppController::exportPng(const QString& path) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    cancelAdjustment();
    if (path.isEmpty()) {
        emit errorOccurred(tr("No export path was provided."));
        return false;
    }
    try {
        core::DocumentIO::exportPng(*document_, path.toStdString());
        return true;
    } catch (const std::exception& error) {
        emit errorOccurred(tr("Could not export PNG: %1")
                               .arg(QString::fromUtf8(error.what())));
        return false;
    }
}

bool AppController::undo() {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    cancelAdjustment();
    if (!history_.undo(*document_)) {
        return false;
    }
    ensureSelection();
    emitDocumentState(true);
    return true;
}

bool AppController::redo() {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    cancelAdjustment();
    if (!history_.redo(*document_)) {
        return false;
    }
    ensureSelection();
    emitDocumentState(true);
    return true;
}

bool AppController::commitTransform(core::CommandPtr command) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    if (!command) {
        return false;
    }
    if (!history_.execute(std::move(command), *document_)) {
        return false;
    }
    ensureSelection();
    emit pixelSelectionChanged();
    emitDocumentState(true);
    return true;
}

bool AppController::crop(const core::CropRect& rect) {
    const core::CropRect clipped = core::clipCropRect(
        rect, document_->width(), document_->height()
    );
    if (clipped.isEmpty()) {
        emit errorOccurred(tr("Select a non-empty crop area inside the canvas."));
        return false;
    }
    if (clipped.x == 0 && clipped.y == 0 &&
        clipped.width == document_->width() &&
        clipped.height == document_->height()) {
        return false;
    }
    return commitTransform(
        std::make_unique<core::CropCommand>(clipped)
    );
}

bool AppController::resizeCanvas(
    std::uint32_t width,
    std::uint32_t height,
    core::Anchor anchor
) {
    if (!core::isValidCanvasSize(width, height)) {
        emit errorOccurred(tr("Invalid canvas size."));
        return false;
    }
    if (width == document_->width() && height == document_->height()) {
        return false;
    }
    return commitTransform(
        std::make_unique<core::CanvasResizeCommand>(width, height, anchor)
    );
}

bool AppController::resizeImage(
    std::uint32_t width,
    std::uint32_t height
) {
    if (!core::isValidCanvasSize(width, height)) {
        emit errorOccurred(tr("Invalid image size."));
        return false;
    }
    if (width == document_->width() && height == document_->height()) {
        return false;
    }
    return commitTransform(
        std::make_unique<core::ImageResizeCommand>(width, height)
    );
}

bool AppController::rotateCanvas(core::DocumentRotation rotation) {
    return commitTransform(
        std::make_unique<core::RotateDocumentCommand>(rotation)
    );
}

bool AppController::rotateCanvas(double degrees) {
    if (!std::isfinite(degrees)) {
        emit errorOccurred(tr("Rotation angle must be a finite number."));
        return false;
    }
    const double normalized = std::fmod(degrees, 360.0);
    if (std::abs(normalized) < 1.0e-9 ||
        std::abs(std::abs(normalized) - 360.0) < 1.0e-9) {
        return false;
    }
    return commitTransform(
        std::make_unique<core::ArbitraryRotateDocumentCommand>(degrees)
    );
}

bool AppController::rotateCanvas90(bool clockwise) {
    return rotateCanvas(
        clockwise ? core::DocumentRotation::Clockwise90
                  : core::DocumentRotation::CounterClockwise90
    );
}

bool AppController::rotateCanvas180() {
    return rotateCanvas(core::DocumentRotation::Rotate180);
}

bool AppController::flipCanvas(bool horizontal) {
    return commitTransform(
        std::make_unique<core::FlipDocumentCommand>(horizontal)
    );
}

bool AppController::transformLayer(
    core::LayerId id,
    core::LayerTransform transform
) {
    if (document_->findLayer(id) == nullptr) {
        emit errorOccurred(tr("Select a layer to transform."));
        return false;
    }
    return commitTransform(
        std::make_unique<core::TransformLayerCommand>(id, transform)
    );
}

bool AppController::rotateLayer(core::LayerId id, double degrees) {
    if (document_->findLayer(id) == nullptr) {
        emit errorOccurred(tr("Select a layer to rotate."));
        return false;
    }
    if (!std::isfinite(degrees)) {
        emit errorOccurred(tr("Rotation angle must be a finite number."));
        return false;
    }
    const double normalized = std::fmod(degrees, 360.0);
    if (std::abs(normalized) < 1.0e-9) {
        return false;
    }
    return commitTransform(
        std::make_unique<core::ArbitraryRotateLayerCommand>(id, degrees)
    );
}

bool AppController::scaleLayer(
    core::LayerId id,
    std::uint32_t width,
    std::uint32_t height
) {
    if (document_->findLayer(id) == nullptr) {
        emit errorOccurred(tr("Select a layer to scale."));
        return false;
    }
    if (!core::isValidCanvasSize(width, height)) {
        emit errorOccurred(tr("Invalid layer size."));
        return false;
    }
    return commitTransform(
        std::make_unique<core::ScaleLayerCommand>(id, width, height)
    );
}

bool AppController::translateLayer(
    core::LayerId id,
    std::int32_t deltaX,
    std::int32_t deltaY
) {
    if (document_->findLayer(id) == nullptr) {
        emit errorOccurred(tr("Select a layer to move."));
        return false;
    }
    if (deltaX == 0 && deltaY == 0) {
        return false;
    }
    return commitTransform(
        std::make_unique<core::TranslateLayerCommand>(id, deltaX, deltaY)
    );
}

void AppController::selectLayer(core::LayerId id) {
    if (document_->findLayer(id) == nullptr || selectedLayerId_ == id) {
        return;
    }
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    selectedLayerId_ = id;
    emit selectionChanged(static_cast<quint64>(selectedLayerId_));
}

void AppController::addLayer() {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    auto command = std::make_unique<core::AddLayerCommand>(
        nextLayerName(), document_->width(), document_->height()
    );
    if (!history_.execute(std::move(command), *document_)) {
        return;
    }
    selectedLayerId_ = topLayerId(*document_);
    emitDocumentState(true);
}

void AppController::removeLayer(core::LayerId id) {
    if (document_->findLayer(id) == nullptr) {
        return;
    }
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    auto command = std::make_unique<core::RemoveLayerCommand>(id);
    if (!history_.execute(std::move(command), *document_)) {
        return;
    }
    ensureSelection();
    emitDocumentState(true);
}

void AppController::renameLayer(core::LayerId id, const QString& name) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    core::Layer* layer = document_->findLayer(id);
    if (layer == nullptr) {
        return;
    }

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        emit errorOccurred(tr("Layer name cannot be empty."));
        return;
    }
    if (trimmed.toStdString() == layer->name()) {
        return;
    }

    auto command = std::make_unique<core::ChangeLayerPropertiesCommand>(
        id,
        trimmed.toStdString(),
        layer->visible(),
        layer->opacity(),
        layer->x(),
        layer->y()
    );
    if (!history_.execute(std::move(command), *document_)) {
        return;
    }
    emitDocumentState(false);
}

void AppController::moveLayerUp(core::LayerId id) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    const auto& layers = document_->layerStack().layers();
    for (std::size_t index = 0; index < layers.size(); ++index) {
        if (layers[index].id() != id) {
            continue;
        }
        if (index + 1 >= layers.size()) {
            return;
        }
        auto command = std::make_unique<core::MoveLayerCommand>(id, index + 1);
        if (history_.execute(std::move(command), *document_)) {
            emitDocumentState(false);
        }
        return;
    }
}

void AppController::moveLayerDown(core::LayerId id) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    const auto& layers = document_->layerStack().layers();
    for (std::size_t index = 0; index < layers.size(); ++index) {
        if (layers[index].id() != id) {
            continue;
        }
        if (index == 0) {
            return;
        }
        auto command = std::make_unique<core::MoveLayerCommand>(id, index - 1);
        if (history_.execute(std::move(command), *document_)) {
            emitDocumentState(false);
        }
        return;
    }
}

void AppController::setLayerVisibility(core::LayerId id, bool visible) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    core::Layer* layer = document_->findLayer(id);
    if (layer == nullptr || layer->visible() == visible) {
        return;
    }

    auto command = std::make_unique<core::ChangeLayerPropertiesCommand>(
        id,
        layer->name(),
        visible,
        layer->opacity(),
        layer->x(),
        layer->y()
    );
    if (!history_.execute(std::move(command), *document_)) {
        return;
    }
    emitDocumentState(false);
}

void AppController::setLayerOpacity(core::LayerId id, float opacity) {
    if (brushStroke_) {
        (void)endBrushStroke();
    }
    cancelSelection();
    if (!std::isfinite(opacity)) {
        return;
    }
    opacity = std::clamp(opacity, 0.0F, 1.0F);

    core::Layer* layer = document_->findLayer(id);
    if (layer == nullptr || std::abs(layer->opacity() - opacity) < 0.0005F) {
        return;
    }

    auto command = std::make_unique<core::ChangeLayerPropertiesCommand>(
        id,
        layer->name(),
        layer->visible(),
        opacity,
        layer->x(),
        layer->y()
    );
    if (!history_.execute(std::move(command), *document_)) {
        return;
    }
    emitDocumentState(false);
}

} // namespace glyma::app
