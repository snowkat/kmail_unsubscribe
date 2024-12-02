#include "oneclickunsubscribejob.h"

#include <QNetworkReply>
#include <QHttpMultiPart>
#include "unsubscribe_debug.h"

using namespace MessageViewer;

OneClickUnsubscribeJob::OneClickUnsubscribeJob(QUrl &oneClickUrl, UnsubscribeManager *parent)
    : QObject(parent),
      mNetworkAccessManager(new QNetworkAccessManager(this))
{
    mNetworkAccessManager->setStrictTransportSecurityEnabled(true);
    mNetworkAccessManager->enableStrictTransportSecurityStore(true);

    connect(mNetworkAccessManager, &QNetworkAccessManager::finished, this, &OneClickUnsubscribeJob::slotFinished);
    connect(mNetworkAccessManager, &QNetworkAccessManager::sslErrors, this, &OneClickUnsubscribeJob::slotSslErrors);

    mUrl = oneClickUrl;
}

void OneClickUnsubscribeJob::start()
{
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);
    QHttpPart mainPart;
    // Per RFC 8058, only "List-Unsubscribe=One-Click" is allowed. To avoid
    // parsing issues, we simply won't parse anything
    mainPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"List-Unsubscribe\""));
    mainPart.setBody("One-Click");
    multiPart->append(mainPart);
    QNetworkRequest request(mUrl);

    qCDebug(UnsubscribePlugin) << "Sending one-click unsubscribe request to" << mUrl;

    mReply = mNetworkAccessManager->post(request, multiPart);
    connect(mReply, &QNetworkReply::errorOccurred, this, &OneClickUnsubscribeJob::slotError);
}

void OneClickUnsubscribeJob::slotSslErrors(QNetworkReply *reply, const QList<QSslError> &error)
{
    // TODO: allow override somehow
    qCDebug(UnsubscribePlugin) << "Got" << error.count() << "SSL error(s)";
    UnsubscribeManager::Result sslErrResult = {
        .Type = UnsubscribeManager::Result::SslError,
        .ErrorString = error.first().errorString(),
    };
    Q_EMIT result(sslErrResult);
}

void OneClickUnsubscribeJob::slotFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError)
    {
        qCDebug(UnsubscribePlugin) << "Successful response from" << mUrl << "with result" << reply->errorString();
        UnsubscribeManager::Result successResult = {
            .Type = UnsubscribeManager::Result::None,
        };
        Q_EMIT result(successResult);
    }
    else
    {
        qCWarning(UnsubscribePlugin) << "Failed response for" << mUrl << "completed with" << reply->errorString();
    }
}

void OneClickUnsubscribeJob::slotError(QNetworkReply::NetworkError error)
{
    UnsubscribeManager::Result errorResult = {
        .Type = UnsubscribeManager::Result::NetworkError,
        .ErrorString = mReply->errorString(),
    };
    Q_EMIT result(errorResult);
}

#include "moc_oneclickunsubscribejob.cpp"