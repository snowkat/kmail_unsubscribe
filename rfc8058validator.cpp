#include "rfc8058validator.h"

bool MessageViewer::Rfc8058::isValidPostHeaderValue(QStringView value)
{
    return value.trimmed() == QLatin1StringView("List-Unsubscribe=One-Click");
}

bool MessageViewer::Rfc8058::isValidPostUrl(const QUrl &url)
{
    return url.isValid() && url.scheme() == QLatin1StringView("https") && !url.host().isEmpty()
        && url.userName().isEmpty() && url.password().isEmpty();
}

QUrl MessageViewer::Rfc8058::oneClickPostUrl(const QList<QUrl> &urls)
{
    QUrl postUrl;
    for (const QUrl &url : urls)
    {
        if (url.scheme().compare(QLatin1StringView("https"), Qt::CaseInsensitive) == 0)
        {
            if (!postUrl.isEmpty() || !isValidPostUrl(url))
            {
                return {};
            }
            postUrl = url;
        }
        else if (url.scheme().compare(QLatin1StringView("http"), Qt::CaseInsensitive) == 0)
        {
            // RFC 8058 permits additional non-HTTP/S URIs, but exactly one
            // HTTPS URI is the web endpoint.
            return {};
        }
    }
    return postUrl;
}

bool MessageViewer::Rfc8058::hasExactlyOneRequiredHeaderPair(const QStringList &headerNames)
{
    qsizetype unsubscribeCount = 0;
    qsizetype postCount = 0;
    for (const QString &headerName : headerNames)
    {
        if (headerName.compare(QStringLiteral("List-Unsubscribe"), Qt::CaseInsensitive) == 0)
        {
            ++unsubscribeCount;
        }
        else if (headerName.compare(QStringLiteral("List-Unsubscribe-Post"), Qt::CaseInsensitive) == 0)
        {
            ++postCount;
        }
    }
    return unsubscribeCount == 1 && postCount == 1;
}

bool MessageViewer::Rfc8058::isCryptographicallyValid(DKIMCheckSignatureJob::DKIMStatus status)
{
    return status == DKIMCheckSignatureJob::DKIMStatus::Valid;
}

bool MessageViewer::Rfc8058::signatureCoversRequiredHeaders(const QStringList &signedHeaders)
{
    return signedHeaders.contains(QStringLiteral("List-Unsubscribe"), Qt::CaseInsensitive)
        && signedHeaders.contains(QStringLiteral("List-Unsubscribe-Post"), Qt::CaseInsensitive);
}
