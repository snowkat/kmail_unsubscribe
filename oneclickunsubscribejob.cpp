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
    // RFC 8058 forbids cookies, authorization, and other browsing context.
    // Redirects are forbidden for one-click endpoints, so leave them for the
    // caller to treat as an unsuccessful response.
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    qCDebug(UnsubscribePlugin) << "Sending one-click unsubscribe request to host" << mUrl.host();

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
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300)
    {
        qCDebug(UnsubscribePlugin) << "Successful response from unsubscribe host" << mUrl.host();
        UnsubscribeManager::Result successResult = {
            .Type = UnsubscribeManager::Result::None,
        };
        Q_EMIT result(successResult);
    }
    else if (reply->error() == QNetworkReply::NoError)
    {
        qCWarning(UnsubscribePlugin) << "Unexpected HTTP response from unsubscribe host" << mUrl.host() << ':' << status;
        UnsubscribeManager::Result errorResult = {
            .Type = UnsubscribeManager::Result::NetworkError,
            .ErrorString = QString::number(status),
        };
        Q_EMIT result(errorResult);
    }
    else
    {
        // QNetworkReply error strings can contain the full URL, including its
        // opaque recipient token.
        qCWarning(UnsubscribePlugin) << "Request to unsubscribe host" << mUrl.host() << "failed with network error" << reply->error();
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
