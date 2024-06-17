#include "unsubscribe_debug.h"
#include "unsubscribeplugininterface.h"
#include <QMessageBox>
#include <KActionCollection>
#include <KLocalizedString>
#include <MessageCore/MailingList>
#include <PimCommon/NetworkManager>
#include <KIO/OpenUrlJob>
#include <KIO/JobUiDelegate>
#include <KIO/JobUiDelegateFactory>

using namespace MessageViewer;

// General yes/no confirm dialog
static bool
confirmDialog(const QString &text, const QString &question, bool safe)
{
    QMessageBox msgBox;
    msgBox.setText(text);
    msgBox.setInformativeText(question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setIcon(safe ? QMessageBox::Question : QMessageBox::Warning);
    msgBox.setDefaultButton(safe ? QMessageBox::Yes : QMessageBox::No);

    return msgBox.exec() == QMessageBox::Yes;
}

UnsubscribePluginInterface::UnsubscribePluginInterface(QWidget *parent, KActionCollection *ac)
    : ViewerPluginInterface(parent),
      mParent(parent)
{
    if (ac)
    {
        // Create the action...
        auto action = new QAction(this);
        action->setIcon(QIcon::fromTheme(QStringLiteral("news-unsubscribe")));
        action->setIconText(i18n("Unsubscribe"));
        action->setWhatsThis(i18n("Allows you to unsubscribe from a mailing list, if the sender supports One-Click Unsubscribe"));

        // ... and add it to the application's collection
        ac->addAction(QStringLiteral("oneclick_unsubscribe"), action);

        connect(action, &QAction::triggered, this, &UnsubscribePluginInterface::slotActivatePlugin);
        // also add it to our list, for actions()
        mActions.append(action);
    }

    connect(&mUnsub, &UnsubscribeManager::oneClickResult, this, &UnsubscribePluginInterface::getOneClickResult);
}

UnsubscribePluginInterface::~UnsubscribePluginInterface() = default;

QList<QAction *> UnsubscribePluginInterface::actions() const
{
    return mActions;
}

void UnsubscribePluginInterface::closePlugin()
{
    // Reset the UnsubscribeManager's state.
    // XXX: Currently, this doesn't cancel any running One-Click Unsubscribe
    //      calls!
    mUnsub.reset();
}

/// @brief Called when the action is clicked or otherwise activated
void UnsubscribePluginInterface::execute()
{
    qCDebug(UnsubscribePlugin) << "-click!-";
    // short-circuit if we're offline
    if (!PimCommon::NetworkManager::self()->isOnline())
    {
        QMessageBox::critical(mParent,
                              i18n("Network not available"),
                              i18n("Please go back online to unsubscribe from this list."));
        return;
    }

    if (mUnsub.hasMessage())
    {
        switch (mUnsub.unsubscribeStatus())
        {
        case UnsubscribeManager::None:
        {
            // Should not happen
            QMessageBox::warning(mParent,
                                 i18n("Can't Unsubscribe"),
                                 i18n("This email doesn't advertise a way to unsubscribe."));
            break;
        }
        break;
        case UnsubscribeManager::NoOneClick:
        {
            // Load the unsubscribe URL normally
            QUrl url = mUnsub.getUrl();
            if (!url.isEmpty())
            {
                auto job = new KIO::OpenUrlJob(url);
                job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, mParent));
                job->start();
            }
            break;
        }
        break;
        case UnsubscribeManager::InvalidOneClick:
        {
            if (confirmDialog(
                    i18n("The digital signature of this email couldn't be validated."),
                    i18n("Do you still want to unsubscribe?"),
                    false))
            {
                mUnsub.doOneClick();
            }
            break;
        }
        case UnsubscribeManager::ValidOneClick:
        {
            if (confirmDialog(
                    i18n("This mailing list supports One-Click Unsubscribe."),
                    i18n("Do you want to unsubscribe?"),
                    true))
            {
                mUnsub.doOneClick();
            }
            break;
        }
        }
    }
}

void UnsubscribePluginInterface::setMessageItem(const Akonadi::Item &item)
{
    mUnsub.setMessageItem(item);
}

/// @brief Triggered when the selected item changes.
/// @param item The new item.
void UnsubscribePluginInterface::updateAction(const Akonadi::Item &item)
{
    mUnsub.setMessageItem(item);
    auto status = mUnsub.unsubscribeStatus();
    QAction *action = mActions.first();
    QString caption;

    switch (mUnsub.unsubscribeStatus())
    {
    case UnsubscribeManager::NoOneClick:
    {
        QString scheme = mUnsub.getUrl().scheme();
        if (scheme.startsWith("http"))
        {
            caption = i18nc("unsubscribe via the web", "Web");
        }
        else if (scheme.startsWith("mailto"))
        {
            caption = i18nc("unsubscribe via email", "Email");
        }
        else
        {
            // Unknown, just set to
            caption = scheme;
        }
    }
    break;
    case UnsubscribeManager::ValidOneClick:
    case UnsubscribeManager::InvalidOneClick:
        caption = i18nc("using RFC 8058 unsubscribe", "One-Click");
        break;
    case UnsubscribeManager::None:
    default:
        // Do nothing
        break;
    }

    // Assuming we have an action, set the action up
    if (action)
    {
        action->setDisabled(status == UnsubscribeManager::None);
        if (caption.isEmpty())
        {
            action->setIconText(i18n("Unsubscribe"));
        }
        else
        {
            action->setIconText(i18nc("unsubscribe via %1", "Unsubscribe (%1)", caption));
        }
    }
}

void UnsubscribePluginInterface::getOneClickResult(bool isSuccess, const QString &resultString)
{
    if (isSuccess)
    {
        QMessageBox::information(mParent,
                                 i18n("Request Complete"),
                                 i18n("The unsubscribe request was successfully sent."));
    }
    else
    {
        QMessageBox::critical(mParent,
                              i18n("Unsubscribe Error"),
                              resultString);
    }
}

#include "moc_unsubscribeplugininterface.cpp"