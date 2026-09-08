#include "unsubscribeactionplugin.h"

#include "unsubscribe_debug.h"
#include "unsubscribeicons.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KLocalizedString>
#include <KPluginFactory>
#include <MessageList/Pane>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QTimer>
#include <QTreeView>
#include <QUrl>

namespace
{
void hideDuplicateUnsubscribeActions(QAction *mailingListAction)
{
    auto *mailingList = qobject_cast<KActionMenu *>(mailingListAction);
    if (!mailingList)
    {
        return;
    }

    bool hasOtherCommands = false;
    for (auto *action : mailingList->menu()->actions())
    {
        // KMail gives these generated actions no action ID. Match its exact
        // localized caption and URL data, only inside the native mailing-list
        // menu. Other commands can share the same URL and must remain intact.
        const QUrl url = action->data().toUrl();
        QString protocol = url.scheme().toLower();
        if (protocol == QLatin1StringView("mailto"))
        {
            protocol = i18nd("kmail", "email");
        }
        else if (protocol.startsWith(QLatin1StringView("http")))
        {
            protocol = i18nd("kmail", "web");
        }
        const QString caption = i18ndc("kmail",
            "%1 is a 'Contact Owner' or similar action. %2 is a protocol normally web or email though could be irc/ftp or other url variant",
            "%1 (%2)", i18nd("kmail", "Unsubscribe from List"), protocol);
        if (!url.isEmpty() && action->text() == caption)
        {
            action->setVisible(false);
        }
        else if (!action->isSeparator() && action->isVisible())
        {
            hasOtherCommands = true;
        }
    }
    mailingListAction->setVisible(hasOtherCommands);
}
}

K_PLUGIN_CLASS_WITH_JSON(UnsubscribeActionPlugin, "kmail_unsubscribe_actionplugin.json")

UnsubscribeActionPlugin::UnsubscribeActionPlugin(QObject *parent, const QList<QVariant> &)
    : PimCommon::GenericPlugin(parent)
{
}

PimCommon::GenericPluginInterface *UnsubscribeActionPlugin::createInterface(QObject *parent)
{
    return new UnsubscribeActionPluginInterface(parent);
}

UnsubscribeActionPluginInterface::UnsubscribeActionPluginInterface(QObject *parent)
    : PimCommon::GenericPluginInterface(parent)
    , mAvailability(this)
    , mBatch(this)
{
    connect(&mAvailability, &KMailUnsubscribe::UnsubscribeAvailability::changed,
            this, &UnsubscribeActionPluginInterface::updateAggregateAction);
    connect(&mBatch, &KMailUnsubscribe::UnsubscribeBatch::busyChanged,
            this, &UnsubscribeActionPluginInterface::updateAggregateAction);
}

void UnsubscribeActionPluginInterface::createAction(KActionCollection *actionCollection)
{
    mActionCollection = actionCollection;
    mBatch.setParentWidget(parentWidget());
    mAction = new QAction(KMailUnsubscribe::unsubscribeIcon(), i18n("Unsubscribe…"), this);
    mAction->setIconText(i18n("Unsubscribe"));
    mAction->setPriority(QAction::LowPriority);
    mAction->setIconVisibleInMenu(true);
    mAction->setEnabled(false);
    // Keep the existing toolbar action ID for saved toolbar configurations.
    actionCollection->addAction(QStringLiteral("kmail_unsubscribe_toolbar"), mAction);
    addActionType(PimCommon::ActionType(mAction, PimCommon::ActionType::ToolBar));
    connect(mAction, &QAction::triggered, this, [this]() {
        if (mMessagePane)
        {
            mBatch.start(mMessagePane->selectionAsMessageItemList());
        }
        else
        {
            mActivationRequested = true;
            Q_EMIT emitPluginActivated(this);
        }
    });
    mMenuSeparator = new QAction(this);
    mMenuSeparator->setSeparator(true);
    qApp->installEventFilter(this);
    connectMessagePane();
    scheduleSelectionRefresh();
}

bool UnsubscribeActionPluginInterface::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show && mActionCollection)
    {
        auto *menu = qobject_cast<QMenu *>(watched);
        auto *mailingList = mActionCollection->action(QStringLiteral("mailing_list"));
        // This is the list's XMLGUI menu, separate from the preview popup.
        if (menu && menu->objectName() == QLatin1StringView("akonadi_messagelist_contextmenu")
            && mailingList && menu->actions().contains(mailingList))
        {
            hideDuplicateUnsubscribeActions(mailingList);
            if (!menu->actions().contains(mAction))
            {
                menu->insertAction(mailingList, mAction);
                menu->insertAction(mailingList, mMenuSeparator);
            }
            mAction->setIconVisibleInMenu(true);
            if (mMessagePane)
            {
                refreshSelection(mMessagePane->selectionAsMessageItemList());
            }
        }
    }
    return PimCommon::GenericPluginInterface::eventFilter(watched, event);
}

void UnsubscribeActionPluginInterface::exec()
{
    if (mActivationRequested)
    {
        mActivationRequested = false;
        mBatch.start(mItems);
    }
    else
    {
        refreshSelection(mItems);
    }
}

void UnsubscribeActionPluginInterface::setItems(const Akonadi::Item::List &items)
{
    mItems = items;
}

PimCommon::GenericPluginInterface::RequireTypes UnsubscribeActionPluginInterface::requiresFeatures() const
{
    return CurrentItems;
}

void UnsubscribeActionPluginInterface::connectMessagePane()
{
    if (mMessagePane || !parentWidget())
    {
        return;
    }
    mMessagePane = parentWidget()->findChild<MessageList::Pane *>();
    if (!mMessagePane)
    {
        return;
    }
    connect(mMessagePane, &MessageList::Pane::selectionChanged, this, &UnsubscribeActionPluginInterface::scheduleSelectionRefresh);
    connect(mMessagePane, &MessageList::Pane::messageExpanded, this, &UnsubscribeActionPluginInterface::scheduleSelectionRefresh);
    connect(mMessagePane, &MessageList::Pane::messageCollapsed, this, &UnsubscribeActionPluginInterface::scheduleSelectionRefresh);
    connect(mMessagePane, &QTabWidget::currentChanged, this, &UnsubscribeActionPluginInterface::scheduleSelectionRefresh);
    qCInfo(UnsubscribePlugin) << "Unsubscribe attached to the message list (single-button build)";
    attachRowActions();
}

void UnsubscribeActionPluginInterface::attachRowActions()
{
    if (!mMessagePane)
    {
        return;
    }
    mRowActions.removeIf([](const auto &rows) { return !rows; });
    for (auto *view : mMessagePane->findChildren<QTreeView *>())
    {
        if (!view->inherits("MessageList::Core::View") || view->property("unsubscribeRowActionsAttached").toBool())
        {
            continue;
        }
        view->setProperty("unsubscribeRowActionsAttached", true);
        auto *rows = new KMailUnsubscribe::UnsubscribeRowActions(view, &mAvailability, this);
        connect(rows, &KMailUnsubscribe::UnsubscribeRowActions::unsubscribeRequested, this, [this](const Akonadi::Item &item) {
            mBatch.start({item});
        });
        mRowActions.append(rows);
    }
}

void UnsubscribeActionPluginInterface::scheduleSelectionRefresh()
{
    if (mRefreshScheduled)
    {
        return;
    }
    mRefreshScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        mRefreshScheduled = false;
        connectMessagePane();
        if (mMessagePane)
        {
            attachRowActions();
            refreshSelection(mMessagePane->selectionAsMessageItemList());
        }
        else
        {
            // Hosts that expose updateActions() supply items on activation.
            Q_EMIT emitPluginActivated(this);
        }
    });
}

void UnsubscribeActionPluginInterface::refreshSelection(const Akonadi::Item::List &items)
{
    mItems = items;
    mAvailability.requestItems(items);
    updateAggregateAction();
}

void UnsubscribeActionPluginInterface::updateAggregateAction()
{
    bool available = false;
    bool oneClick = false;
    for (const auto &item : std::as_const(mItems))
    {
        const auto state = mAvailability.state(item.id());
        available = available || state.available();
        oneClick = oneClick || state.web == KMailUnsubscribe::UnsubscribeWorkflow::WebCapability::OneClick;
    }
    for (const auto &rows : std::as_const(mRowActions))
    {
        if (rows)
        {
            rows->setBusy(mBatch.busy());
        }
    }
    if (mAction)
    {
        mAction->setEnabled(available && !mBatch.busy());
        if (mAction->property("oneClick").toBool() != oneClick)
        {
            mAction->setProperty("oneClick", oneClick);
            mAction->setIcon(KMailUnsubscribe::unsubscribeIcon(oneClick));
        }
        mAction->setToolTip(oneClick ? i18n("Unsubscribe from the selected messages; one-click is available for some or all")
                                    : i18n("Unsubscribe from the selected messages"));
    }
}

void UnsubscribeActionPluginInterface::updateActions(int numberOfSelectedItems, int numberOfSelectedCollections)
{
    Q_UNUSED(numberOfSelectedItems)
    Q_UNUSED(numberOfSelectedCollections)
    scheduleSelectionRefresh();
}

#include "unsubscribeactionplugin.moc"
#include "moc_unsubscribeactionplugin.cpp"
