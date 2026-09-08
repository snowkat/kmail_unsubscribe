#include "webunsubscribeprobe.h"

#include <QNetworkReply>
#include <QNetworkRequest>

#include <utility>

using namespace KMailUnsubscribe;

WebUnsubscribeProbe::WebUnsubscribeProbe(QUrl url, QObject *parent)
    : QObject(parent)
    , mNetworkAccessManager(this)
    , mUrl(std::move(url))
{
}

void WebUnsubscribeProbe::start()
{
    QNetworkRequest request(mUrl);
    // The probe deliberately has no browsing session. It must not carry
    // cookies or credentials from KMail, and it must not follow redirects.
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(5000);

    auto *reply = mNetworkAccessManager.head(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString mimeType = reply->header(QNetworkRequest::ContentTypeHeader)
                                     .toString()
                                     .section(QLatin1Char(';'), 0, 0)
                                     .trimmed()
                                     .toLower();
        const bool pageMissing = status == 404 || status == 410;
        const bool jsonResponse = mimeType == QLatin1StringView("application/json")
            || mimeType.endsWith(QLatin1StringView("+json"));
        Q_EMIT finished(!pageMissing && !jsonResponse, status, jsonResponse);
        reply->deleteLater();
        deleteLater();
    });
}
