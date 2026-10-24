#include "glyma/app/app_controller.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>

#include <QFileInfo>

#include "glyma/core/commands.hpp"
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

void AppController::installDocument(
    std::unique_ptr<core::Document> document,
    const QString& path,
    bool modifiedOnLoad
) {
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
    if (!history_.undo(*document_)) {
        return false;
    }
    ensureSelection();
    emitDocumentState(true);
    return true;
}

bool AppController::redo() {
    if (!history_.redo(*document_)) {
        return false;
    }
    ensureSelection();
    emitDocumentState(true);
    return true;
}

void AppController::selectLayer(core::LayerId id) {
    if (document_->findLayer(id) == nullptr || selectedLayerId_ == id) {
        return;
    }
    selectedLayerId_ = id;
    emit selectionChanged(static_cast<quint64>(selectedLayerId_));
}

void AppController::addLayer() {
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
    auto command = std::make_unique<core::RemoveLayerCommand>(id);
    if (!history_.execute(std::move(command), *document_)) {
        return;
    }
    ensureSelection();
    emitDocumentState(true);
}

void AppController::renameLayer(core::LayerId id, const QString& name) {
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
