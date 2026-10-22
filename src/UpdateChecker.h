#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

// Asks GitHub whether a newer version exists. It only reads one small web page and
// never downloads or installs anything.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    struct Result {
        enum Status { NotSetUp, Newest, Available, Failed };
        Status status = Failed;
        QString latest;        // the newest version's tag, when known
        QString url;           // where to download it
        QString error;         // plain-English reason, when Failed
    };

    explicit UpdateChecker(QObject *parent = nullptr);

    void check();              // answers later through finished()

signals:
    void finished(const UpdateChecker::Result &result);

private:
    QNetworkAccessManager m_net;
};
