#include "UpdateCheck.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <algorithm>

namespace UpdateCheck {

namespace {

QList<int> numbers(QString text)
{
    text = text.trimmed();
    if (text.startsWith('v') || text.startsWith('V'))
        text.remove(0, 1);
    QList<int> out;
    for (const QString &part : text.split('.')) {
        int value = 0;
        int digits = 0;
        for (const QChar c : part) {
            if (!c.isDigit())
                break;                          // "3-beta" counts as 3
            value = value * 10 + c.digitValue();
            ++digits;
        }
        out << (digits > 0 ? value : 0);
    }
    return out;
}

} // namespace

std::optional<Release> parseRelease(const QByteArray &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return std::nullopt;
    const QJsonObject o = doc.object();
    Release r;
    r.tag = o.value("tag_name").toString().trimmed();
    r.url = o.value("html_url").toString().trimmed();
    if (!isVersionTag(r.tag))
        return std::nullopt;
    return r;
}

bool isVersionTag(const QString &tag)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[vV]?\\d{1,4}(\\.\\d{1,4}){0,3}(-[0-9A-Za-z.]{1,20})?$"));
    return pattern.match(tag).hasMatch();
}

QString releasePagesFor(const QString &apiUrl)
{
    const QUrl u(apiUrl, QUrl::StrictMode);
    if (!u.isValid() || u.scheme() != "https" || u.host() != "api.github.com"
        || !u.userInfo().isEmpty() || (u.port() != -1 && u.port() != 443)
        || u.hasQuery() || u.hasFragment())
        return QString();
    const QStringList parts = u.path().split('/', Qt::SkipEmptyParts);
    if (parts.size() != 5 || parts[0] != "repos" || parts[3] != "releases" || parts[4] != "latest")
        return QString();
    static const QRegularExpression name(QStringLiteral("^[A-Za-z0-9._-]{1,100}$"));
    for (const QString &part : {parts[1], parts[2]})
        if (!name.match(part).hasMatch() || part == "." || part == "..")
            return QString();
    return QString("https://github.com/%1/%2/releases/").arg(parts[1], parts[2]);
}

bool isSafeReleaseUrl(const QString &url, const QString &apiUrl)
{
    const QString pages = releasePagesFor(apiUrl);
    if (pages.isEmpty())
        return false;
    const QUrl u(url, QUrl::StrictMode);
    if (!u.isValid() || u.scheme() != "https" || u.host() != "github.com"
        || !u.userInfo().isEmpty() || (u.port() != -1 && u.port() != 443))
        return false;
    const QString path = u.path(QUrl::FullyDecoded);
    if (path.contains("..") || path.contains('\\'))
        return false;                            // no climbing out of the release pages
    return path.startsWith(QUrl(pages).path(), Qt::CaseInsensitive);
}

bool isNewer(const QString &latest, const QString &current)
{
    QList<int> a = numbers(latest);
    QList<int> b = numbers(current);
    const int n = int(std::max(a.size(), b.size()));
    while (a.size() < n) a << 0;                // 1.2 is the same as 1.2.0
    while (b.size() < n) b << 0;
    for (int i = 0; i < n; ++i)
        if (a[i] != b[i])
            return a[i] > b[i];
    return false;
}

} // namespace UpdateCheck
