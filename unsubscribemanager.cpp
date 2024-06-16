#include <KLocalizedString>

#include "unsubscribe_debug.h"

#include "unsubscribemanager.h"
#include "oneclickunsubscribejob.h"

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

    // First, we have to have a KMime::Message::Ptr
    if (item.hasPayload<KMime::Message::Ptr>())
    {
        mMessage = item.payload<KMime::Message::Ptr>();
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

    // Don't bother checking DKIM if we don't have an unsubscribe URL, or if we
    // don't have DKIM enabled
    if (!(getUrl().isEmpty()) && MessageViewerSettings::self()->enabledDkim())
    {
        mDkimMgr.checkDKim(item);
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
        mDKIMValid = checkResult.isValid();
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
            // One-Click requires List-Unsubscribe-Post
            if (mMessage->hasHeader(LIST_UNSUBSCRIBE_POST_HDR))
            {
                if (MessageViewerSettings::self()->enabledDkim() &&
                    !mDKIMValid)
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
    if (status == UnsubscribeManager::InvalidOneClick || status == UnsubscribeManager::ValidOneClick)
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
        foreach (QUrl url, mList.unsubscribeUrls())
        {
            // Per the RFC, there should only be one HTTPS URL. So grab the
            // first one we find
            if (url.scheme() == QStringLiteral("https"))
            {
                mPostUrl = url;
                break;
            }
        }
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