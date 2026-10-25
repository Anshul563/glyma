#include "glyma/app/text_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QString>
#include <QStringList>

#include "glyma/app/qt_image.hpp"

namespace glyma::app {
namespace {

QFont buildFont(const core::TextStyle& style) {
    QFont font(QString::fromStdString(style.fontFamily));
    const int pixelSize = std::max(
        1, static_cast<int>(std::lround(static_cast<double>(style.fontSize)))
    );
    font.setPixelSize(pixelSize);
    font.setWeight(
        static_cast<QFont::Weight>(std::clamp(style.fontWeight, 1, 1000))
    );
    font.setItalic(style.italic);
    if (std::abs(style.letterSpacing) > 0.0F) {
        font.setLetterSpacing(
            QFont::AbsoluteSpacing, static_cast<double>(style.letterSpacing)
        );
    }
    return font;
}

// Greedy word wrap of a single logical line to `maxWidth` device pixels.
std::vector<QString> wrapLine(
    const QString& line,
    const QFontMetricsF& metrics,
    double maxWidth
) {
    std::vector<QString> wrapped;
    if (maxWidth <= 0.0) {
        wrapped.push_back(line);
        return wrapped;
    }

    const QStringList words = line.split(QLatin1Char(' '), Qt::KeepEmptyParts);
    QString current;
    for (const QString& word : words) {
        const QString candidate =
            current.isEmpty() ? word : current + QLatin1Char(' ') + word;
        if (current.isEmpty() || metrics.horizontalAdvance(candidate) <= maxWidth) {
            current = candidate;
        } else {
            wrapped.push_back(current);
            current = word;
        }
    }
    wrapped.push_back(current);
    return wrapped;
}

} // namespace

TextRenderResult TextRenderer::render(const core::TextLayerData& data) {
    data.validate();

    const QFont font = buildFont(data.style);
    const QFontMetricsF metrics(font);
    const double lineHeight = std::max(
        1.0,
        metrics.height() * static_cast<double>(data.style.lineSpacing)
    );
    const double boxMaxWidth = data.autoSize
        ? 0.0
        : static_cast<double>(data.boxWidth);

    const QString source = QString::fromStdString(data.text);
    const QStringList paragraphs =
        source.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    std::vector<QString> lines;
    for (const QString& paragraph : paragraphs) {
        if (data.style.wordWrap && !data.autoSize && data.boxWidth > 0) {
            const auto wrapped = wrapLine(paragraph, metrics, boxMaxWidth);
            lines.insert(lines.end(), wrapped.begin(), wrapped.end());
        } else {
            lines.push_back(paragraph);
        }
    }
    if (lines.empty()) {
        lines.push_back(QString());
    }

    double contentWidth = 0.0;
    for (const QString& line : lines) {
        contentWidth = std::max(contentWidth, metrics.horizontalAdvance(line));
    }
    const double contentHeight =
        lineHeight * static_cast<double>(lines.size());

    std::uint32_t boxWidth = 0;
    std::uint32_t boxHeight = 0;
    if (data.autoSize) {
        boxWidth = static_cast<std::uint32_t>(
            std::max(1.0, std::ceil(contentWidth))
        );
        boxHeight = static_cast<std::uint32_t>(
            std::max(1.0, std::ceil(contentHeight))
        );
    } else {
        boxWidth = data.boxWidth;
        boxHeight = data.boxHeight;
    }

    const std::uint64_t pixels =
        static_cast<std::uint64_t>(boxWidth) *
        static_cast<std::uint64_t>(boxHeight);
    if (boxWidth > core::TextLayerData::maxBoxDimension ||
        boxHeight > core::TextLayerData::maxBoxDimension ||
        pixels > core::TextLayerData::maxPixelCount) {
        throw std::runtime_error("Text render exceeds the supported size.");
    }

    QImage image(
        static_cast<int>(boxWidth),
        static_cast<int>(boxHeight),
        QImage::Format_RGBA8888
    );
    if (image.isNull()) {
        throw std::runtime_error("Failed to allocate the text render image.");
    }
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(font);
    painter.setPen(QColor(
        data.style.color.r,
        data.style.color.g,
        data.style.color.b,
        data.style.color.a
    ));

    const double boxWidthD = static_cast<double>(boxWidth);
    const double boxHeightD = static_cast<double>(boxHeight);

    double startY = 0.0;
    switch (data.style.verticalAlign) {
    case core::TextVerticalAlign::Top:
        startY = 0.0;
        break;
    case core::TextVerticalAlign::Middle:
        startY = (boxHeightD - contentHeight) / 2.0;
        break;
    case core::TextVerticalAlign::Bottom:
        startY = boxHeightD - contentHeight;
        break;
    }

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const QString& line = lines[index];
        const double advance = metrics.horizontalAdvance(line);
        double x = 0.0;
        switch (data.style.horizontalAlign) {
        case core::TextHorizontalAlign::Left:
            x = 0.0;
            break;
        case core::TextHorizontalAlign::Center:
            x = (boxWidthD - advance) / 2.0;
            break;
        case core::TextHorizontalAlign::Right:
            x = boxWidthD - advance;
            break;
        }
        const double baseline =
            startY + static_cast<double>(index) * lineHeight + metrics.ascent();
        painter.drawText(QPointF(x, baseline), line);
    }
    painter.end();

    return TextRenderResult{toImageBuffer(image), boxWidth, boxHeight};
}

bool TextRenderer::renderInto(core::Layer& layer) {
    if (!layer.isText()) {
        return false;
    }
    core::TextLayerData data = layer.text();
    try {
        TextRenderResult result = render(data);
        layer.buffer() = std::move(result.image);
        if (data.autoSize) {
            data.boxWidth = result.boxWidth;
            data.boxHeight = result.boxHeight;
            layer.setTextData(std::move(data));
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::size_t TextRenderer::refreshDocument(core::Document& document) {
    std::vector<core::LayerId> textIds;
    for (const core::Layer& layer : document.layerStack().layers()) {
        if (layer.isText()) {
            textIds.push_back(layer.id());
        }
    }

    std::size_t refreshed = 0;
    for (const core::LayerId id : textIds) {
        core::Layer* layer = document.findLayer(id);
        if (layer != nullptr && renderInto(*layer)) {
            ++refreshed;
        }
    }
    return refreshed;
}

} // namespace glyma::app
