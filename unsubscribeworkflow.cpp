#include "unsubscribeworkflow.h"

#include "unsubscribecomposerrequest.h"
#include "unsubscribecomposerrequeststore.h"
#include "unsubscribe_debug.h"
#include "webunsubscribeprobe.h"

#include <KIdentityManagementCore/Identity>
#include <KIdentityManagementCore/IdentityManager>
#include <KEmailAddress>
#include <KIO/JobUiDelegate>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <MailCommon/MailUtil>
#include <MessageCore/StringUtil>
#include <MessageCore/Util>
#include <PimCommon/NetworkManager>
#include <QProcess>

using namespace KMailUnsubscribe;

namespace
{
struct MailtoFields
{
    QString to;
    QString cc;
    QString bcc;
    QString subject;
    QString body;
};

MailtoFields parseMailtoFields(const QUrl &url)
{
    MailtoFields result;
    const auto fields = MessageCore::StringUtil::parseMailtoUrl(url);
    for (const auto &field : fields)
    {
        if (field.first == QLatin1StringView("to"))
        {
            result.to = field.second;
        }
        else if (field.first == QLatin1StringView("cc"))
        {
            result.cc = field.second;
        }
        else if (field.first == QLatin1StringView("bcc"))
        {
            result.bcc = field.second;
        }
        else if (field.first == QLatin1StringView("subject"))
        {
            result.subject = field.second;
        }
        else if (field.first == QLatin1StringView("body"))
        {
            result.body = field.second;
        }
    }
    return result;
}

bool hasValidMailtoRecipient(const QUrl &url)
{
    const MailtoFields fields = parseMailtoFields(url);
    return !fields.to.trimmed().isEmpty()
        && KEmailAddress::isValidAddress(fields.to.trimmed()) == KEmailAddress::AddressOk;
}

QString identityNameForMessage(Akonadi::Item item,
                               const Akonadi::Collection &currentCollection,
                               const std::shared_ptr<KMime::Message> &message)
{
    if (!item.parentCollection().isValid() && currentCollection.isValid())
    {
        item.setParentCollection(currentCollection);
    }

    auto *const identityManager = KIdentityManagementCore::IdentityManager::self();
    const uint folderIdentityId = MailCommon::Util::folderIdentity(item);
    const auto &folderIdentity = identityManager->identityForUoid(folderIdentityId);
    if (!folderIdentity.isNull())
    {
        return folderIdentity.identityName();
    }

    return MessageCore::Util::identityForMessage(message.get(), identityManager, folderIdentityId).identityName();
}

bool openUnsubscribeEmail(const QUrl &url,
                          const Akonadi::Item &item,
                          const Akonadi::Collection &currentCollection,
                          bool deleteAfterSuccess,
                          QString &error)
{
    if (url.isEmpty() || !hasValidMailtoRecipient(url) || !item.hasPayload<std::shared_ptr<KMime::Message>>())
    {
        return false;
    }

    const auto message = item.payload<std::shared_ptr<KMime::Message>>();
    if (!message)
    {
        return false;
    }

    const MailtoFields fields = parseMailtoFields(url);
    const QString identityName = identityNameForMessage(item, currentCollection, message);

    QStringList arguments = {QStringLiteral("--composer")};
    if (!identityName.isEmpty())
    {
        arguments << QStringLiteral("--identity") << identityName;
    }
    if (!fields.cc.isEmpty())
    {
        arguments << QStringLiteral("--cc") << fields.cc;
    }
    if (!fields.bcc.isEmpty())
    {
        arguments << QStringLiteral("--bcc") << fields.bcc;
    }
    if (!fields.subject.isEmpty())
    {
        arguments << QStringLiteral("--subject") << fields.subject;
    }

    // KMail treats a non-empty body as an instruction to skip the new-message
    // template. The editor-init plugin replaces this marker after KMail has
    // finished its deferred signature insertion.
    const QString marker = makeComposerRequest(fields.body);
    const QString token = composerRequestFromText(marker)->token;
    ComposerRequestStore store;
    if (deleteAfterSuccess)
    {
        const auto *subject = message->subject(KMime::CreatePolicy::DontCreate);
        if (!store.remember(token, {item.id(), subject ? subject->asUnicodeString() : i18n("(No subject)")}))
        {
            error = i18n("KMail could not prepare deletion after sending the unsubscribe email. The original message was kept.");
            return false;
        }
    }
    arguments << QStringLiteral("--body") << marker;
    if (!fields.to.isEmpty())
    {
        arguments << fields.to;
    }

    QProcess process;
    process.setProgram(QStringLiteral("kmail"));
    process.setArguments(arguments);
    const bool opened = process.startDetached();
    if (!opened)
    {
        store.discard(token);
    }
    return opened;
}
}

UnsubscribeWorkflow::UnsubscribeWorkflow(QObject *parent)
    : QObject(parent)
{
    connect(&mUnsubscribeManager,
            &MessageViewer::UnsubscribeManager::oneClickResult,
            this,
            &UnsubscribeWorkflow::finished);
    connect(&mUnsubscribeManager,
            &MessageViewer::UnsubscribeManager::unsubscribeStatusChanged,
            this,
            &UnsubscribeWorkflow::stateChanged);
}

void UnsubscribeWorkflow::setParentWidget(QWidget *parent)
{
    mParent = parent;
}

void UnsubscribeWorkflow::setMessageItem(const Akonadi::Item &item, bool verifyOneClick)
{
    mMessageItem = item;
    mUnsubscribeManager.setMessageItem(item, verifyOneClick);
}

void UnsubscribeWorkflow::setCurrentCollection(const Akonadi::Collection &collection)
{
    mCurrentCollection = collection;
}

void UnsubscribeWorkflow::setDeleteAfterSuccess(bool enabled)
{
    mDeleteAfterSuccess = enabled;
}

void UnsubscribeWorkflow::reset()
{
    mUnsubscribeManager.reset();
    mMessageItem = Akonadi::Item();
    mCurrentCollection = Akonadi::Collection();
    mDeleteAfterSuccess = false;
}

bool UnsubscribeWorkflow::emailAdvertised()
{
    return mUnsubscribeManager.hasMessage() && !mUnsubscribeManager.emailUrls().isEmpty();
}

QUrl UnsubscribeWorkflow::validEmailUrl() const
{
    for (const QUrl &url : mUnsubscribeManager.emailUrls())
    {
        if (hasValidMailtoRecipient(url))
        {
            return url;
        }
    }
    return {};
}

bool UnsubscribeWorkflow::emailAvailable()
{
    return !validEmailUrl().isEmpty();
}

QString UnsubscribeWorkflow::emailValidationError()
{
    if (emailAdvertised() && !emailAvailable())
    {
        return i18n("The advertised unsubscribe email address is invalid, so it was not opened.");
    }
    return {};
}

bool UnsubscribeWorkflow::webAvailable()
{
    return mUnsubscribeManager.hasMessage() && !mUnsubscribeManager.webUrl().isEmpty();
}

bool UnsubscribeWorkflow::anyMethodAvailable()
{
    return emailAvailable() || webAvailable();
}

bool UnsubscribeWorkflow::hasOneClickCandidate()
{
    return mUnsubscribeManager.hasOneClickCandidate();
}

UnsubscribeWorkflow::WebCapability UnsubscribeWorkflow::webCapability()
{
    if (!webAvailable())
    {
        return WebCapability::None;
    }

    switch (mUnsubscribeManager.unsubscribeStatus())
    {
    case MessageViewer::UnsubscribeManager::CheckingOneClick:
        return WebCapability::VerifyingOneClick;
    case MessageViewer::UnsubscribeManager::ValidOneClick:
        return WebCapability::OneClick;
    case MessageViewer::UnsubscribeManager::None:
    case MessageViewer::UnsubscribeManager::NoOneClick:
    case MessageViewer::UnsubscribeManager::InvalidOneClick:
        return WebCapability::Regular;
    }

    return WebCapability::Regular;
}

std::optional<UnsubscribeWorkflow::Method> UnsubscribeWorkflow::resolvedMethod(Method method)
{
    switch (method)
    {
    case Method::OneClick:
        return webCapability() == WebCapability::OneClick ? std::optional(Method::OneClick) : std::nullopt;
    case Method::Email:
        return emailAvailable() ? std::optional(Method::Email) : std::nullopt;
    case Method::Web:
        return webAvailable() ? std::optional(Method::Web) : std::nullopt;
    }
    return std::nullopt;
}

void UnsubscribeWorkflow::execute(Method method)
{
    // Execute exactly the method shown in the confirmed plan. If it is no
    // longer available, fail instead of silently choosing a different method.
    const auto resolved = resolvedMethod(method);
    if (!resolved)
    {
        Q_EMIT finished(false, i18n("The confirmed unsubscribe method is no longer available."));
        return;
    }

    if (*resolved == Method::Email)
    {
        QString error;
        const bool opened = openUnsubscribeEmail(validEmailUrl(), mMessageItem, mCurrentCollection, mDeleteAfterSuccess, error);
        Q_EMIT finished(opened, opened ? QString()
                                      : error.isEmpty() ? i18n("KMail could not start the unsubscribe email composer.") : error);
        return;
    }

    if (!PimCommon::NetworkManager::self()->isOnline())
    {
        Q_EMIT finished(false, i18n("Please go back online to unsubscribe from this list."));
        return;
    }

    if (*resolved == Method::OneClick)
    {
        if (!mUnsubscribeManager.doOneClick())
        {
            Q_EMIT finished(false, i18n("The one-click unsubscribe request could not be started."));
        }
        return;
    }

    const QUrl webUrl = mUnsubscribeManager.webUrl();
    const auto openInBrowser = [this, webUrl]() {
        auto *job = new KIO::OpenUrlJob(webUrl);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::Flags{}, mParent.data()));
        connect(job, &KJob::result, this, [this, job]() {
            const bool opened = !job->error();
            // The browser owns the URL after this point. Its eventual page
            // status cannot be observed here, and errorString() can expose
            // URL tokens.
            Q_EMIT finished(opened, opened ? QString() : i18n("KMail could not open the web unsubscribe page."));
        });
        job->start();
    };

    // A probe is needed only when an email address can provide a useful
    // fallback. Preserve direct browser behavior for web-only messages.
    if (!emailAvailable())
    {
        openInBrowser();
        return;
    }

    auto *probe = new WebUnsubscribeProbe(webUrl, this);
    connect(probe, &WebUnsubscribeProbe::finished, this, [this, openInBrowser](bool shouldOpenInBrowser, int status, bool jsonResponse) {
        if (!shouldOpenInBrowser)
        {
            const QString error = status == 404
                ? i18n("The web unsubscribe page was not found (HTTP 404).")
                : status == 410
                    ? i18n("The web unsubscribe page is no longer available (HTTP 410).")
                    : jsonResponse
                        ? i18n("The web unsubscribe address returned a JSON response instead of an interactive page.")
                        : i18n("KMail could not open the web unsubscribe page.");
            Q_EMIT finished(false, error);
            return;
        }
        openInBrowser();
    });
    probe->start();
}
