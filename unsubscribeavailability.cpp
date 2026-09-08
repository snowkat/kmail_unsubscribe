#include "unsubscribeavailability.h"

#include "unsubscribe_debug.h"

#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/MessageParts>
#include <KJob>
#include <QSet>

#include <algorithm>

using namespace KMailUnsubscribe;

UnsubscribeAvailability::UnsubscribeAvailability(QObject *parent)
    : QObject(parent)
{
    mClock.start();
}

UnsubscribeAvailability::State UnsubscribeAvailability::state(Akonadi::Item::Id id) const
{
    return mEntries.value(id).state;
}

void UnsubscribeAvailability::requestItems(const Akonadi::Item::List &items)
{
    const qint64 now = mClock.elapsed();
    const quint64 generation = ++mGeneration;
    Akonadi::Item::List missing;
    for (const auto &item : items)
    {
        if (!item.isValid())
        {
            continue;
        }
        auto entry = mEntries.find(item.id());
        if (entry != mEntries.end())
        {
            entry->usedAt = now;
            // DKIM writes an Akonadi attribute, which changes the revision.
            // Do not turn that revision change into another verification loop.
            if (entry->pending || now - entry->requestedAt < 60000)
            {
                continue;
            }
        }
        mEntries.insert(item.id(), Entry{State{}, generation, now, now, true});
        missing.append(Akonadi::Item(item.id()));
        if (missing.size() == 100)
        {
            fetchHeaders(missing, generation);
            missing.clear();
        }
    }
    if (!missing.isEmpty())
    {
        fetchHeaders(missing, generation);
    }
    trimCache();
}

void UnsubscribeAvailability::fetchHeaders(const Akonadi::Item::List &items, quint64 generation)
{
    // ENVELOPE has a KMime payload and even reports HEAD as loaded, but omits
    // List-Unsubscribe. An explicit HEAD fetch is essential for list items.
    auto *job = new Akonadi::ItemFetchJob(items, this);
    job->fetchScope().fetchPayloadPart(Akonadi::MessagePart::Header);
    job->fetchScope().setIgnoreRetrievalErrors(true);
    connect(job, &KJob::result, this, [this, job, items, generation]() {
        for (const auto &requested : items)
        {
            auto entry = mEntries.find(requested.id());
            if (entry != mEntries.end() && entry->generation == generation)
            {
                entry->state.loaded = true;
                entry->pending = false;
            }
        }
        if (job->error())
        {
            qCWarning(UnsubscribePlugin) << "Could not inspect unsubscribe headers:" << job->errorString();
        }
        for (const auto &item : job->items())
        {
            auto entry = mEntries.find(item.id());
            if (entry == mEntries.end() || entry->generation != generation)
            {
                continue;
            }
            UnsubscribeWorkflow workflow;
            workflow.setMessageItem(item, false);
            entry->state.email = workflow.emailAvailable();
            entry->state.web = workflow.webCapability();
            if (workflow.hasOneClickCandidate())
            {
                entry->pending = true;
                entry->state.web = UnsubscribeWorkflow::WebCapability::VerifyingOneClick;
                mVerificationQueue.enqueue({item.id(), generation});
            }
        }
        Q_EMIT changed();
        verifyNext();
    });
}

void UnsubscribeAvailability::verifyNext()
{
    // Reading a large selection must not launch hundreds of body downloads or
    // DNS checks at once. Plain web/email actions remain usable while checking.
    while (mVerifying < 2 && !mVerificationQueue.isEmpty())
    {
        const Verification request = mVerificationQueue.dequeue();
        auto entry = mEntries.constFind(request.id);
        if (entry == mEntries.cend() || entry->generation != request.generation)
        {
            continue;
        }
        ++mVerifying;
        auto *job = new Akonadi::ItemFetchJob(Akonadi::Item(request.id), this);
        job->fetchScope().fetchFullPayload();
        connect(job, &KJob::result, this, [this, job, request]() {
            if (job->error() || job->items().isEmpty())
            {
                finishVerification(request);
                return;
            }
            auto *workflow = new UnsubscribeWorkflow(this);
            workflow->setMessageItem(job->items().first(), true);
            if (workflow->webCapability() != UnsubscribeWorkflow::WebCapability::VerifyingOneClick)
            {
                finishVerification(request, workflow);
                return;
            }
            connect(workflow, &UnsubscribeWorkflow::stateChanged, this, [this, workflow, request]() {
                if (workflow->webCapability() != UnsubscribeWorkflow::WebCapability::VerifyingOneClick)
                {
                    workflow->disconnect(this);
                    finishVerification(request, workflow);
                }
            });
        });
    }
}

void UnsubscribeAvailability::finishVerification(Verification request, UnsubscribeWorkflow *workflow)
{
    auto entry = mEntries.find(request.id);
    if (entry != mEntries.end() && entry->generation == request.generation)
    {
        entry->pending = false;
        entry->state.web = workflow ? workflow->webCapability() : UnsubscribeWorkflow::WebCapability::Regular;
    }
    if (workflow)
    {
        workflow->deleteLater();
    }
    --mVerifying;
    Q_EMIT changed();
    verifyNext();
}

void UnsubscribeAvailability::trimCache()
{
    if (mEntries.size() <= 1024)
    {
        return;
    }
    auto ids = mEntries.keys();
    std::sort(ids.begin(), ids.end(), [this](auto a, auto b) {
        return mEntries.value(a).usedAt < mEntries.value(b).usedAt;
    });
    const qint64 now = mClock.elapsed();
    for (const auto id : ids)
    {
        const auto entry = mEntries.constFind(id);
        // Keep current requests and recent visible rows. Large selections may
        // temporarily exceed the cap; each entry holds only display state.
        if (!entry->pending && now - entry->usedAt > 60000)
        {
            mEntries.remove(id);
        }
        if (mEntries.size() <= 1024)
        {
            break;
        }
    }
}
