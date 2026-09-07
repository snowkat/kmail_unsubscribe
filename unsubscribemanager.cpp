#include <KLocalizedString>

#include "unsubscribe_debug.h"

#include "unsubscribemanager.h"
#include "oneclickunsubscribejob.h"
#include "rfc8058validator.h"

#include <MessageViewer/DKIMInfo>

using namespace MessageViewer;

UnsubscribeManager::UnsubscribeManager(QObject *parent)
    : QObject(parent),
      mDkimMgr(DKIMManager(this))
{
    connect(&mDkimMgr, &DKIMManager::result, this, &UnsubscribeManager::getDkimResult);
}

UnsubscribeManager::~UnsubscribeManager() = default;

void UnsubscribeManager::reset()
{
    mDKIMValid = false;
    mItemId = -1;
    mPostUrl = QUrl();
    mMessage = nullptr;
}

void UnsubscribeManager::setMessageItem(const Akonadi::Item &item)
{
    if (item.id() == mItemId)
    {
        // We already have this item, do nothing
        return;
    }
    else if (mItemId != -1)
    {
        // reset wasn't called first, so call it for them
        this->reset();
    }

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
    if (!oneClickUrl().isEmpty() && hasValidOneClickHeaders())
    {
        // A stored DKIM result may predate a local message change, so always
        // verify the message that supplies the one-click URI.
        mDkimMgr.recheckDKim(item);
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
        // CheckSignatureResult::isValid() only reports whether a result was
        // produced; Invalid and EmailNotSigned are also "valid" result objects.
        mDKIMValid = Rfc8058::isCryptographicallyValid(checkResult.status)
            && dkimSignatureCoversOneClickHeaders();
        qCDebug(UnsubscribePlugin) << "Got DKIM result! Valid:" << mDKIMValid;
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

void UnsubscribeManager::doOneClick()
{
    auto status = unsubscribeStatus();
    if (status == UnsubscribeManager::ValidOneClick)
    {
        auto job = new OneClickUnsubscribeJob(mPostUrl, this);
        connect(job, &OneClickUnsubscribeJob::result, this, &UnsubscribeManager::checkResult);
        job->start();
    }
}

QUrl UnsubscribeManager::oneClickUrl()
{
    if (mPostUrl.isEmpty() && mList.features().testFlag(MessageCore::MailingList::Unsubscribe))
    {
        mPostUrl = Rfc8058::oneClickPostUrl(mList.unsubscribeUrls());
    }

    return mPostUrl;
}

QUrl UnsubscribeManager::getUrl()
{
    // TODO: Maybe this should be customizable.
    // The theory is that HTTPS is most secure, mailto is more likely to be
    // secure (MTAs often use TLS these days), and http as last resort. I really
    // hope nobody's requesting unsubscribe over IRC...
    const QStringList protocols = {"https", "mailto", "http"};
    if (!mPostUrl.isEmpty())
    {
        return mPostUrl;
    }
    else
    {
        foreach (QString scheme, protocols)
        {
            foreach (QUrl url, mList.unsubscribeUrls())
            {
                if (url.scheme() == scheme)
                {
                    return url;
                }
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
        resultStr = i18n("Unable to send unsubscribe request: %1", result.ErrorString);
        break;
    case Result::SslError:
        resultStr = i18n("Got one or more SSL errors: %1", result.ErrorString);
        break;
    default:
        resultStr = i18n("Plugin hit an unreachable point");
    }

    Q_EMIT oneClickResult(isSuccess, resultStr);
}

#include "moc_unsubscribemanager.cpp"
