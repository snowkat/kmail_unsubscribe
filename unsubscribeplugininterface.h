#pragma once

#include "unsubscribeavailability.h"
#include "unsubscribebatch.h"

#include <MessageViewer/ViewerPluginInterface>
#include <QPointer>

namespace MessageViewer
{
class UnsubscribePluginInterface : public ViewerPluginInterface
{
    Q_OBJECT
public:
    explicit UnsubscribePluginInterface(QWidget *parent, KActionCollection *ac = nullptr);
    ~UnsubscribePluginInterface() override;

    [[nodiscard]] QList<QAction *> actions() const override;
    void closePlugin() override;
    void execute() override;
    void setMessageItem(const Akonadi::Item &item) override;
    void setCurrentCollection(const Akonadi::Collection &collection) override;
    void updateAction(const Akonadi::Item &item) override;
    [[nodiscard]] SpecificFeatureTypes featureTypes() const override { return NeedMessage; }

private:
    void updateActions();

    KMailUnsubscribe::UnsubscribeAvailability mAvailability;
    KMailUnsubscribe::UnsubscribeBatch mBatch;
    Akonadi::Item mItem;
    Akonadi::Collection mCollection;
    QAction *mAction = nullptr;
    QPointer<QWidget> mHeaderBar;
};
}
