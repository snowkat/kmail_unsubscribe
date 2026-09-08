#pragma once

#include "unsubscribeavailability.h"
#include "unsubscribebatch.h"
#include "unsubscriberowactions.h"

#include <PimCommon/GenericPlugin>
#include <PimCommonAkonadi/GenericPluginInterface>
#include <QPointer>

class QAction;
namespace MessageList { class Pane; }

class UnsubscribeActionPlugin : public PimCommon::GenericPlugin
{
    Q_OBJECT
public:
    explicit UnsubscribeActionPlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());
    [[nodiscard]] PimCommon::GenericPluginInterface *createInterface(QObject *parent = nullptr) override;
};

class UnsubscribeActionPluginInterface : public PimCommon::GenericPluginInterface
{
    Q_OBJECT
public:
    explicit UnsubscribeActionPluginInterface(QObject *parent = nullptr);
    void createAction(KActionCollection *actionCollection) override;
    void exec() override;
    void setItems(const Akonadi::Item::List &items) override;
    [[nodiscard]] RequireTypes requiresFeatures() const override;
    void updateActions(int numberOfSelectedItems, int numberOfSelectedCollections) override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void connectMessagePane();
    void attachRowActions();
    void scheduleSelectionRefresh();
    void refreshSelection(const Akonadi::Item::List &items);
    void updateAggregateAction();

    QAction *mAction = nullptr;
    QAction *mMenuSeparator = nullptr;
    QPointer<KActionCollection> mActionCollection;
    QPointer<MessageList::Pane> mMessagePane;
    KMailUnsubscribe::UnsubscribeAvailability mAvailability;
    KMailUnsubscribe::UnsubscribeBatch mBatch;
    QList<QPointer<KMailUnsubscribe::UnsubscribeRowActions>> mRowActions;
    Akonadi::Item::List mItems;
    bool mRefreshScheduled = false;
    bool mActivationRequested = false;
};
