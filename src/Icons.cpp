#include "Icons.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QSvgRenderer>
#include <memory>

namespace Icons {

namespace {

constexpr const char *kPrefix = "fluent:";

QString path(const QString &name, int designSize, bool filled)
{
    return QString(":/icons/fluent/ic_fluent_%1_%2_%3.svg")
        .arg(name).arg(designSize).arg(filled ? "filled" : "regular");
}

// Each size is drawn separately (I4); scaling only fills in for the screen's sharpness.
int designSizeFor(int size) { return size >= 20 ? 24 : 16; }

QHash<QString, QPixmap> &cache()
{
    static QHash<QString, QPixmap> pixmaps;
    static bool ready = false;
    if (!ready) {
        ready = true;   // pixmaps must be gone before the application object is
        if (QCoreApplication::instance())
            QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                             [] { pixmaps.clear(); });
    }
    return pixmaps;
}

// Draws the icon in one colour wherever a QIcon is painted without ClayStyle's help.
class FluentEngine : public QIconEngine
{
public:
    explicit FluentEngine(const QString &name) : m_name(name) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        const int size = qMin(rect.width(), rect.height());
        const QPixmap pm = Icons::pixmap(m_name, size, colorFor(mode), state == QIcon::On, dpr);
        painter->drawPixmap(QRect(rect.center() - QPoint(size / 2, size / 2), QSize(size, size)), pm);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return Icons::pixmap(m_name, qMin(size.width(), size.height()), colorFor(mode),
                             state == QIcon::On, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        return Icons::pixmap(m_name, qMin(size.width(), size.height()), colorFor(mode),
                             state == QIcon::On, scale);
    }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override
    {
        const int s = qMin(size.width(), size.height());
        return QSize(s, s);
    }

    QString key() const override { return QStringLiteral("FluentEngine"); }
    QIconEngine *clone() const override { return new FluentEngine(m_name); }
    QString iconName() override { return QString(kPrefix) + m_name; }
    bool isNull() override { return !Icons::exists(m_name); }

private:
    static QColor colorFor(QIcon::Mode mode)
    {
        const QPalette pal = QApplication::palette();
        if (mode == QIcon::Disabled)
            return pal.color(QPalette::Disabled, QPalette::ButtonText);
        if (mode == QIcon::Selected)
            return pal.color(QPalette::HighlightedText);
        return pal.color(QPalette::ButtonText);
    }

    QString m_name;
};

} // namespace

QIcon icon(const QString &name)
{
    return QIcon(new FluentEngine(name));
}

QString nameOf(const QIcon &icon)
{
    const QString n = icon.name();
    return n.startsWith(kPrefix) ? n.mid(int(qstrlen(kPrefix))) : QString();
}

bool exists(const QString &name)
{
    return !name.isEmpty() && QFile::exists(path(name, 16, false));
}

QPixmap pixmap(const QString &name, int size, const QColor &color, bool filled, qreal dpr)
{
    if (name.isEmpty() || size <= 0)
        return QPixmap();
    const QString key = QString("%1|%2|%3|%4|%5").arg(name).arg(size).arg(color.rgba())
                            .arg(filled).arg(dpr, 0, 'f', 2);
    auto &pixmaps = cache();
    if (const auto it = pixmaps.constFind(key); it != pixmaps.constEnd())
        return it.value();

    const int px = qRound(size * dpr);
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QSvgRenderer renderer(path(name, designSizeFor(size), filled));
    if (renderer.isValid()) {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p, QRectF(0, 0, px, px));
        // One solid colour (I3): keep the shape, replace its colour.
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(img.rect(), color);
    }
    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(dpr);
    pixmaps.insert(key, pm);
    return pm;
}

} // namespace Icons
