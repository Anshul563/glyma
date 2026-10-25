#include "glyma/core/selection.hpp"

#include <algorithm>
#include <cmath>

namespace glyma::core {

void Selection::setDocumentSize(
    std::uint32_t width,
    std::uint32_t height
) noexcept {
    docWidth_ = width;
    docHeight_ = height;
    clipToDocument(
        left_, top_, right_, bottom_, width, height
    );
    if (type_ != Type::None && !inverted_ &&
        (right_ <= left_ || bottom_ <= top_)) {
        clear();
    }
}

std::uint32_t Selection::documentWidth() const noexcept {
    return docWidth_;
}

std::uint32_t Selection::documentHeight() const noexcept {
    return docHeight_;
}

bool Selection::isEmpty() const noexcept {
    return type_ == Type::None && !inverted_;
}

Selection::Type Selection::type() const noexcept {
    return type_;
}

bool Selection::inverted() const noexcept {
    return inverted_;
}

void Selection::clear() noexcept {
    type_ = Type::None;
    inverted_ = false;
    left_ = top_ = right_ = bottom_ = 0.0;
}

void Selection::setRectangle(
    double left,
    double top,
    double right,
    double bottom
) {
    if (!std::isfinite(left) || !std::isfinite(top) ||
        !std::isfinite(right) || !std::isfinite(bottom)) {
        clear();
        return;
    }

    // Accept drags in any direction by normalizing the endpoints.
    if (right < left) {
        std::swap(left, right);
    }
    if (bottom < top) {
        std::swap(top, bottom);
    }

    // A zero-area selection selects nothing.
    if (right <= left || bottom <= top) {
        clear();
        return;
    }

    clipToDocument(left, top, right, bottom, docWidth_, docHeight_);

    if (right <= left || bottom <= top) {
        clear();
        return;
    }

    type_ = Type::Rectangle;
    inverted_ = false;
    left_ = left;
    top_ = top;
    right_ = right;
    bottom_ = bottom;
}

void Selection::setEllipse(
    double left,
    double top,
    double right,
    double bottom
) {
    if (!std::isfinite(left) || !std::isfinite(top) ||
        !std::isfinite(right) || !std::isfinite(bottom)) {
        clear();
        return;
    }

    if (right < left) {
        std::swap(left, right);
    }
    if (bottom < top) {
        std::swap(top, bottom);
    }

    if (right <= left || bottom <= top) {
        clear();
        return;
    }

    clipToDocument(left, top, right, bottom, docWidth_, docHeight_);

    if (right <= left || bottom <= top) {
        clear();
        return;
    }

    type_ = Type::Ellipse;
    inverted_ = false;
    left_ = left;
    top_ = top;
    right_ = right;
    bottom_ = bottom;
}

void Selection::invert() noexcept {
    inverted_ = !inverted_;
}

void Selection::translate(double dx, double dy) noexcept {
    if (type_ == Type::None) {
        return;
    }
    if (!std::isfinite(dx) || !std::isfinite(dy)) {
        return;
    }
    left_ += dx;
    right_ += dx;
    top_ += dy;
    bottom_ += dy;
    clipToDocument(left_, top_, right_, bottom_, docWidth_, docHeight_);
}

void Selection::scale(double scaleX, double scaleY) noexcept {
    if (!std::isfinite(scaleX) || !std::isfinite(scaleY) ||
        scaleX <= 0.0 || scaleY <= 0.0) {
        return;
    }

    const auto scaledWidth = static_cast<std::uint32_t>(std::max(
        1.0,
        std::round(static_cast<double>(docWidth_) * scaleX)
    ));
    const auto scaledHeight = static_cast<std::uint32_t>(std::max(
        1.0,
        std::round(static_cast<double>(docHeight_) * scaleY)
    ));

    if (type_ != Type::None) {
        left_ *= scaleX;
        right_ *= scaleX;
        top_ *= scaleY;
        bottom_ *= scaleY;
    }
    docWidth_ = scaledWidth;
    docHeight_ = scaledHeight;
    clipToDocument(left_, top_, right_, bottom_, docWidth_, docHeight_);
}

void Selection::rotate90Clockwise() noexcept {
    if (type_ != Type::None) {
        const double oldLeft = left_;
        const double oldTop = top_;
        const double oldRight = right_;
        const double oldBottom = bottom_;
        left_ = static_cast<double>(docHeight_) - oldBottom;
        right_ = static_cast<double>(docHeight_) - oldTop;
        top_ = oldLeft;
        bottom_ = oldRight;
    }
    std::swap(docWidth_, docHeight_);
    clipToDocument(left_, top_, right_, bottom_, docWidth_, docHeight_);
}

void Selection::rotate90CounterClockwise() noexcept {
    if (type_ != Type::None) {
        const double oldLeft = left_;
        const double oldTop = top_;
        const double oldRight = right_;
        const double oldBottom = bottom_;
        const double oldWidth = static_cast<double>(docWidth_);
        left_ = oldTop;
        right_ = oldBottom;
        top_ = oldWidth - oldRight;
        bottom_ = oldWidth - oldLeft;
    }
    std::swap(docWidth_, docHeight_);
    clipToDocument(left_, top_, right_, bottom_, docWidth_, docHeight_);
}

void Selection::rotate180() noexcept {
    if (type_ != Type::None) {
        const double oldLeft = left_;
        const double oldTop = top_;
        const double oldRight = right_;
        const double oldBottom = bottom_;
        left_ = static_cast<double>(docWidth_) - oldRight;
        right_ = static_cast<double>(docWidth_) - oldLeft;
        top_ = static_cast<double>(docHeight_) - oldBottom;
        bottom_ = static_cast<double>(docHeight_) - oldTop;
    }
    clipToDocument(left_, top_, right_, bottom_, docWidth_, docHeight_);
}

void Selection::flipHorizontal() noexcept {
    if (type_ == Type::None) {
        return;
    }
    const double oldLeft = left_;
    const double oldRight = right_;
    left_ = static_cast<double>(docWidth_) - oldRight;
    right_ = static_cast<double>(docWidth_) - oldLeft;
    clipToDocument(left_, top_, right_, bottom_, docWidth_, docHeight_);
}

void Selection::flipVertical() noexcept {
    if (type_ == Type::None) {
        return;
    }
    const double oldTop = top_;
    const double oldBottom = bottom_;
    top_ = static_cast<double>(docHeight_) - oldBottom;
    bottom_ = static_cast<double>(docHeight_) - oldTop;
    clipToDocument(left_, top_, right_, bottom_, docWidth_, docHeight_);
}

bool Selection::contains(double x, double y) const noexcept {
    const bool inside = containsGeometry(x, y);
    return inverted_ ? !inside : inside;
}

bool Selection::pixelSelected(
    std::uint32_t x,
    std::uint32_t y
) const noexcept {
    if (x >= docWidth_ || y >= docHeight_) {
        return false;
    }
    return contains(
        static_cast<double>(x) + 0.5,
        static_cast<double>(y) + 0.5
    );
}

bool Selection::bounds(
    double& left,
    double& top,
    double& right,
    double& bottom
) const noexcept {
    // An empty, non-inverted selection selects nothing.
    if (type_ == Type::None && !inverted_) {
        return false;
    }

    if (inverted_) {
        if (docWidth_ == 0 || docHeight_ == 0) {
            return false;
        }
        left = 0.0;
        top = 0.0;
        right = static_cast<double>(docWidth_);
        bottom = static_cast<double>(docHeight_);
        return true;
    }

    left = left_;
    top = top_;
    right = right_;
    bottom = bottom_;
    return true;
}

bool Selection::containsGeometry(double x, double y) const noexcept {
    switch (type_) {
    case Type::None:
        return false;
    case Type::Rectangle:
        return x >= left_ && x <= right_ && y >= top_ && y <= bottom_;
    case Type::Ellipse: {
        const double radiusX = (right_ - left_) * 0.5;
        const double radiusY = (bottom_ - top_) * 0.5;
        if (radiusX <= 0.0 || radiusY <= 0.0) {
            return false;
        }
        const double centreX = (left_ + right_) * 0.5;
        const double centreY = (top_ + bottom_) * 0.5;
        const double dx = (x - centreX) / radiusX;
        const double dy = (y - centreY) / radiusY;
        return dx * dx + dy * dy <= 1.0;
    }
    }
    return false;
}

void Selection::clipToDocument(
    double& left,
    double& top,
    double& right,
    double& bottom,
    std::uint32_t width,
    std::uint32_t height
) noexcept {
    left = std::clamp(left, 0.0, static_cast<double>(width));
    top = std::clamp(top, 0.0, static_cast<double>(height));
    right = std::clamp(right, 0.0, static_cast<double>(width));
    bottom = std::clamp(bottom, 0.0, static_cast<double>(height));
}

} // namespace glyma::core
