#include "glyma/app/shape_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <QBrush>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

#include "glyma/app/qt_image.hpp"

namespace glyma::app {
namespace {

constexpr std::uint64_t kMaxRenderPixels = 64ULL * 1024ULL * 1024ULL;

QColor toColor(core::Pixel pixel) {
    return QColor(pixel.r, pixel.g, pixel.b, pixel.a);
}

Qt::PenStyle toPenStyle(core::StrokeStyle style) {
    switch (style) {
    case core::StrokeStyle::Solid:
        return Qt::SolidLine;
    case core::StrokeStyle::Dashed:
        return Qt::DashLine;
    case core::StrokeStyle::Dotted:
        return Qt::DotLine;
    }
    return Qt::SolidLine;
}

QPolygonF arrowHead(
    QPointF tip,
    QPointF from,
    double length,
    double width
) {
    QPolygonF head;
    const double dx = from.x() - tip.x();
    const double dy = from.y() - tip.y();
    const double magnitude = std::hypot(dx, dy);
    if (magnitude <= 0.0 || length <= 0.0 || width <= 0.0) {
        return head;
    }
    const double ux = dx / magnitude;
    const double uy = dy / magnitude;
    const QPointF baseCentre(tip.x() + ux * length, tip.y() + uy * length);
    const double px = -uy * (width * 0.5);
    const double py = ux * (width * 0.5);
    head << tip
         << QPointF(baseCentre.x() + px, baseCentre.y() + py)
         << QPointF(baseCentre.x() - px, baseCentre.y() - py);
    return head;
}

QPainterPath buildPath(const core::ShapeLayerData& shape) {
    QPainterPath path;
    switch (shape.type) {
    case core::ShapeType::Rectangle:
        path.addRect(QRectF(shape.x, shape.y, shape.width, shape.height));
        break;
    case core::ShapeType::RoundedRectangle: {
        const double radius = std::min(
            static_cast<double>(shape.cornerRadius),
            std::min(shape.width, shape.height) * 0.5
        );
        path.addRoundedRect(
            QRectF(shape.x, shape.y, shape.width, shape.height),
            radius,
            radius
        );
        break;
    }
    case core::ShapeType::Ellipse:
        path.addEllipse(QRectF(shape.x, shape.y, shape.width, shape.height));
        break;
    case core::ShapeType::Line:
    case core::ShapeType::Arrow:
        path.moveTo(shape.startX, shape.startY);
        path.lineTo(shape.endX, shape.endY);
        break;
    case core::ShapeType::Polygon: {
        if (!shape.points.empty()) {
            path.moveTo(shape.points.front().x, shape.points.front().y);
            for (std::size_t i = 1; i < shape.points.size(); ++i) {
                path.lineTo(shape.points[i].x, shape.points[i].y);
            }
            path.closeSubpath();
        }
        break;
    }
    }
    return path;
}

bool isClosedShape(core::ShapeType type) {
    return type != core::ShapeType::Line && type != core::ShapeType::Arrow;
}

} // namespace

QPainterPath ShapeRenderer::painterPath(const core::ShapeLayerData& shape) {
    return buildPath(shape);
}

void ShapeRenderer::paintShape(QPainter& painter, const core::ShapeLayerData& shape) {
    const QPainterPath path = buildPath(shape);

    if (shape.fill.enabled && isClosedShape(shape.type)) {
        painter.setPen(Qt::NoPen);
        painter.fillPath(path, QBrush(toColor(shape.fill.color)));
    }

    if (shape.stroke.enabled && shape.stroke.width > 0.0F) {
        QPen pen(toColor(shape.stroke.color));
        pen.setWidthF(static_cast<double>(shape.stroke.width));
        pen.setStyle(toPenStyle(shape.stroke.style));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    if (shape.type == core::ShapeType::Arrow) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(toColor(shape.stroke.color)));
        const QPointF start(shape.startX, shape.startY);
        const QPointF end(shape.endX, shape.endY);
        if (shape.arrow.end) {
            painter.drawPolygon(arrowHead(
                end, start, shape.arrow.length, shape.arrow.width
            ));
        }
        if (shape.arrow.start) {
            painter.drawPolygon(arrowHead(
                start, end, shape.arrow.length, shape.arrow.width
            ));
        }
    }
}

ShapeRenderResult ShapeRenderer::render(const core::ShapeLayerData& shape) {
    core::ShapeLayerData data = shape;
    core::sanitizeShape(data);
    data.validate();

    const core::ShapeBounds bounds = core::shapeVisualBounds(data);
    const double originX = std::floor(bounds.x);
    const double originY = std::floor(bounds.y);
    const std::int64_t width = std::max<std::int64_t>(
        1,
        static_cast<std::int64_t>(std::ceil(bounds.right())) -
            static_cast<std::int64_t>(originX)
    );
    const std::int64_t height = std::max<std::int64_t>(
        1,
        static_cast<std::int64_t>(std::ceil(bounds.bottom())) -
            static_cast<std::int64_t>(originY)
    );

    const std::uint64_t pixels =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (width > core::ShapeLayerData::maxDimension ||
        height > core::ShapeLayerData::maxDimension ||
        pixels > kMaxRenderPixels) {
        throw std::runtime_error("Shape render exceeds the supported size.");
    }

    QImage image(
        static_cast<int>(width),
        static_cast<int>(height),
        QImage::Format_RGBA8888
    );
    if (image.isNull()) {
        throw std::runtime_error("Failed to allocate the shape render image.");
    }
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const core::ShapePoint centre = core::shapeCentre(data);
    painter.translate(-originX, -originY);
    if (data.rotation != 0.0) {
        painter.translate(centre.x, centre.y);
        painter.rotate(data.rotation);
        painter.translate(-centre.x, -centre.y);
    }

    paintShape(painter, data);
    painter.end();

    return ShapeRenderResult{
        toImageBuffer(image),
        bounds,
    };
}

bool ShapeRenderer::renderInto(core::Layer& layer) {
    if (!layer.isVector()) {
        return false;
    }
    try {
        ShapeRenderResult result = render(layer.shape());
        layer.buffer() = std::move(result.image);
        layer.setPosition(
            static_cast<std::int32_t>(std::floor(result.bounds.x)),
            static_cast<std::int32_t>(std::floor(result.bounds.y))
        );
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::size_t ShapeRenderer::refreshDocument(core::Document& document) {
    std::vector<core::LayerId> shapeIds;
    for (const core::Layer& layer : document.layerStack().layers()) {
        if (layer.isVector()) {
            shapeIds.push_back(layer.id());
        }
    }

    std::size_t refreshed = 0;
    for (const core::LayerId id : shapeIds) {
        core::Layer* layer = document.findLayer(id);
        if (layer != nullptr && renderInto(*layer)) {
            ++refreshed;
        }
    }
    return refreshed;
}

} // namespace glyma::app
