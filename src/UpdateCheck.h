#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

// Reading GitHub's "latest release" answer and comparing version numbers. No network
// and no GUI here, so it can be tested.
namespace UpdateCheck {

struct Release {
    QString tag;       // for example "v1.1.0"
    QString url;       // the release page, where the installer can be downloaded
};

// Reads the JSON that https://api.github.com/repos/OWNER/REPO/releases/latest returns.
// Unset when it is not a release (no tag).
std::optional<Release> parseRelease(const QByteArray &json);

// True when `latest` ("v1.10.0" or "1.10.0") is a higher version than `current`.
// Compared number by number, so 1.10.0 is newer than 1.9.0. Text after a number is ignored.
bool isNewer(const QString &latest, const QString &current);

} // namespace UpdateCheck
