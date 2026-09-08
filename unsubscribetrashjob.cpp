#include "unsubscribetrashjob.h"

#include <Akonadi/CollectionFetchJob>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemMoveJob>
#include <KLocalizedString>
#include <MailCommon/MailKernel>

using namespace KMailUnsubscribe;

UnsubscribeTrashJob::UnsubscribeTrashJob(Akonadi::Item::Id originalId, QObject *parent)
    : KJob(parent), mOriginalId(originalId)
{
    setCapabilities(KJob::Killable);
}

bool UnsubscribeTrashJob::moved() const
{
    return mMoved;
}

void UnsubscribeTrashJob::fail(const QString &message)
{
    setError(KJob::UserDefinedError);
    setErrorText(message);
    emitResult();
}

void UnsubscribeTrashJob::start()
{
    // Resolve the real storage folder again: search folders and a changed
    // selection must not redirect deletion to a different message/account.
    auto *fetch = new Akonadi::ItemFetchJob(Akonadi::Item(mOriginalId), this);
    connect(fetch, &KJob::result, this, [this, fetch]() {
        if (fetch->error() || fetch->items().size() != 1 || fetch->items().constFirst().storageCollectionId() <= 0)
        {
            fail(i18n("Unsubscribed, but KMail could not find the message's folder to move it to Trash."));
            return;
        }
        const auto item = fetch->items().constFirst();
        auto *folderFetch = new Akonadi::CollectionFetchJob(Akonadi::Collection(item.storageCollectionId()),
                                                           Akonadi::CollectionFetchJob::Base, this);
        connect(folderFetch, &KJob::result, this, [this, folderFetch, item]() {
            if (folderFetch->error() || folderFetch->collections().size() != 1)
            {
                fail(i18n("Unsubscribed, but KMail could not find the message's folder to move it to Trash."));
                return;
            }
            const auto source = folderFetch->collections().constFirst();
            auto *kernel = MailCommon::Kernel::self();
            auto trash = kernel->trashCollectionFromResource(source);
            if (!trash.isValid())
            {
                trash = kernel->trashCollectionFolder();
            }
            if (!trash.isValid())
            {
                fail(i18n("Unsubscribed, but no Trash folder is available. The message was kept."));
                return;
            }
            if (source == trash || kernel->folderIsTrash(source))
            {
                // Repeating an unsubscribe in Trash must never permanently
                // delete the message.
                emitResult();
                return;
            }
            auto *move = new Akonadi::ItemMoveJob(item, trash, this);
            connect(move, &KJob::result, this, [this, move]() {
                if (move->error())
                {
                    fail(i18n("Unsubscribed, but KMail could not move the message to Trash."));
                    return;
                }
                mMoved = true;
                emitResult();
            });
        });
    });
}

bool UnsubscribeTrashJob::doKill()
{
    const auto jobs = findChildren<KJob *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *job : jobs)
    {
        job->disconnect(this);
        job->kill(KJob::Quietly);
    }
    return true;
}

#include "moc_unsubscribetrashjob.cpp"
