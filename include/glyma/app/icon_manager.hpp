#pragma once

#include <QColor>
#include <QIcon>
#include <QSize>
#include <QString>
#include <QStringList>

namespace glyma::app {

// Centralized, theme-aware icon loader for the Glyma user interface.
//
// Widgets reference icons by *semantic name* (for example "brush" or "undo"),
// never by file path. The semantic name -> asset mapping lives in
// resources/icon_manifest.txt; assets are bundled into the application through
// a Qt resource file generated from that manifest at configure time.
//
// Icons are resolved lazily on first use, rasterized from SVG at the requested
// size (with a high-DPI variant), tinted to the active theme, and cached. A
// missing icon name yields a predictable placeholder instead of an empty icon.
class IconManager {
public:
    // Active theme used to tint icons. Auto derives the tint from the
    // application palette; the main window pins this explicitly because its
    // stylesheet does not update the palette.
    enum class Theme {
        Auto,
        Light,
        Dark,
    };

    // Returns the themed icon for a semantic name (all standard sizes).
    [[nodiscard]] static QIcon icon(const QString& name);

    // Returns the themed icon rendered for a single size.
    [[nodiscard]] static QIcon icon(const QString& name, const QSize& size);

    // Returns the icon rendered for a single size in an explicit color.
    [[nodiscard]] static QIcon icon(
        const QString& name,
        const QSize& size,
        const QColor& color
    );

    // True when a semantic name is known (present in the manifest).
    [[nodiscard]] static bool contains(const QString& name);

    // The bundled asset filename for a semantic name, or an empty string.
    [[nodiscard]] static QString sourceFileName(const QString& name);

    // The Qt resource path for a semantic name, or an empty string.
    [[nodiscard]] static QString resourcePath(const QString& name);

    // All known semantic names, sorted.
    [[nodiscard]] static QStringList names();

    // Centralized default sizes. Widgets should use these instead of hardcoding
    // their own values so spacing and scaling stay consistent.
    [[nodiscard]] static QSize toolbarIconSize();
    [[nodiscard]] static QSize menuIconSize();
    [[nodiscard]] static QSize panelIconSize();
    [[nodiscard]] static QSize toolButtonIconSize();

    [[nodiscard]] static Theme theme();
    static void setTheme(Theme theme);

    // The neutral color currently used for themed icons.
    [[nodiscard]] static QColor defaultIconColor();

    // Drops cached icons (useful after a theme change).
    static void clearCache();
};

} // namespace glyma::app
