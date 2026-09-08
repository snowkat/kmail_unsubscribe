#include "unsubscribeemailcleanup.h"

#include <Akonadi/ErrorAttribute>
#include <Akonadi/MessageFlags>
#include <Akonadi/MessageQueueJob>
#include <QTest>

namespace KMailUnsubscribe
{
struct CleanupEffects
{
    QList<Akonadi::Item::Id> watched;
    int moves = 0;
};

class SimulatedCleanup : public UnsubscribeEmailCleanup
{
public:
    SimulatedCleanup(QObject *composer, CleanupEffects &effects, QObject *parent = nullptr)
        : UnsubscribeEmailCleanup(composer, {101, QStringLiteral("Original list message")}, parent), mEffects(effects) {}

private:
    void watchOutgoing(const Akonadi::Item &item) override { mEffects.watched.append(item.id()); }
    void moveOriginalToTrash() override { ++mEffects.moves; }
    CleanupEffects &mEffects;
};

// The real KDE queue-job type/parenting is used, but start() is never called.
// Neither messages nor requests are sent and no real Akonadi items are read.
class SyntheticQueueJob : public Akonadi::MessageQueueJob
{
public:
    using Akonadi::MessageQueueJob::MessageQueueJob;
    void complete(bool success = true)
    {
        setError(success ? 0 : KJob::UserDefinedError);
        emitResult();
    }
};

class UnsubscribeEmailCleanupTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void discardedComposerKeepsOriginal();
    void draftsAndUnrelatedMailDoNotDelete();
    void queuedEmailWaitsForSendingAfterComposerCloses();
    void failedSendWaitsForSuccessfulRetry();
    void removedOutgoingEmailKeepsOriginal();
    void allOutgoingCopiesMustBeSent();
    void queueFailureKeepsOriginal();

private:
    Akonadi::Item queued(Akonadi::Item::Id id = 1001);
    Akonadi::Item sent(Akonadi::Item::Id id = 1001);
};

Akonadi::Item UnsubscribeEmailCleanupTest::queued(Akonadi::Item::Id id)
{
    Akonadi::Item item(id);
    item.setFlag(Akonadi::MessageFlags::Queued);
    return item;
}

Akonadi::Item UnsubscribeEmailCleanupTest::sent(Akonadi::Item::Id id)
{
    Akonadi::Item item(id);
    item.setFlag(Akonadi::MessageFlags::Sent);
    return item;
}

void UnsubscribeEmailCleanupTest::discardedComposerKeepsOriginal()
{
    CleanupEffects effects;
    auto *composer = new QObject;
    QPointer<SimulatedCleanup> cleanup = new SimulatedCleanup(composer, effects, this);
    delete composer;
    QTRY_VERIFY(cleanup.isNull());
    QCOMPARE(effects.moves, 0);
    QVERIFY(effects.watched.isEmpty());
}

void UnsubscribeEmailCleanupTest::draftsAndUnrelatedMailDoNotDelete()
{
    CleanupEffects effects;
    QObject composer;
    SimulatedCleanup cleanup(&composer, effects);
    // Saving drafts creates items outside MessageQueueJob and must not arm
    // sending cleanup, even though the composer reports sentSuccessfully.
    new QObject(&composer);
    cleanup.queuedItem(Akonadi::Item(2001)); // No Queued flag: a saved draft.
    cleanup.observeItem(sent(3001));
    QCoreApplication::processEvents();
    QVERIFY(cleanup.mQueues.isEmpty());
    QVERIFY(effects.watched.isEmpty());
    QCOMPARE(effects.moves, 0);
}

void UnsubscribeEmailCleanupTest::queuedEmailWaitsForSendingAfterComposerCloses()
{
    CleanupEffects effects;
    auto *composer = new QObject;
    auto *cleanup = new SimulatedCleanup(composer, effects, this);
    auto *queue = new SyntheticQueueJob(composer);
    QTRY_COMPARE(cleanup->mQueues.size(), 1);
    cleanup->queuedItem(queued());
    QCOMPARE(effects.watched, QList<Akonadi::Item::Id>{1001});
    cleanup->observeItem(queued());
    QCOMPARE(effects.moves, 0);
    queue->complete();
    delete composer;
    QCoreApplication::processEvents();
    QCOMPARE(effects.moves, 0);
    cleanup->observeItem(sent(9999)); // Another account/message cannot match.
    cleanup->observeItem(sent());
    QTRY_COMPARE(effects.moves, 1);
    cleanup->observeItem(sent());
    QCoreApplication::processEvents();
    QCOMPARE(effects.moves, 1);
    delete cleanup;
}

void UnsubscribeEmailCleanupTest::failedSendWaitsForSuccessfulRetry()
{
    CleanupEffects effects;
    QObject composer;
    SimulatedCleanup cleanup(&composer, effects);
    auto *queue = new SyntheticQueueJob(&composer);
    QTRY_COMPARE(cleanup.mQueues.size(), 1);
    cleanup.queuedItem(queued());
    queue->complete();
    auto failed = sent();
    failed.setFlag(Akonadi::MessageFlags::HasError);
    failed.addAttribute(new Akonadi::ErrorAttribute(QStringLiteral("Transport failed")));
    cleanup.observeItem(failed);
    QCoreApplication::processEvents();
    QCOMPARE(effects.moves, 0);
    failed.removeAttribute<Akonadi::ErrorAttribute>();
    // KDE can retain HasError after retrying; its Sent state and removal of
    // ErrorAttribute are the authoritative successful result.
    cleanup.observeItem(failed);
    QTRY_COMPARE(effects.moves, 1);
}

void UnsubscribeEmailCleanupTest::removedOutgoingEmailKeepsOriginal()
{
    CleanupEffects effects;
    QObject composer;
    SimulatedCleanup cleanup(&composer, effects);
    auto *queue = new SyntheticQueueJob(&composer);
    QTRY_COMPARE(cleanup.mQueues.size(), 1);
    cleanup.queuedItem(queued());
    queue->complete();
    cleanup.outgoingRemoved(1001);
    cleanup.observeItem(sent());
    QCoreApplication::processEvents();
    QCOMPARE(effects.moves, 0);
}

void UnsubscribeEmailCleanupTest::allOutgoingCopiesMustBeSent()
{
    CleanupEffects effects;
    QObject composer;
    SimulatedCleanup cleanup(&composer, effects);
    auto *first = new SyntheticQueueJob(&composer);
    auto *second = new SyntheticQueueJob(&composer);
    QTRY_COMPARE(cleanup.mQueues.size(), 2);
    cleanup.queuedItem(queued(1001));
    cleanup.queuedItem(queued(1002));
    first->complete();
    cleanup.observeItem(sent(1001));
    QCoreApplication::processEvents();
    QCOMPARE(effects.moves, 0);
    second->complete();
    QCoreApplication::processEvents();
    QCOMPARE(effects.moves, 0);
    cleanup.observeItem(sent(1002));
    QTRY_COMPARE(effects.moves, 1);
}

void UnsubscribeEmailCleanupTest::queueFailureKeepsOriginal()
{
    CleanupEffects effects;
    QObject composer;
    SimulatedCleanup cleanup(&composer, effects);
    auto *queue = new SyntheticQueueJob(&composer);
    QTRY_COMPARE(cleanup.mQueues.size(), 1);
    queue->complete(false);
    cleanup.observeItem(sent());
    QCoreApplication::processEvents();
    QCOMPARE(effects.moves, 0);
}
}

QTEST_GUILESS_MAIN(KMailUnsubscribe::UnsubscribeEmailCleanupTest)
#include "unsubscribeemailcleanuptest.moc"
