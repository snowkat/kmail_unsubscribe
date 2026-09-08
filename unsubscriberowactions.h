#pragma once

#include "unsubscribeavailability.h"

#include <QHash>
#include <QIcon>
#include <QPointer>

class QTreeView;
class QToolButton;

namespace KMailUnsubscribe
{
// Extends the existing, left-aligned Status column of KMail's "Smart with
// Clickable Status" theme. The native delegate and its hit testing stay intact.
class UnsubscribeRowActions : public QObject
{
    Q_OBJECT
public:
    UnsubscribeRowActions(QTreeView *view, UnsubscribeAvailability *availability, QObject *parent = nullptr);
    ~UnsubscribeRowActions() override;
    void setBusy(bool busy);

Q_SIGNALS:
    void unsubscribeRequested(const Akonadi::Item &item);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Buttons
    {
        Akonadi::Item item;
        QPointer<QToolButton> button;
    };

    void scheduleRefresh();
    void refresh();
    void removeButtons(Akonadi::Item::Id id);

    QPointer<QTreeView> mView;
    UnsubscribeAvailability *const mAvailability;
    QHash<Akonadi::Item::Id, Buttons> mButtons;
    QIcon mIcon;
    QIcon mOneClickIcon;
    bool mRefreshScheduled = false;
    bool mBusy = false;
};
}
