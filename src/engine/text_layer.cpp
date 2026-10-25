#include "glyma/core/text_layer.hpp"

#include <cmath>
#include <stdexcept>

namespace glyma::core {

bool isValidTextStyle(const TextStyle& style) noexcept {
    if (style.fontFamily.empty()) {
        return false;
    }
    if (!std::isfinite(style.fontSize) ||
        style.fontSize <= 0.0F ||
        style.fontSize > 100000.0F) {
        return false;
    }
    if (style.fontWeight < 1 || style.fontWeight > 1000) {
        return false;
    }
    if (!std::isfinite(style.lineSpacing) ||
        style.lineSpacing <= 0.0F ||
        style.lineSpacing > 100.0F) {
        return false;
    }
    if (!std::isfinite(style.letterSpacing) ||
        std::abs(style.letterSpacing) > 10000.0F) {
        return false;
    }
    return true;
}

bool isValidTextBox(
    bool autoSize,
    std::uint32_t boxWidth,
    std::uint32_t boxHeight
) noexcept {
    if (boxWidth > TextLayerData::maxBoxDimension ||
        boxHeight > TextLayerData::maxBoxDimension) {
        return false;
    }
    if (!autoSize && (boxWidth == 0 || boxHeight == 0)) {
        return false;
    }
    const std::uint64_t pixels =
        static_cast<std::uint64_t>(boxWidth) *
        static_cast<std::uint64_t>(boxHeight);
    return pixels <= TextLayerData::maxPixelCount;
}

void TextLayerData::validate() const {
    if (!isValidTextStyle(style)) {
        throw std::invalid_argument("Text style is outside the supported range.");
    }
    if (!isValidTextBox(autoSize, boxWidth, boxHeight)) {
        throw std::invalid_argument("Text box dimensions are invalid.");
    }
}

bool TextLayerData::isValid() const noexcept {
    return isValidTextStyle(style) &&
           isValidTextBox(autoSize, boxWidth, boxHeight);
}

const char* textHorizontalAlignName(TextHorizontalAlign align) noexcept {
    switch (align) {
    case TextHorizontalAlign::Left:
        return "left";
    case TextHorizontalAlign::Center:
        return "center";
    case TextHorizontalAlign::Right:
        return "right";
    }
    return "left";
}

const char* textVerticalAlignName(TextVerticalAlign align) noexcept {
    switch (align) {
    case TextVerticalAlign::Top:
        return "top";
    case TextVerticalAlign::Middle:
        return "middle";
    case TextVerticalAlign::Bottom:
        return "bottom";
    }
    return "top";
}

bool parseTextHorizontalAlign(
    const std::string& value,
    TextHorizontalAlign& out
) noexcept {
    if (value == "left") {
        out = TextHorizontalAlign::Left;
        return true;
    }
    if (value == "center") {
        out = TextHorizontalAlign::Center;
        return true;
    }
    if (value == "right") {
        out = TextHorizontalAlign::Right;
        return true;
    }
    return false;
}

bool parseTextVerticalAlign(
    const std::string& value,
    TextVerticalAlign& out
) noexcept {
    if (value == "top") {
        out = TextVerticalAlign::Top;
        return true;
    }
    if (value == "middle") {
        out = TextVerticalAlign::Middle;
        return true;
    }
    if (value == "bottom") {
        out = TextVerticalAlign::Bottom;
        return true;
    }
    return false;
}

} // namespace glyma::core
