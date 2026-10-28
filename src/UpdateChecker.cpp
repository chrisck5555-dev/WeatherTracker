#include "UpdateChecker.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <memory>

#include "UpdateCheck.h"
#include "Version.h"

namespace {
const QString kUnreadable = QStringLiteral("The update page gave an answer the program could not read.");
}

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {}

void UpdateChecker::check()
{
    if (!m_allowed) {                               // she turned it off: no connection at all
        Result r;
        r.error = "Checking for updates is turned off in Settings, under \"Your privacy\".";
        emit finished(r);
        return;
    }
    const QString api = QString::fromUtf8(APP_UPDATE_API);
    if (api.isEmpty()) {
        Result r;
        r.status = Result::NotSetUp;
        emit finished(r);
        return;
    }
    if (!UpdateCheck::isSafeApiUrl(api)) {          // HTTPS to api.github.com only
        Result r;
        r.error = "Checking for updates is not set up correctly.";
        emit finished(r);
        return;
    }

    QNetworkRequest request{QUrl(api)};
    request.setRawHeader("User-Agent", QByteArray("WeatherTracker/") + APP_VERSION);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(10000);              // give up after ten seconds
    // Follow a redirect only to the same site, over the same secure connection.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);

    QNetworkReply *reply = m_net.get(request);

    // Stop reading if the answer is far bigger than a real one.
    auto tooBig = std::make_shared<bool>(false);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply, tooBig](qint64 received, qint64) {
        if (received > UpdateCheck::kMaxReplyBytes && !*tooBig) {
            *tooBig = true;
            reply->abort();                         // finished() still follows, once
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, api, tooBig] {
        reply->deleteLater();
        Result r;
        if (*tooBig) {
            r.error = kUnreadable;
            emit finished(r);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            r.error = code == 404
                ? "No version has been published yet."
                : "The program could not reach the update page. Please check the internet "
                  "connection and try again.";
            emit finished(r);
            return;
        }
        const QByteArray body = reply->read(UpdateCheck::kMaxReplyBytes + 1);
        const auto release = body.size() > UpdateCheck::kMaxReplyBytes
            ? std::nullopt : UpdateCheck::parseRelease(body);
        if (!release) {
            r.error = kUnreadable;
            emit finished(r);
            return;
        }
        r.latest = release->tag;
        // Only ever open this repository's release pages; anything else falls back to them.
        r.url = UpdateCheck::isSafeReleaseUrl(release->url, api)
            ? release->url : UpdateCheck::releasePagesFor(api) + "latest";
        r.status = UpdateCheck::isNewer(release->tag, QString::fromUtf8(APP_VERSION))
                       ? Result::Available : Result::Newest;
        emit finished(r);
    });
}
