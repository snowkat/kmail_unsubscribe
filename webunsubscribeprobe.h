#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

namespace KMailUnsubscribe
{
// A HEAD request can identify an expired web address without retrieving the
// page body or submitting an unsubscribe action. Other outcomes are left to
// the browser, which handles interactive web unsubscribe flows.
class WebUnsubscribeProbe : public QObject
{
    Q_OBJECT
public:
    explicit WebUnsubscribeProbe(QUrl url, QObject *parent = nullptr);
    void start();

Q_SIGNALS:
    // false is emitted for a terminal status or a JSON response, both of
    // which can safely fall through to the advertised email method.
    void finished(bool shouldOpenInBrowser, int httpStatus, bool jsonResponse);

private:
    QNetworkAccessManager mNetworkAccessManager;
    QUrl mUrl;
};
}
