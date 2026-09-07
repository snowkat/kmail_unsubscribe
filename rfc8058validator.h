#ifndef RFC8058VALIDATOR_H
#define RFC8058VALIDATOR_H

#include <QList>
#include <QStringList>
#include <QStringView>
#include <QUrl>
#include <MessageViewer/DKIMCheckSignatureJob>

namespace MessageViewer::Rfc8058
{
[[nodiscard]] bool isValidPostHeaderValue(QStringView value);
[[nodiscard]] bool isValidPostUrl(const QUrl &url);
[[nodiscard]] QUrl oneClickPostUrl(const QList<QUrl> &urls);
[[nodiscard]] bool hasExactlyOneRequiredHeaderPair(const QStringList &headerNames);
[[nodiscard]] bool isCryptographicallyValid(DKIMCheckSignatureJob::DKIMStatus status);
[[nodiscard]] bool signatureCoversRequiredHeaders(const QStringList &signedHeaders);
}

#endif
