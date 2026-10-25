#include <cassert>
#include <iostream>

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>

#include "glyma/app/icon_manager.hpp"

namespace {

using glyma::app::IconManager;

int opaquePixelCount(const QImage& image) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                ++count;
            }
        }
    }
    return count;
}

QColor firstOpaqueColor(const QIcon& icon, const QSize& size) {
    const QImage image = icon.pixmap(size).toImage();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                return image.pixelColor(x, y);
            }
        }
    }
    return QColor();
}

void testManifestLoaded() {
    assert(IconManager::contains(QStringLiteral("brush")));
    assert(IconManager::sourceFileName(QStringLiteral("brush")) ==
           QStringLiteral("brush.svg"));
    assert(IconManager::resourcePath(QStringLiteral("brush")) ==
           QStringLiteral(":/glyma/icons/brush.svg"));
    assert(QFile::exists(QStringLiteral(":/glyma/icon_manifest.txt")));

    const QStringList names = IconManager::names();
    assert(names.size() >= 30);
    QStringList sorted = names;
    sorted.sort();
    assert(sorted == names);

    assert(!IconManager::contains(QStringLiteral("no-such-icon")));
}

void testEveryManifestEntryResolves() {
    const QStringList names = IconManager::names();
    assert(!names.isEmpty());
    for (const QString& name : names) {
        const QString path = IconManager::resourcePath(name);
        assert(!path.isEmpty());
        assert(QFile::exists(path));

        const QIcon icon = IconManager::icon(name);
        assert(!icon.isNull());
        const QImage image = icon.pixmap(IconManager::toolbarIconSize()).toImage();
        assert(!image.isNull());
        assert(opaquePixelCount(image) > 0);
    }
}

void testMissingIconFallback() {
    const QString name = QStringLiteral("definitely-not-an-icon");
    assert(!IconManager::contains(name));
    assert(IconManager::sourceFileName(name).isEmpty());
    assert(IconManager::resourcePath(name).isEmpty());

    const QIcon fallback = IconManager::icon(name);
    assert(!fallback.isNull());
    const QImage image = fallback.pixmap(IconManager::toolbarIconSize()).toImage();
    assert(!image.isNull());
    assert(opaquePixelCount(image) > 0);
}

void testDefaultSizes() {
    assert(IconManager::toolbarIconSize() == QSize(20, 20));
    assert(IconManager::menuIconSize() == QSize(18, 18));
    assert(IconManager::panelIconSize() == QSize(18, 18));
    assert(IconManager::toolButtonIconSize() == QSize(24, 24));
}

void testThemeSelection() {
    IconManager::setTheme(IconManager::Theme::Dark);
    const QColor dark = IconManager::defaultIconColor();
    const QColor darkPixel = firstOpaqueColor(
        IconManager::icon(QStringLiteral("brush"), QSize(24, 24)), QSize(24, 24)
    );
    assert(darkPixel.isValid());
    assert(darkPixel.red() > 128 && darkPixel.green() > 128);

    IconManager::setTheme(IconManager::Theme::Light);
    const QColor light = IconManager::defaultIconColor();
    const QColor lightPixel = firstOpaqueColor(
        IconManager::icon(QStringLiteral("brush"), QSize(24, 24)), QSize(24, 24)
    );
    assert(lightPixel.isValid());
    assert(lightPixel.red() < 128 && lightPixel.green() < 128);

    assert(dark != light);
    assert(darkPixel != lightPixel);

    IconManager::setTheme(IconManager::Theme::Dark);
}

void testExplicitColor() {
    const QColor red(255, 0, 0);
    const QIcon icon =
        IconManager::icon(QStringLiteral("brush"), QSize(24, 24), red);
    const QColor sampled = firstOpaqueColor(icon, QSize(24, 24));
    assert(sampled.isValid());
    assert(sampled.red() > 200 && sampled.green() < 40 && sampled.blue() < 40);
}

void testHighDpi() {
    const QIcon icon = IconManager::icon(QStringLiteral("brush"));
    const QPixmap normal = icon.pixmap(QSize(20, 20), 1.0);
    const QPixmap hidpi = icon.pixmap(QSize(20, 20), 2.0);
    assert(!normal.isNull());
    assert(!hidpi.isNull());
    assert(hidpi.width() >= normal.width());
}

void testCacheReset() {
    IconManager::clearCache();
    const QIcon icon = IconManager::icon(QStringLiteral("brush"));
    assert(!icon.isNull());
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Glyma Icon Tests"));

    testManifestLoaded();
    testEveryManifestEntryResolves();
    testMissingIconFallback();
    testDefaultSizes();
    testThemeSelection();
    testExplicitColor();
    testHighDpi();
    testCacheReset();

    std::cout << "All Glyma icon tests passed\n";
    return 0;
}
