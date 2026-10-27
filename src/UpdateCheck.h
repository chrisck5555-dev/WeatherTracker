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

// GitHub's answer is about 5 KB. Anything far bigger is not a real answer.
inline constexpr qint64 kMaxReplyBytes = 1024 * 1024;

// Reads the JSON that https://api.github.com/repos/OWNER/REPO/releases/latest returns.
// Unset when it is not a release (no tag, or a tag that is not a version number).
std::optional<Release> parseRelease(const QByteArray &json);

// "v1.2.3", "1.2", "v1.2.0-beta": a version number and nothing else. The tag is shown to
// her, so anything else (HTML, very long text) is refused.
bool isVersionTag(const QString &tag);

// The release pages of the repository that `apiUrl` asks about:
// "https://api.github.com/repos/OWNER/REPO/releases/latest" -> "https://github.com/OWNER/REPO/releases/".
// Empty when `apiUrl` is not exactly that form over HTTPS.
QString releasePagesFor(const QString &apiUrl);
inline bool isSafeApiUrl(const QString &apiUrl) { return !releasePagesFor(apiUrl).isEmpty(); }

// True only for an HTTPS page on github.com under releasePagesFor(apiUrl). The program
// opens this link in her browser, so a link anywhere else is never used.
bool isSafeReleaseUrl(const QString &url, const QString &apiUrl);

// True when `latest` ("v1.10.0" or "1.10.0") is a higher version than `current`.
// Compared number by number, so 1.10.0 is newer than 1.9.0. Text after a number is ignored.
bool isNewer(const QString &latest, const QString &current);

} // namespace UpdateCheck
