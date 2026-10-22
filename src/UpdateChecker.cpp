#include "UpdateChecker.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "UpdateCheck.h"
#include "Version.h"

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {}

void UpdateChecker::check()
{
    const QString api = QString::fromUtf8(APP_UPDATE_API);
    if (api.isEmpty()) {
        Result r;
        r.status = Result::NotSetUp;
        emit finished(r);
        return;
    }

    QNetworkRequest request{QUrl(api)};
    request.setRawHeader("User-Agent", QByteArray("WeatherTracker/") + APP_VERSION);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(10000);              // give up after ten seconds

    QNetworkReply *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        Result r;
        if (reply->error() != QNetworkReply::NoError) {
            const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            r.error = code == 404
                ? "No version has been published yet."
                : "The program could not reach the update page. Please check the internet "
                  "connection and try again.";
            emit finished(r);
            return;
        }
        const auto release = UpdateCheck::parseRelease(reply->readAll());
        if (!release) {
            r.error = "The update page gave an answer the program could not read.";
            emit finished(r);
            return;
        }
        r.latest = release->tag;
        r.url = release->url;
        r.status = UpdateCheck::isNewer(release->tag, QString::fromUtf8(APP_VERSION))
                       ? Result::Available : Result::Newest;
        emit finished(r);
    });
}
