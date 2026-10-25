#include "glyma/core/shape_layer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace glyma::core {
namespace {

constexpr double kPi = 3.14159265358979323846;

double degreesToRadians(double degrees) noexcept {
    return degrees * kPi / 180.0;
}

double normalizeAngle(double degrees) noexcept {
    if (!std::isfinite(degrees)) {
        return 0.0;
    }
    double result = std::fmod(degrees, 360.0);
    if (result < 0.0) {
        result += 360.0;
    }
    return result;
}

ShapePoint rotatePoint(
    ShapePoint point,
    double pivotX,
    double pivotY,
    double degrees
) noexcept {
    const double radians = degreesToRadians(degrees);
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    const double dx = point.x - pivotX;
    const double dy = point.y - pivotY;
    return ShapePoint{
        pivotX + dx * cosine - dy * sine,
        pivotY + dx * sine + dy * cosine,
    };
}

double clampCoordinate(double value) noexcept {
    return std::clamp(value, -ShapeLayerData::maxCoordinate, ShapeLayerData::maxCoordinate);
}

double clampDimension(double value) noexcept {
    return std::clamp(value, 0.0, ShapeLayerData::maxDimension);
}

double pointToSegmentDistance(
    ShapePoint point,
    ShapePoint a,
    ShapePoint b
) noexcept {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0) {
        return std::hypot(point.x - a.x, point.y - a.y);
    }
    double t = ((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared;
    t = std::clamp(t, 0.0, 1.0);
    const double projX = a.x + t * dx;
    const double projY = a.y + t * dy;
    return std::hypot(point.x - projX, point.y - projY);
}

bool pointInPolygon(ShapePoint point, const std::vector<ShapePoint>& points) noexcept {
    if (points.size() < 3) {
        return false;
    }
    bool inside = false;
    for (std::size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
        const ShapePoint& a = points[i];
        const ShapePoint& b = points[j];
        const bool intersects =
            ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

void ensureFinite(double& value) noexcept {
    if (!std::isfinite(value)) {
        value = 0.0;
    }
}

void ensureFinite(float& value) noexcept {
    if (!std::isfinite(value)) {
        value = 0.0F;
    }
}

} // namespace

const char* shapeTypeName(ShapeType type) noexcept {
    switch (type) {
    case ShapeType::Rectangle:
        return "rectangle";
    case ShapeType::Ellipse:
        return "ellipse";
    case ShapeType::Line:
        return "line";
    case ShapeType::Arrow:
        return "arrow";
    case ShapeType::RoundedRectangle:
        return "rounded-rectangle";
    case ShapeType::Polygon:
        return "polygon";
    }
    return "rectangle";
}

bool parseShapeType(const std::string& value, ShapeType& out) noexcept {
    if (value == "rectangle") {
        out = ShapeType::Rectangle;
        return true;
    }
    if (value == "ellipse") {
        out = ShapeType::Ellipse;
        return true;
    }
    if (value == "line") {
        out = ShapeType::Line;
        return true;
    }
    if (value == "arrow") {
        out = ShapeType::Arrow;
        return true;
    }
    if (value == "rounded-rectangle" || value == "rounded_rectangle") {
        out = ShapeType::RoundedRectangle;
        return true;
    }
    if (value == "polygon") {
        out = ShapeType::Polygon;
        return true;
    }
    return false;
}

const char* strokeStyleName(StrokeStyle style) noexcept {
    switch (style) {
    case StrokeStyle::Solid:
        return "solid";
    case StrokeStyle::Dashed:
        return "dashed";
    case StrokeStyle::Dotted:
        return "dotted";
    }
    return "solid";
}

bool parseStrokeStyle(const std::string& value, StrokeStyle& out) noexcept {
    if (value == "solid") {
        out = StrokeStyle::Solid;
        return true;
    }
    if (value == "dashed") {
        out = StrokeStyle::Dashed;
        return true;
    }
    if (value == "dotted") {
        out = StrokeStyle::Dotted;
        return true;
    }
    return false;
}

bool isBoxShape(ShapeType type) noexcept {
    return type == ShapeType::Rectangle || type == ShapeType::Ellipse ||
           type == ShapeType::RoundedRectangle;
}

bool isLineShape(ShapeType type) noexcept {
    return type == ShapeType::Line || type == ShapeType::Arrow;
}

ShapeBounds normalizeRect(double x0, double y0, double x1, double y1) noexcept {
    ShapeBounds bounds;
    bounds.x = std::min(x0, x1);
    bounds.y = std::min(y0, y1);
    bounds.width = std::abs(x1 - x0);
    bounds.height = std::abs(y1 - y0);
    return bounds;
}

ShapeBounds rotateBounds(
    const ShapeBounds& bounds,
    double pivotX,
    double pivotY,
    double degrees
) noexcept {
    const ShapePoint corners[4] = {
        {bounds.x, bounds.y},
        {bounds.right(), bounds.y},
        {bounds.right(), bounds.bottom()},
        {bounds.x, bounds.bottom()},
    };
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    for (int i = 0; i < 4; ++i) {
        const ShapePoint rotated = rotatePoint(corners[i], pivotX, pivotY, degrees);
        if (i == 0) {
            minX = maxX = rotated.x;
            minY = maxY = rotated.y;
        } else {
            minX = std::min(minX, rotated.x);
            minY = std::min(minY, rotated.y);
            maxX = std::max(maxX, rotated.x);
            maxY = std::max(maxY, rotated.y);
        }
    }
    return ShapeBounds{minX, minY, maxX - minX, maxY - minY};
}

ShapeBounds shapeGeometryBounds(const ShapeLayerData& shape) noexcept {
    if (isLineShape(shape.type)) {
        return normalizeRect(shape.startX, shape.startY, shape.endX, shape.endY);
    }
    if (shape.type == ShapeType::Polygon) {
        if (shape.points.empty()) {
            return ShapeBounds{};
        }
        double minX = shape.points.front().x;
        double minY = shape.points.front().y;
        double maxX = minX;
        double maxY = minY;
        for (const ShapePoint& point : shape.points) {
            minX = std::min(minX, point.x);
            minY = std::min(minY, point.y);
            maxX = std::max(maxX, point.x);
            maxY = std::max(maxY, point.y);
        }
        return ShapeBounds{minX, minY, maxX - minX, maxY - minY};
    }
    return ShapeBounds{shape.x, shape.y, shape.width, shape.height};
}

ShapePoint shapeCentre(const ShapeLayerData& shape) noexcept {
    const ShapeBounds bounds = shapeGeometryBounds(shape);
    return ShapePoint{bounds.centreX(), bounds.centreY()};
}

ShapeBounds shapeVisualBounds(const ShapeLayerData& shape) noexcept {
    ShapeBounds base = shapeGeometryBounds(shape);
    if (shape.type == ShapeType::Arrow) {
        const double extent = std::max(0.0, static_cast<double>(shape.arrow.length));
        base.x -= extent;
        base.y -= extent;
        base.width += extent * 2.0;
        base.height += extent * 2.0;
    }
    const ShapePoint centre = shapeCentre(shape);
    ShapeBounds rotated = rotateBounds(base, centre.x, centre.y, shape.rotation);
    if (shape.stroke.enabled && shape.stroke.width > 0.0F) {
        const double half = static_cast<double>(shape.stroke.width) * 0.5;
        rotated.x -= half;
        rotated.y -= half;
        rotated.width += half * 2.0;
        rotated.height += half * 2.0;
    }
    return rotated;
}

void setRectGeometry(
    ShapeLayerData& shape,
    double x,
    double y,
    double width,
    double height
) noexcept {
    shape.x = width < 0.0 ? x + width : x;
    shape.y = height < 0.0 ? y + height : y;
    shape.width = std::abs(width);
    shape.height = std::abs(height);
}

void setLineGeometry(
    ShapeLayerData& shape,
    double startX,
    double startY,
    double endX,
    double endY
) noexcept {
    shape.startX = startX;
    shape.startY = startY;
    shape.endX = endX;
    shape.endY = endY;
    syncShapeBounds(shape);
}

void setPolygonGeometry(ShapeLayerData& shape, std::vector<ShapePoint> points) noexcept {
    shape.points = std::move(points);
    syncShapeBounds(shape);
}

void syncShapeBounds(ShapeLayerData& shape) noexcept {
    if (isLineShape(shape.type)) {
        const ShapeBounds bounds =
            normalizeRect(shape.startX, shape.startY, shape.endX, shape.endY);
        shape.x = bounds.x;
        shape.y = bounds.y;
        shape.width = bounds.width;
        shape.height = bounds.height;
        return;
    }
    if (shape.type == ShapeType::Polygon) {
        const ShapeBounds bounds = shapeGeometryBounds(shape);
        shape.x = bounds.x;
        shape.y = bounds.y;
        shape.width = bounds.width;
        shape.height = bounds.height;
        return;
    }
    // Box shapes: keep the bounds non-negative.
    if (shape.width < 0.0) {
        shape.x += shape.width;
        shape.width = -shape.width;
    }
    if (shape.height < 0.0) {
        shape.y += shape.height;
        shape.height = -shape.height;
    }
}

void translateShape(ShapeLayerData& shape, double dx, double dy) noexcept {
    if (isLineShape(shape.type)) {
        shape.startX += dx;
        shape.startY += dy;
        shape.endX += dx;
        shape.endY += dy;
        syncShapeBounds(shape);
        return;
    }
    if (shape.type == ShapeType::Polygon) {
        for (ShapePoint& point : shape.points) {
            point.x += dx;
            point.y += dy;
        }
        syncShapeBounds(shape);
        return;
    }
    shape.x += dx;
    shape.y += dy;
}

void scaleShape(
    ShapeLayerData& shape,
    double originX,
    double originY,
    double scaleX,
    double scaleY
) noexcept {
    const auto scaleXValue = [originX, scaleX](double value) {
        return originX + (value - originX) * scaleX;
    };
    const auto scaleYValue = [originY, scaleY](double value) {
        return originY + (value - originY) * scaleY;
    };

    if (isLineShape(shape.type)) {
        shape.startX = scaleXValue(shape.startX);
        shape.endX = scaleXValue(shape.endX);
        shape.startY = scaleYValue(shape.startY);
        shape.endY = scaleYValue(shape.endY);
        syncShapeBounds(shape);
        return;
    }
    if (shape.type == ShapeType::Polygon) {
        for (ShapePoint& point : shape.points) {
            point.x = scaleXValue(point.x);
            point.y = scaleYValue(point.y);
        }
        syncShapeBounds(shape);
        return;
    }
    const double left = scaleXValue(shape.x);
    const double right = scaleXValue(shape.x + shape.width);
    const double top = scaleYValue(shape.y);
    const double bottom = scaleYValue(shape.y + shape.height);
    setRectGeometry(shape, left, top, right - left, bottom - top);
}

void rotateShape(
    ShapeLayerData& shape,
    double pivotX,
    double pivotY,
    double degrees
) noexcept {
    if (!std::isfinite(degrees) || degrees == 0.0) {
        return;
    }
    const ShapePoint centre = shapeCentre(shape);
    const ShapePoint rotated = rotatePoint(centre, pivotX, pivotY, degrees);
    translateShape(shape, rotated.x - centre.x, rotated.y - centre.y);
    shape.rotation = normalizeAngle(shape.rotation + degrees);
}

void flipShape(
    ShapeLayerData& shape,
    bool horizontal,
    double canvasWidth,
    double canvasHeight
) noexcept {
    if (horizontal) {
        if (isLineShape(shape.type)) {
            shape.startX = canvasWidth - shape.startX;
            shape.endX = canvasWidth - shape.endX;
            syncShapeBounds(shape);
        } else if (shape.type == ShapeType::Polygon) {
            for (ShapePoint& point : shape.points) {
                point.x = canvasWidth - point.x;
            }
            syncShapeBounds(shape);
        } else {
            shape.x = canvasWidth - (shape.x + shape.width);
        }
        shape.rotation = normalizeAngle(180.0 - shape.rotation);
    } else {
        if (isLineShape(shape.type)) {
            shape.startY = canvasHeight - shape.startY;
            shape.endY = canvasHeight - shape.endY;
            syncShapeBounds(shape);
        } else if (shape.type == ShapeType::Polygon) {
            for (ShapePoint& point : shape.points) {
                point.y = canvasHeight - point.y;
            }
            syncShapeBounds(shape);
        } else {
            shape.y = canvasHeight - (shape.y + shape.height);
        }
        shape.rotation = normalizeAngle(-shape.rotation);
    }
}

bool shapeHitTest(
    const ShapeLayerData& shape,
    double documentX,
    double documentY,
    double tolerance
) noexcept {
    const ShapePoint centre = shapeCentre(shape);
    const ShapePoint local =
        rotatePoint(ShapePoint{documentX, documentY}, centre.x, centre.y, -shape.rotation);

    const double strokeHalf =
        (shape.stroke.enabled && shape.stroke.width > 0.0F)
        ? static_cast<double>(shape.stroke.width) * 0.5
        : 0.0;
    const double pad = std::max(0.0, tolerance) + strokeHalf;

    switch (shape.type) {
    case ShapeType::Rectangle:
    case ShapeType::RoundedRectangle:
        return local.x >= shape.x - pad &&
               local.x <= shape.x + shape.width + pad &&
               local.y >= shape.y - pad &&
               local.y <= shape.y + shape.height + pad;
    case ShapeType::Ellipse: {
        const double rx = shape.width * 0.5 + pad;
        const double ry = shape.height * 0.5 + pad;
        if (rx <= 0.0 || ry <= 0.0) {
            return false;
        }
        const double nx = (local.x - (shape.x + shape.width * 0.5)) / rx;
        const double ny = (local.y - (shape.y + shape.height * 0.5)) / ry;
        return nx * nx + ny * ny <= 1.0;
    }
    case ShapeType::Line:
    case ShapeType::Arrow: {
        const double distance = pointToSegmentDistance(
            local,
            ShapePoint{shape.startX, shape.startY},
            ShapePoint{shape.endX, shape.endY}
        );
        return distance <= std::max(pad, 1.0);
    }
    case ShapeType::Polygon: {
        if (shape.points.empty()) {
            return false;
        }
        if (pointInPolygon(local, shape.points)) {
            return true;
        }
        const double slack = std::max(pad, 1.0);
        for (std::size_t i = 0; i < shape.points.size(); ++i) {
            const ShapePoint& a = shape.points[i];
            const ShapePoint& b = shape.points[(i + 1) % shape.points.size()];
            if (pointToSegmentDistance(local, a, b) <= slack) {
                return true;
            }
        }
        return false;
    }
    }
    return false;
}

void sanitizeShape(ShapeLayerData& shape) noexcept {
    ensureFinite(shape.x);
    ensureFinite(shape.y);
    ensureFinite(shape.width);
    ensureFinite(shape.height);
    ensureFinite(shape.startX);
    ensureFinite(shape.startY);
    ensureFinite(shape.endX);
    ensureFinite(shape.endY);
    ensureFinite(shape.rotation);
    ensureFinite(shape.cornerRadius);

    shape.x = clampCoordinate(shape.x);
    shape.y = clampCoordinate(shape.y);
    shape.startX = clampCoordinate(shape.startX);
    shape.startY = clampCoordinate(shape.startY);
    shape.endX = clampCoordinate(shape.endX);
    shape.endY = clampCoordinate(shape.endY);

    if (shape.type == ShapeType::Polygon) {
        if (shape.points.size() > ShapeLayerData::maxPoints) {
            shape.points.resize(ShapeLayerData::maxPoints);
        }
        for (ShapePoint& point : shape.points) {
            ensureFinite(point.x);
            ensureFinite(point.y);
            point.x = clampCoordinate(point.x);
            point.y = clampCoordinate(point.y);
        }
    }

    syncShapeBounds(shape);
    shape.width = clampDimension(shape.width);
    shape.height = clampDimension(shape.height);

    shape.rotation = normalizeAngle(shape.rotation);
    shape.cornerRadius = static_cast<float>(
        std::clamp(
            static_cast<double>(shape.cornerRadius),
            0.0,
            ShapeLayerData::maxDimension
        )
    );
    ensureFinite(shape.stroke.width);
    shape.stroke.width = std::clamp(shape.stroke.width, 0.0F, ShapeLayerData::maxStrokeWidth);
    ensureFinite(shape.arrow.length);
    ensureFinite(shape.arrow.width);
    shape.arrow.length = std::clamp(shape.arrow.length, 0.0F, ShapeLayerData::maxStrokeWidth);
    shape.arrow.width = std::clamp(shape.arrow.width, 0.0F, ShapeLayerData::maxStrokeWidth);
}

void ShapeLayerData::validate() const {
    const auto requireFinite = [](double value, const char* field) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(
                std::string("Shape ") + field + " must be finite."
            );
        }
    };

    requireFinite(x, "x");
    requireFinite(y, "y");
    requireFinite(width, "width");
    requireFinite(height, "height");
    requireFinite(startX, "startX");
    requireFinite(startY, "startY");
    requireFinite(endX, "endX");
    requireFinite(endY, "endY");
    requireFinite(rotation, "rotation");
    requireFinite(cornerRadius, "cornerRadius");
    requireFinite(stroke.width, "stroke width");
    requireFinite(arrow.length, "arrow length");
    requireFinite(arrow.width, "arrow width");

    if (width < 0.0 || height < 0.0) {
        throw std::invalid_argument("Shape dimensions must be non-negative.");
    }
    if (width > maxDimension || height > maxDimension) {
        throw std::invalid_argument("Shape dimensions exceed the supported size.");
    }
    const auto withinCoordinate = [](double value) {
        return value >= -maxCoordinate && value <= maxCoordinate;
    };
    if (!withinCoordinate(x) || !withinCoordinate(y) ||
        !withinCoordinate(startX) || !withinCoordinate(startY) ||
        !withinCoordinate(endX) || !withinCoordinate(endY)) {
        throw std::invalid_argument("Shape coordinates are outside the supported range.");
    }
    if (cornerRadius < 0.0F) {
        throw std::invalid_argument("Corner radius must be non-negative.");
    }
    if (stroke.width < 0.0F || stroke.width > maxStrokeWidth) {
        throw std::invalid_argument("Stroke width is outside the supported range.");
    }
    if (arrow.length < 0.0F || arrow.length > maxStrokeWidth ||
        arrow.width < 0.0F || arrow.width > maxStrokeWidth) {
        throw std::invalid_argument("Arrowhead size is outside the supported range.");
    }
    if (type == ShapeType::Polygon) {
        if (points.size() > maxPoints) {
            throw std::invalid_argument("Polygon has too many vertices.");
        }
        for (const ShapePoint& point : points) {
            requireFinite(point.x, "vertex x");
            requireFinite(point.y, "vertex y");
            if (!withinCoordinate(point.x) || !withinCoordinate(point.y)) {
                throw std::invalid_argument(
                    "Polygon vertex is outside the supported range."
                );
            }
        }
    }
}

bool ShapeLayerData::isValid() const noexcept {
    try {
        validate();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace glyma::core
