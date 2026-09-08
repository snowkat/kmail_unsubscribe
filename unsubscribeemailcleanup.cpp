#include "unsubscribeemailcleanup.h"

#include "unsubscribe_debug.h"
#include "unsubscribetrashjob.h"

#include <Akonadi/ErrorAttribute>
#include <Akonadi/ItemCreateJob>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/MessageFlags>
#include <Akonadi/MessageQueueJob>
#include <Akonadi/Monitor>
#include <QApplication>
#include <QChildEvent>
#include <QEventLoopLocker>
#include <KLocalizedString>
#include <QMessageBox>
#include <QTimer>

using namespace KMailUnsubscribe;

UnsubscribeEmailCleanup::UnsubscribeEmailCleanup(QObject *composer, const ComposerDeleteRequest &request, QObject *parent)
    : QObject(parent), mComposer(composer), mRequest(request)
{
    Q_ASSERT(composer);
    composer->installEventFilter(this);
    connect(composer, &QObject::destroyed, this, [this]() {
        mComposer = nullptr;
        checkCompletion();
    });
}

UnsubscribeEmailCleanup::~UnsubscribeEmailCleanup() = default;

bool UnsubscribeEmailCleanup::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::ChildAdded)
    {
        return false;
    }
    auto *child = static_cast<QChildEvent *>(event)->child();
    if (watched == mComposer)
    {
        // ChildAdded runs inside the QObject constructor. Wait until the
        // MessageQueueJob is fully constructed before testing its type.
        QTimer::singleShot(0, this, [this, child = QPointer<QObject>(child)]() {
            if (auto *queue = qobject_cast<Akonadi::MessageQueueJob *>(child.data()))
            {
                watchQueue(queue);
            }
        });
    }
    else if (mQueues.contains(qobject_cast<Akonadi::MessageQueueJob *>(watched)))
    {
        // KCompositeJob::addSubjob reparents the already constructed create
        // job before wiring its result. Observe it now, before the composer
        // can close and destroy its own queue job on successful queueing.
        if (auto *create = qobject_cast<Akonadi::ItemCreateJob *>(child))
        {
            watchCreateJob(create);
        }
    }
    return false;
}

void UnsubscribeEmailCleanup::watchQueue(Akonadi::MessageQueueJob *queue)
{
    if (mQueues.contains(queue) || mMoving)
    {
        return;
    }
    if (mQueues.isEmpty() && mOutgoing.isEmpty())
    {
        mQueueFailed = false; // A new send attempt after a queueing failure.
    }
    mQueues.insert(queue);
    queue->installEventFilter(this);
    connect(queue, &KJob::result, this, [this, queue]() {
        mQueues.remove(queue);
        mQueueFailed |= queue->error() != 0;
        QTimer::singleShot(0, this, &UnsubscribeEmailCleanup::checkCompletion);
    });
    connect(queue, &QObject::destroyed, this, [this, queue]() {
        if (mQueues.remove(queue))
        {
            mQueueFailed = true;
            checkCompletion();
        }
    });
    const auto created = queue->findChildren<Akonadi::ItemCreateJob *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *job : created)
    {
        watchCreateJob(job);
    }
}

void UnsubscribeEmailCleanup::watchCreateJob(Akonadi::ItemCreateJob *job)
{
    if (job->property("kmailUnsubscribeCleanupObserved").toBool())
    {
        return;
    }
    job->setProperty("kmailUnsubscribeCleanupObserved", true);
    connect(job, &KJob::result, this, [this, job]() {
        if (!job->error())
        {
            queuedItem(job->item());
        }
    });
}

void UnsubscribeEmailCleanup::queuedItem(const Akonadi::Item &item)
{
    if (!item.isValid() || !item.hasFlag(Akonadi::MessageFlags::Queued) || mOutgoing.contains(item.id()))
    {
        return;
    }
    mOutgoing.insert(item.id(), false);
    watchOutgoing(item);
    qCInfo(UnsubscribePlugin) << "Waiting for the unsubscribe email to be sent before moving its original to Trash";
}

void UnsubscribeEmailCleanup::watchOutgoing(const Akonadi::Item &item)
{
    if (!mMonitor)
    {
        // A standalone composer may be the last window. Keep the application
        // alive while its email is dispatched and cleanup is still pending.
        mQuitLocker = std::make_unique<QEventLoopLocker>();
        mMonitor = new Akonadi::Monitor(this);
        mMonitor->itemFetchScope().fetchAllAttributes();
        connect(mMonitor, &Akonadi::Monitor::itemChanged, this, [this](const Akonadi::Item &changed) {
            observeItem(changed);
        });
        connect(mMonitor, &Akonadi::Monitor::itemsFlagsChanged, this,
                [this](const Akonadi::Item::List &items, const QSet<QByteArray> &, const QSet<QByteArray> &) {
                    for (const auto &changed : items)
                    {
                        fetchSentState(changed.id());
                    }
                });
        connect(mMonitor, &Akonadi::Monitor::itemRemoved, this, [this](const Akonadi::Item &removed) {
            outgoingRemoved(removed.id());
        });
        connect(mMonitor, &Akonadi::Monitor::monitorReady, this, [this]() {
            for (auto id : mOutgoing.keys())
            {
                fetchSentState(id);
            }
        });
    }
    mMonitor->setItemMonitored(item);
    // Sending can finish before the monitor subscribes. Check current state
    // as well as future notifications; no email bodies need to be fetched.
    fetchSentState(item.id());
}

void UnsubscribeEmailCleanup::fetchSentState(Akonadi::Item::Id id)
{
    if (!mOutgoing.contains(id) || mFetching.contains(id) || mMoving)
    {
        return;
    }
    mFetching.insert(id);
    auto *fetch = new Akonadi::ItemFetchJob(Akonadi::Item(id), this);
    fetch->fetchScope().fetchAllAttributes();
    connect(fetch, &KJob::result, this, [this, fetch, id]() {
        mFetching.remove(id);
        if (!fetch->error() && fetch->items().size() == 1)
        {
            observeItem(fetch->items().constFirst());
        }
        else if (!fetch->error() && fetch->items().isEmpty())
        {
            outgoingRemoved(id);
        }
    });
}

void UnsubscribeEmailCleanup::observeItem(const Akonadi::Item &item)
{
    if (mOutgoing.contains(item.id()) && item.hasFlag(Akonadi::MessageFlags::Sent)
        && !item.hasFlag(Akonadi::MessageFlags::Queued) && !item.hasAttribute<Akonadi::ErrorAttribute>())
    {
        mOutgoing[item.id()] = true;
        QTimer::singleShot(0, this, &UnsubscribeEmailCleanup::checkCompletion);
    }
}

void UnsubscribeEmailCleanup::outgoingRemoved(Akonadi::Item::Id id)
{
    if (!mOutgoing.contains(id) || mOutgoing.value(id) || mMoving)
    {
        return;
    }
    // Removing an email from Outbox is not proof of sending: the user may
    // have discarded it. This also preserves originals when sent copies are
    // discarded and the dispatcher provides no Sent state to verify.
    mOutgoing.remove(id);
    mQueueFailed = true;
    qCInfo(UnsubscribePlugin) << "Kept the original message: the outgoing email was removed without confirmed sending";
    checkCompletion();
}

void UnsubscribeEmailCleanup::checkCompletion()
{
    if (mMoving || !mQueues.isEmpty())
    {
        return;
    }
    if (!mQueueFailed && !mOutgoing.isEmpty())
    {
        bool allSent = true;
        for (bool sent : std::as_const(mOutgoing))
        {
            allSent &= sent;
        }
        if (allSent)
        {
            mMoving = true;
            moveOriginalToTrash();
            return;
        }
    }
    if (!mComposer && (mOutgoing.isEmpty() || mQueueFailed))
    {
        deleteLater();
    }
}

void UnsubscribeEmailCleanup::moveOriginalToTrash()
{
    auto *move = new UnsubscribeTrashJob(mRequest.originalId, this);
    connect(move, &KJob::result, this, [this, move]() {
        if (move->error())
        {
            auto *dialog = new QMessageBox(QMessageBox::Warning, i18n("Unsubscribe"),
                                          i18n("The unsubscribe email for “%1” was sent.\n%2", mRequest.subject, move->errorText()),
                                          QMessageBox::Ok, QApplication::activeWindow());
            dialog->setTextFormat(Qt::PlainText);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
        }
        else
        {
            qCInfo(UnsubscribePlugin) << "Unsubscribe email sent; original message is in Trash";
        }
        deleteLater();
    });
    move->start();
}

#include "moc_unsubscribeemailcleanup.cpp"
