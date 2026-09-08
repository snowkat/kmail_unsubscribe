#include "unsubscribeplugininterface.h"

#include "unsubscribeicons.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <QAction>
#include <QBoxLayout>
#include <QToolButton>

using namespace MessageViewer;

UnsubscribePluginInterface::UnsubscribePluginInterface(QWidget *parent, KActionCollection *actionCollection)
    : ViewerPluginInterface(parent)
    , mAvailability(this)
    , mBatch(this)
{
    mBatch.setParentWidget(parent);
    connect(&mAvailability, &KMailUnsubscribe::UnsubscribeAvailability::changed, this, &UnsubscribePluginInterface::updateActions);
    connect(&mBatch, &KMailUnsubscribe::UnsubscribeBatch::busyChanged, this, &UnsubscribePluginInterface::updateActions);

    mAction = new QAction(KMailUnsubscribe::unsubscribeIcon(), i18n("Unsubscribe…"), this);
    mAction->setIconText(i18n("Unsubscribe"));
    mAction->setIconVisibleInMenu(true);
    mAction->setEnabled(false);
    if (actionCollection)
    {
        QString name = QStringLiteral("oneclick_unsubscribe");
        if (actionCollection->action(name))
        {
            name += QLatin1Char('_') + QString::number(reinterpret_cast<quintptr>(this), 16);
        }
        actionCollection->addAction(name, mAction);
    }
    connect(mAction, &QAction::triggered, this, &UnsubscribePluginInterface::slotActivatePlugin);

    // ViewerPluginToolManager passes the reader box, whose vertical layout
    // contains the HTML view. A native control sits directly above From/To,
    // independent of the selected HTML header theme or message content.
    auto *readerLayout = parent ? qobject_cast<QVBoxLayout *>(parent->layout()) : nullptr;
    if (readerLayout)
    {
        mHeaderBar = new QWidget(parent);
        mHeaderBar->setObjectName(QStringLiteral("unsubscribeHeaderBar"));
        mHeaderBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        auto *layout = new QHBoxLayout(mHeaderBar);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->addStretch();
        auto *button = new QToolButton(mHeaderBar);
        button->setObjectName(QStringLiteral("unsubscribeHeaderButton"));
        button->setAutoRaise(true);
        button->setDefaultAction(mAction);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        layout->addWidget(button);
        readerLayout->insertWidget(0, mHeaderBar);
        mHeaderBar->hide();
    }
}

UnsubscribePluginInterface::~UnsubscribePluginInterface()
{
    delete mHeaderBar.data();
}

QList<QAction *> UnsubscribePluginInterface::actions() const
{
    return {mAction};
}

void UnsubscribePluginInterface::closePlugin()
{
    mItem = Akonadi::Item();
    mCollection = Akonadi::Collection();
    updateActions();
}

void UnsubscribePluginInterface::execute()
{
    Akonadi::Item item = mItem;
    if (!item.parentCollection().isValid())
    {
        item.setParentCollection(mCollection);
    }
    mBatch.start({item});
}

void UnsubscribePluginInterface::setMessageItem(const Akonadi::Item &item)
{
    mItem = item;
    mAvailability.requestItems({item});
    updateActions();
}

void UnsubscribePluginInterface::setCurrentCollection(const Akonadi::Collection &collection)
{
    mCollection = collection;
}

void UnsubscribePluginInterface::updateAction(const Akonadi::Item &item)
{
    setMessageItem(item);
}

void UnsubscribePluginInterface::updateActions()
{
    const auto state = mAvailability.state(mItem.id());
    const bool oneClick = state.web == KMailUnsubscribe::UnsubscribeWorkflow::WebCapability::OneClick;
    mAction->setEnabled(mItem.isValid() && state.available() && !mBatch.busy());
    if (mAction->property("oneClick").toBool() != oneClick)
    {
        mAction->setProperty("oneClick", oneClick);
        mAction->setIcon(KMailUnsubscribe::unsubscribeIcon(oneClick));
    }
    mAction->setToolTip(!state.loaded ? i18n("Checking unsubscribe methods…")
                       : !state.available() ? i18n("No unsubscribe method is available for this message")
                       : oneClick ? i18n("Unsubscribe from this message; verified one-click is available")
                                  : i18n("Unsubscribe from this message"));
    if (mHeaderBar)
    {
        mHeaderBar->setVisible(mItem.isValid());
    }
}

#include "moc_unsubscribeplugininterface.cpp"
