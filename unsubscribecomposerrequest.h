#pragma once

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <optional>

namespace KMailUnsubscribe
{
struct ComposerRequest
{
    QString token;
    QString body;
};

inline QString composerRequestPrefix()
{
    return QStringLiteral("__KMAIL_UNSUBSCRIBE_REQUEST_V1__");
}

inline QString composerRequestSuffix()
{
    return QStringLiteral("__END_KMAIL_UNSUBSCRIBE_REQUEST__");
}

inline QString makeComposerRequest(const QString &body)
{
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QByteArray encodedBody = body.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return composerRequestPrefix() + token + QLatin1Char(':') + QString::fromLatin1(encodedBody) + composerRequestSuffix();
}

inline std::optional<ComposerRequest> composerRequestFromText(const QString &text)
{
    const QString prefix = composerRequestPrefix();
    const qsizetype markerStart = text.indexOf(prefix);
    if (markerStart < 0)
    {
        return std::nullopt;
    }

    const qsizetype contentStart = markerStart + prefix.size();
    const QString suffix = composerRequestSuffix();
    const qsizetype markerEnd = text.indexOf(suffix, contentStart);
    if (markerEnd < 0)
    {
        return std::nullopt;
    }

    const QString content = text.mid(contentStart, markerEnd - contentStart);
    const qsizetype separator = content.indexOf(QLatin1Char(':'));
    if (separator < 0 || QUuid(content.left(separator)).isNull())
    {
        return std::nullopt;
    }

    const QByteArray encodedBody = content.mid(separator + 1).toLatin1();
    const auto decodedBody = QByteArray::fromBase64Encoding(
        encodedBody,
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decodedBody)
    {
        return std::nullopt;
    }

    return ComposerRequest{QUuid(content.left(separator)).toString(QUuid::WithoutBraces),
                           QString::fromUtf8(decodedBody.decoded)};
}

inline std::optional<QString> bodyFromComposerRequest(const QString &text)
{
    const auto request = composerRequestFromText(text);
    return request ? std::optional(request->body) : std::nullopt;
}
}
