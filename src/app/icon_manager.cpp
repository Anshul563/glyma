#include "glyma/app/icon_manager.hpp"

#include <algorithm>

#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QLoggingCategory>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSet>
#include <QTextStream>
#include <QVector>

Q_LOGGING_CATEGORY(glymaIconsLog, "glyma.icons")

// Defined in the global namespace so the block-scope `extern` inside
// Q_INIT_RESOURCE resolves to the generated (global) resource initializer.
static void glymaInitIconResources() {
    Q_INIT_RESOURCE(glyma_icons);
}

namespace glyma::app {
namespace {

constexpr char kManifestResource[] = ":/glyma/icon_manifest.txt";
constexpr char kIconResourcePrefix[] = ":/glyma/icons/";

// Neutral tints chosen to stay legible on the matching surface.
const QColor kDarkThemeTint(0xE4, 0xE4, 0xE6);
const QColor kLightThemeTint(0x14, 0x1B, 0x34);

// Sizes baked into a default icon so any widget can request a crisp pixmap
// without re-rasterizing.
const QVector<int>& defaultSizes() {
    static const QVector<int> sizes{16, 18, 20, 22, 24, 32, 48};
    return sizes;
}

IconManager::Theme& themeRef() {
    static IconManager::Theme theme = IconManager::Theme::Auto;
    return theme;
}

QHash<QString, QString>& mappingRef() {
    static QHash<QString, QString> mapping;
    return mapping;
}

QHash<QString, QIcon>& cacheRef() {
    static QHash<QString, QIcon> cache;
    return cache;
}

QHash<QString, QIcon>& variantCacheRef() {
    static QHash<QString, QIcon> cache;
    return cache;
}

QSet<QString>& warnedRef() {
    static QSet<QString> warned;
    return warned;
}

bool& loadedRef() {
    static bool loaded = false;
    return loaded;
}

QHash<QString, QString> parseManifest() {
    QHash<QString, QString> mapping;
    QFile file(QString::fromLatin1(kManifestResource));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(glymaIconsLog)
            << "icon manifest resource" << kManifestResource << "is unavailable";
        return mapping;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const qsizetype separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            qCWarning(glymaIconsLog) << "ignoring malformed manifest line:" << line;
            continue;
        }
        const QString name = line.left(separator).trimmed();
        const QString asset = line.mid(separator + 1).trimmed();
        if (name.isEmpty() || asset.isEmpty()) {
            qCWarning(glymaIconsLog) << "ignoring malformed manifest line:" << line;
            continue;
        }
        mapping.insert(name, asset);
    }
    return mapping;
}

void ensureLoaded() {
    if (loadedRef()) {
        return;
    }
    // Pull in the generated resource object so it survives static linking.
    glymaInitIconResources();
    mappingRef() = parseManifest();
    loadedRef() = true;
}

QColor resolveThemeColor() {
    switch (themeRef()) {
    case IconManager::Theme::Light:
        return kLightThemeTint;
    case IconManager::Theme::Dark:
        return kDarkThemeTint;
    case IconManager::Theme::Auto:
    default:
        break;
    }

    if (auto* application = qobject_cast<QGuiApplication*>(QGuiApplication::instance())) {
        const QColor window = application->palette().color(QPalette::Window);
        if (window.isValid() && window.lightness() < 128) {
            return kDarkThemeTint;
        }
    }
    return kLightThemeTint;
}

QPixmap renderPixmap(
    const QString& resourcePath,
    const QSize& logicalSize,
    qreal devicePixelRatio,
    const QColor& color
) {
    const QSize pixelSize(
        std::max(1, qRound(logicalSize.width() * devicePixelRatio)),
        std::max(1, qRound(logicalSize.height() * devicePixelRatio))
    );

    QImageReader reader(resourcePath);
    reader.setScaledSize(pixelSize);
    const QImage image = reader.read();
    if (image.isNull()) {
        return QPixmap();
    }

    QPixmap pixmap(pixelSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(0, 0, image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    pixmap.setDevicePixelRatio(devicePixelRatio);
    return pixmap;
}

QPixmap fallbackPixmap(const QSize& logicalSize, qreal devicePixelRatio, const QColor& color) {
    const QSize pixelSize(
        std::max(1, qRound(logicalSize.width() * devicePixelRatio)),
        std::max(1, qRound(logicalSize.height() * devicePixelRatio))
    );

    QPixmap pixmap(pixelSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color);
    pen.setWidthF(std::max(1.0, pixelSize.width() / 12.0));
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    const qreal inset = pen.widthF();
    painter.drawRoundedRect(
        QRectF(inset, inset, pixelSize.width() - 2.0 * inset, pixelSize.height() - 2.0 * inset),
        pixelSize.width() / 6.0,
        pixelSize.width() / 6.0
    );

    QFont font = painter.font();
    font.setPixelSize(static_cast<int>(pixelSize.height() * 0.6));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("?"));
    painter.end();

    pixmap.setDevicePixelRatio(devicePixelRatio);
    return pixmap;
}

QIcon buildIcon(const QString& resourcePath, const QColor& color, const QVector<int>& sizes) {
    QIcon icon;
    for (const int size : sizes) {
        for (const qreal ratio : {1.0, 2.0}) {
            const QPixmap pixmap =
                renderPixmap(resourcePath, QSize(size, size), ratio, color);
            if (!pixmap.isNull()) {
                icon.addPixmap(pixmap);
            }
        }
    }
    return icon;
}

QIcon buildFallbackIcon(const QColor& color, const QVector<int>& sizes) {
    QIcon icon;
    for (const int size : sizes) {
        for (const qreal ratio : {1.0, 2.0}) {
            icon.addPixmap(fallbackPixmap(QSize(size, size), ratio, color));
        }
    }
    return icon;
}

void warnMissing(const QString& name) {
    if (warnedRef().contains(name)) {
        return;
    }
    warnedRef().insert(name);
    qCWarning(glymaIconsLog)
        << "unknown icon" << name << "- falling back to placeholder";
}

} // namespace

QIcon IconManager::icon(const QString& name) {
    ensureLoaded();

    const QString path = resourcePath(name);
    if (path.isEmpty()) {
        warnMissing(name);
        return buildFallbackIcon(resolveThemeColor(), defaultSizes());
    }

    auto& cache = cacheRef();
    if (const auto it = cache.constFind(name); it != cache.constEnd()) {
        return it.value();
    }

    QIcon built = buildIcon(path, resolveThemeColor(), defaultSizes());
    if (built.isNull()) {
        warnMissing(name);
        built = buildFallbackIcon(resolveThemeColor(), defaultSizes());
    }
    cache.insert(name, built);
    return built;
}

QIcon IconManager::icon(const QString& name, const QSize& size) {
    return icon(name, size, defaultIconColor());
}

QIcon IconManager::icon(const QString& name, const QSize& size, const QColor& color) {
    ensureLoaded();

    const QVector<int> sizes{std::max(1, size.width())};
    const QString key = QStringLiteral("%1|%2|%3")
                            .arg(name)
                            .arg(size.width())
                            .arg(color.name(QColor::HexArgb));

    auto& cache = variantCacheRef();
    if (const auto it = cache.constFind(key); it != cache.constEnd()) {
        return it.value();
    }

    const QString path = resourcePath(name);
    QIcon built;
    if (path.isEmpty()) {
        warnMissing(name);
        built = buildFallbackIcon(color, sizes);
    } else {
        built = buildIcon(path, color, sizes);
        if (built.isNull()) {
            warnMissing(name);
            built = buildFallbackIcon(color, sizes);
        }
    }
    cache.insert(key, built);
    return built;
}

bool IconManager::contains(const QString& name) {
    ensureLoaded();
    return mappingRef().contains(name);
}

QString IconManager::sourceFileName(const QString& name) {
    ensureLoaded();
    return mappingRef().value(name);
}

QString IconManager::resourcePath(const QString& name) {
    ensureLoaded();
    const QString asset = mappingRef().value(name);
    if (asset.isEmpty()) {
        return QString();
    }
    return QString::fromLatin1(kIconResourcePrefix) + asset;
}

QStringList IconManager::names() {
    ensureLoaded();
    QStringList result = mappingRef().keys();
    result.sort();
    return result;
}

QSize IconManager::toolbarIconSize() {
    return QSize(20, 20);
}

QSize IconManager::menuIconSize() {
    return QSize(18, 18);
}

QSize IconManager::panelIconSize() {
    return QSize(18, 18);
}

QSize IconManager::toolButtonIconSize() {
    return QSize(24, 24);
}

IconManager::Theme IconManager::theme() {
    return themeRef();
}

void IconManager::setTheme(Theme theme) {
    if (themeRef() == theme) {
        return;
    }
    themeRef() = theme;
    clearCache();
}

QColor IconManager::defaultIconColor() {
    return resolveThemeColor();
}

void IconManager::clearCache() {
    cacheRef().clear();
    variantCacheRef().clear();
}

} // namespace glyma::app
