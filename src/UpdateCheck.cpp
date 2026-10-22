#include "UpdateCheck.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
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
    if (r.tag.isEmpty())
        return std::nullopt;
    return r;
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
