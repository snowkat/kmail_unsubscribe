#include <KLocalizedString>

#include "unsubscribe_debug.h"

#include "unsubscribemanager.h"
#include "oneclickunsubscribejob.h"
#include "rfc8058validator.h"

#include <MessageViewer/DKIMInfo>

using namespace MessageViewer;

namespace
{
QList<QByteArray> rawHeaderValues(const QByteArray &headers, const QByteArray &wantedName)
{
    QList<QByteArray> values;
    QByteArray value;
    bool collecting = false;
    const auto finishHeader = [&]() {
        if (collecting)
        {
            values.append(value);
        }
        value.clear();
        collecting = false;
    };

    for (QByteArray line : headers.split('\n'))
    {
        if (line.endsWith('\r'))
        {
            line.chop(1);
        }
        if (line.isEmpty())
        {
            finishHeader();
            break;
        }
        if (line.startsWith(' ') || line.startsWith('\t'))
        {
            if (collecting)
            {
                // Folding whitespace cannot be part of a RFC 2369 URI.
                value.append(line.trimmed());
            }
            continue;
        }

        finishHeader();
        const qsizetype separator = line.indexOf(':');
        if (separator <= 0)
        {
            continue;
        }
        collecting = line.left(separator).compare(wantedName, Qt::CaseInsensitive) == 0;
        if (collecting)
        {
            value = line.mid(separator + 1).trimmed();
        }
    }
    finishHeader();
    return values;
}

QList<QUrl> rawMailtoUnsubscribeUrls(const std::shared_ptr<KMime::Message> &message)
{
    QList<QUrl> urls;
    if (!message)
    {
        return urls;
    }

    for (const QByteArray &value : rawHeaderValues(message->head(), QByteArrayLiteral("List-Unsubscribe")))
    {
        const QByteArray lowerValue = value.toLower();
        qsizetype offset = 0;
        while (true)
        {
            const qsizetype start = lowerValue.indexOf("<mailto:", offset);
            if (start < 0)
            {
                break;
            }
            const qsizetype end = value.indexOf('>', start + 1);
            if (end < 0)
            {
                break;
            }

            const QByteArray encodedUrl = value.mid(start + 1, end - start - 1);
            const QUrl url = QUrl::fromEncoded(encodedUrl, QUrl::StrictMode);
            if (!encodedUrl.contains('<') && url.isValid()
                && url.scheme().compare(QLatin1StringView("mailto"), Qt::CaseInsensitive) == 0
                && !urls.contains(url))
            {
                urls.append(url);
                offset = end + 1;
            }
            else
            {
                // An unmatched earlier '<' must not conceal a later, complete
                // bracketed mailto URI. Continue inside the rejected fragment.
                offset = start + 1;
            }
        }
    }
    return urls;
}
}

UnsubscribeManager::UnsubscribeManager(QObject *parent)
    : QObject(parent)
{
    mDkimTimeout.setSingleShot(true);
    mDkimTimeout.setInterval(30000);
    connect(&mDkimTimeout, &QTimer::timeout, this, [this]() {
        if (mDkimVerificationPending)
        {
            mDkimVerificationPending = false;
            qCWarning(UnsubscribePlugin) << "Timed out while verifying one-click unsubscribe DKIM";
            Q_EMIT unsubscribeStatusChanged();
        }
    });
}

UnsubscribeManager::~UnsubscribeManager() = default;

void UnsubscribeManager::reset()
{
    mDkimTimeout.stop();
    mDkimMgr.reset();
    mDKIMValid = false;
    mOneClickCandidate = false;
    mDkimVerificationPending = false;
    mItemId = -1;
    mPostUrl = QUrl();
    mMessage = nullptr;
    mList = MessageCore::MailingList();
}

void UnsubscribeManager::setMessageItem(const Akonadi::Item &item, bool verifyOneClick)
{
    // The same item can arrive first as an envelope and later with complete
    // headers/body. Its ID alone does not identify the supplied payload.
    reset();

    // First, we have to have a KMime::Message pointer
    if (item.hasPayload<std::shared_ptr<KMime::Message>>())
    {
        mMessage = item.payload<std::shared_ptr<KMime::Message>>();
        if (mMessage == nullptr)
        {
            // Sometimes we get nullptr even though item.hasPayload() was true...
            qCInfo(UnsubscribePlugin) << "Can't get current email";
            return;
        }
        mItemId = item.id();
        // Check if we even have Unsubscribe info
        mList = MessageCore::MailingList::detect(mMessage);
    }
    else
    {
        qWarning(UnsubscribePlugin) << "Received email doesn't seem to be an email";
    }

    // RFC 8058 requires a valid DKIM signature covering both unsubscribe
    // headers. This verification is required even when KMail's DKIM display is
    // disabled.
    mOneClickCandidate = !oneClickUrl().isEmpty() && hasValidOneClickHeaders();
    if (mOneClickCandidate && verifyOneClick)
    {
        // A stored DKIM result may predate a local message change, so always
        // verify the message that supplies the one-click URI.
        mDkimVerificationPending = true;
        mDkimTimeout.start();
        mDkimMgr = std::make_unique<DKIMManager>(this);
        connect(mDkimMgr.get(), &DKIMManager::result, this, &UnsubscribeManager::getDkimResult);
        mDkimMgr->recheckDKim(item);
    }
}

bool UnsubscribeManager::hasMessage()
{
    return !(mMessage == nullptr);
}

void UnsubscribeManager::getDkimResult(const MessageViewer::DKIMCheckSignatureJob::CheckSignatureResult &checkResult, Akonadi::Item::Id id)
{
    if (id == mItemId)
    {
        mDkimTimeout.stop();
        mDkimVerificationPending = false;
        // CheckSignatureResult::isValid() only reports whether a result was
        // produced; Invalid and EmailNotSigned are also "valid" result objects.
        mDKIMValid = Rfc8058::isCryptographicallyValid(checkResult.status)
            && dkimSignatureCoversOneClickHeaders();
        qCDebug(UnsubscribePlugin) << "Got DKIM result! Valid:" << mDKIMValid;
        Q_EMIT unsubscribeStatusChanged();
    }
    else
    {
        qCDebug(UnsubscribePlugin) << "Got DKIM result for wrong ID! Wanted" << mItemId << "but got" << id;
    }
}

UnsubscribeManager::Status
UnsubscribeManager::unsubscribeStatus()
{
    if (mMessage != nullptr && mList.features().testFlag(MessageCore::MailingList::Unsubscribe))
    {
        if (!oneClickUrl().isEmpty())
        {
            if (hasValidOneClickHeaders())
            {
                if (mDkimVerificationPending)
                {
                    return UnsubscribeManager::CheckingOneClick;
                }
                if (!mDKIMValid)
                {
                    return UnsubscribeManager::InvalidOneClick;
                }
                return UnsubscribeManager::ValidOneClick;
            }
        }

        return UnsubscribeManager::NoOneClick;
    }

    return UnsubscribeManager::None;
}

bool UnsubscribeManager::hasOneClickCandidate()
{
    return mOneClickCandidate;
}

bool UnsubscribeManager::oneClickVerificationPending() const
{
    return mDkimVerificationPending;
}

bool UnsubscribeManager::doOneClick()
{
    auto status = unsubscribeStatus();
    if (status == UnsubscribeManager::ValidOneClick)
    {
        auto job = new OneClickUnsubscribeJob(mPostUrl, this);
        connect(job, &OneClickUnsubscribeJob::result, this, &UnsubscribeManager::checkResult);
        job->start();
        return true;
    }
    return false;
}

QUrl UnsubscribeManager::oneClickUrl()
{
    if (mPostUrl.isEmpty() && mList.features().testFlag(MessageCore::MailingList::Unsubscribe))
    {
        mPostUrl = Rfc8058::oneClickPostUrl(mList.unsubscribeUrls());
    }

    return mPostUrl;
}

QList<QUrl> UnsubscribeManager::emailUrls() const
{
    QList<QUrl> urls;
    for (const QUrl &url : mList.unsubscribeUrls())
    {
        if (url.scheme().compare(QLatin1StringView("mailto"), Qt::CaseInsensitive) == 0 && !urls.contains(url))
        {
            urls.append(url);
        }
    }

    // MessageCore stops parsing the complete field when another advertised URI
    // has an unmatched angle bracket. Recover only explicitly bracketed mailto
    // values from this one header; never derive an address from message sender
    // or reply fields.
    for (const QUrl &url : rawMailtoUnsubscribeUrls(mMessage))
    {
        if (!urls.contains(url))
        {
            urls.append(url);
        }
    }
    return urls;
}

QUrl UnsubscribeManager::emailUrl() const
{
    return emailUrls().value(0);
}

QUrl UnsubscribeManager::webUrl()
{
    if (!mPostUrl.isEmpty())
    {
        return mPostUrl;
    }

    const QStringList protocols = {QStringLiteral("https"), QStringLiteral("http")};
    for (const QString &scheme : protocols)
    {
        for (const QUrl &url : mList.unsubscribeUrls())
        {
            if (url.scheme() == scheme)
            {
                return url;
            }
        }
    }

    return QUrl();
}

bool UnsubscribeManager::hasValidOneClickHeaders() const
{
    if (!mMessage)
    {
        return false;
    }

    QStringList headerNames;
    for (const auto *const header : mMessage->headers())
    {
        headerNames.append(QString::fromLatin1(header->type()));
    }
    if (!Rfc8058::hasExactlyOneRequiredHeaderPair(headerNames))
    {
        return false;
    }

    const auto *const header = mMessage->headerByType(LIST_UNSUBSCRIBE_POST_HDR);
    if (!header)
    {
        return false;
    }

    const QString value = header->asUnicodeString();
    return Rfc8058::isValidPostHeaderValue(value);
}

bool UnsubscribeManager::dkimSignatureCoversOneClickHeaders() const
{
    if (!mMessage)
    {
        return false;
    }

    const auto *const header = mMessage->headerByType("DKIM-Signature");
    if (!header)
    {
        return false;
    }

    DKIMInfo info;
    return info.parseDKIM(header->asUnicodeString())
        && Rfc8058::signatureCoversRequiredHeaders(info.listSignedHeader());
}

void UnsubscribeManager::checkResult(const Result &result)
{
    bool isSuccess = false;
    QString resultStr;
    switch (result.Type)
    {
    case Result::None:
        isSuccess = true;
        break;
    case Result::NetworkError:
        resultStr = i18n("The unsubscribe server %1 could not be reached. Unsubscribing was not confirmed.",
                         result.ServerHost);
        break;
    case Result::SslError:
        resultStr = i18n("A secure connection to the unsubscribe server %1 could not be established. "
                         "Unsubscribing was not confirmed.", result.ServerHost);
        break;
    case Result::HttpError:
        if (result.HttpStatus == 404)
        {
            resultStr = i18n("The unsubscribe server %1 reported that this address was not found (HTTP 404). "
                             "The link may be outdated. Unsubscribing was not confirmed.", result.ServerHost);
        }
        else if (result.HttpStatus == 410)
        {
            resultStr = i18n("The unsubscribe server %1 reported that this address is no longer available (HTTP 410). "
                             "Unsubscribing was not confirmed.", result.ServerHost);
        }
        else
        {
            resultStr = i18n("The unsubscribe server %1 returned HTTP %2. Unsubscribing was not confirmed.",
                             result.ServerHost, result.HttpStatus);
        }
        break;
    default:
        resultStr = i18n("Plugin hit an unreachable point");
    }

    Q_EMIT oneClickResult(isSuccess, resultStr);
}

#include "moc_unsubscribemanager.cpp"
