#pragma once

#include "unsubscribecomposerrequeststore.h"

#include <Akonadi/Item>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

#include <memory>

class QEventLoopLocker;
namespace Akonadi
{
class ItemCreateJob;
class MessageQueueJob;
class Monitor;
}

namespace KMailUnsubscribe
{
// Watches the exact outgoing items created by this composer, independently of
// its window and the unsubscribe batch. Queueing or saving a draft is not send
// confirmation; only the mail dispatcher's Sent state permits cleanup.
class UnsubscribeEmailCleanup : public QObject
{
    Q_OBJECT
public:
    UnsubscribeEmailCleanup(QObject *composer, const ComposerDeleteRequest &request, QObject *parent = nullptr);
    ~UnsubscribeEmailCleanup() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    virtual void watchOutgoing(const Akonadi::Item &item);
    virtual void moveOriginalToTrash();

private:
    friend class UnsubscribeEmailCleanupTest;
    void watchQueue(Akonadi::MessageQueueJob *queue);
    void watchCreateJob(Akonadi::ItemCreateJob *job);
    void queuedItem(const Akonadi::Item &item);
    void fetchSentState(Akonadi::Item::Id id);
    void observeItem(const Akonadi::Item &item);
    void outgoingRemoved(Akonadi::Item::Id id);
    void checkCompletion();

    QPointer<QObject> mComposer;
    ComposerDeleteRequest mRequest;
    Akonadi::Monitor *mMonitor = nullptr;
    QSet<Akonadi::MessageQueueJob *> mQueues;
    QSet<Akonadi::Item::Id> mFetching;
    QHash<Akonadi::Item::Id, bool> mOutgoing;
    std::unique_ptr<QEventLoopLocker> mQuitLocker;
    bool mQueueFailed = false;
    bool mMoving = false;
};
}
