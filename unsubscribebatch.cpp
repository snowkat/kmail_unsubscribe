#include "unsubscribebatch.h"

#include "unsubscribeconfirmation.h"
#include "unsubscribetrashjob.h"
#include "unsubscribe_debug.h"

#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <QApplication>
#include <KJob>
#include <KLocalizedString>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QTimer>

#include <limits>

using namespace KMailUnsubscribe;

namespace
{
QString messageSubject(const Akonadi::Item &item)
{
    if (item.hasPayload<std::shared_ptr<KMime::Message>>())
    {
        const std::shared_ptr<const KMime::Message> message = item.payload<std::shared_ptr<KMime::Message>>();
        const auto *header = message ? message->subject() : nullptr;
        if (header)
        {
            const QString subject = header->asUnicodeString().simplified();
            if (!subject.isEmpty())
            {
                return subject;
            }
        }
    }
    return i18n("(No subject)");
}

QWidget *dialogParent(QWidget *preferredParent)
{
    if (preferredParent && preferredParent->window() && preferredParent->window()->isVisible())
    {
        return preferredParent->window();
    }
    if (auto *activeWindow = QApplication::activeWindow())
    {
        return activeWindow;
    }
    return preferredParent;
}

void presentConfirmation(QDialog *dialog)
{
    // QAction slots run while a toolbar/menu popup can still own focus. Queue
    // presentation so the dialog is raised after that popup has closed.
    QTimer::singleShot(0, dialog, [dialog]() {
        dialog->setWindowModality(Qt::ApplicationModal);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
}

const char *methodName(UnsubscribeWorkflow::Method method)
{
    switch (method)
    {
    case UnsubscribeWorkflow::Method::OneClick:
        return "one-click";
    case UnsubscribeWorkflow::Method::Web:
        return "web";
    case UnsubscribeWorkflow::Method::Email:
        return "email";
    }
    return "unknown";
}
}

UnsubscribeBatch::UnsubscribeBatch(QObject *parent)
    : QObject(parent)
{
}

UnsubscribeBatch::~UnsubscribeBatch()
{
    disconnect(this, nullptr, nullptr, nullptr);
    stop();
}

void UnsubscribeBatch::setParentWidget(QWidget *parent)
{
    mParent = parent;
}

bool UnsubscribeBatch::busy() const
{
    return mPhase != Phase::Idle;
}

void UnsubscribeBatch::start(const Akonadi::Item::List &items)
{
    if (busy())
    {
        qCInfo(UnsubscribePlugin) << "Ignored unsubscribe request while another unsubscribe operation is active";
        return;
    }
    QSet<Akonadi::Item::Id> ids;
    for (const auto &item : items)
    {
        if (item.isValid() && !ids.contains(item.id()))
        {
            ids.insert(item.id());
            Entry entry;
            entry.item = item;
            entry.plan.subject = messageSubject(item);
            mEntries.append(std::move(entry));
        }
    }
    if (mEntries.isEmpty())
    {
        qCInfo(UnsubscribePlugin) << "Ignored unsubscribe request without valid messages";
        return;
    }
    mPhase = Phase::Preparing;
    qCInfo(UnsubscribePlugin) << "Started unsubscribe batch for" << mEntries.size() << "message(s)";
    Q_EMIT busyChanged();
    createProgressDialog();
    prepareNext();
}

void UnsubscribeBatch::createProgressDialog()
{
    mProgress = new QProgressDialog(i18n("Checking unsubscribe methods…"), i18n("Cancel"), 0, mEntries.size(), dialogParent(mParent));
    mProgress->setObjectName(QStringLiteral("unsubscribeProgress"));
    mProgress->setWindowTitle(i18n("Unsubscribe"));
    mProgress->setWindowModality(Qt::NonModal);
    // QProgressDialog can show early based on its estimate of remaining work.
    // Use a fixed delay instead so quick checks never flash a second window.
    mProgress->setMinimumDuration(std::numeric_limits<int>::max());
    connect(mProgress, &QProgressDialog::canceled, this, &UnsubscribeBatch::stop);
    mProgress->setValue(0);
    QTimer::singleShot(800, mProgress, [progress = mProgress]() {
        if (progress && progress->value() >= progress->minimum() && progress->value() < progress->maximum())
        {
            progress->show();
        }
    });
}

void UnsubscribeBatch::prepareNext()
{
    if (mPhase != Phase::Preparing)
    {
        return;
    }
    if (mPrepared == mEntries.size())
    {
        mProgress->reset();
        mProgress->deleteLater();
        mProgress = nullptr;
        confirm();
        return;
    }
    // Bound body retrieval and DKIM/DNS work, including large selections.
    while (mPreparing < 2 && mNext < mEntries.size())
    {
        const qsizetype index = mNext++;
        ++mPreparing;
        auto *job = new Akonadi::ItemFetchJob(Akonadi::Item(mEntries[index].item.id()), this);
        mJobs.insert(job);
        // A typed envelope, even one reporting HEAD as loaded, is incomplete.
        job->fetchScope().fetchFullPayload();
        job->fetchScope().setAncestorRetrieval(Akonadi::ItemFetchScope::Parent);
        connect(job, &KJob::result, this, [this, job, index]() {
            mJobs.remove(job);
            if (job->error() || job->items().isEmpty()
                || !job->items().first().hasPayload<std::shared_ptr<KMime::Message>>()
                || !job->items().first().payload<std::shared_ptr<KMime::Message>>())
            {
                mEntries[index].plan.loadFailed = true;
                prepared(index);
                return;
            }
            auto &entry = mEntries[index];
            entry.plan.subject = messageSubject(job->items().first());
            entry.workflow = new UnsubscribeWorkflow(this);
            entry.workflow->setParentWidget(mParent);
            entry.workflow->setCurrentCollection(entry.item.parentCollection());
            connect(entry.workflow, &UnsubscribeWorkflow::stateChanged, this, [this, index]() {
                if (mEntries[index].workflow->webCapability() != UnsubscribeWorkflow::WebCapability::VerifyingOneClick)
                {
                    prepared(index);
                }
            });
            entry.workflow->setMessageItem(job->items().first(), true);
            if (entry.workflow->webCapability() != UnsubscribeWorkflow::WebCapability::VerifyingOneClick)
            {
                prepared(index);
            }
        });
    }
}

void UnsubscribeBatch::prepared(qsizetype index)
{
    if (mPhase != Phase::Preparing || index >= mEntries.size())
    {
        return;
    }
    auto &entry = mEntries[index];
    if (entry.prepared)
    {
        return;
    }
    entry.prepared = true;
    if (entry.workflow)
    {
        disconnect(entry.workflow, &UnsubscribeWorkflow::stateChanged, this, nullptr);
        entry.attempts = unsubscribeMethods({
            entry.workflow->emailAvailable(),
            entry.workflow->webAvailable(),
            entry.workflow->webCapability() == UnsubscribeWorkflow::WebCapability::OneClick,
        });
        entry.plan.method = entry.attempts.isEmpty()
            ? std::nullopt
            : std::optional<UnsubscribeWorkflow::Method>(entry.attempts.constFirst());
        entry.plan.fallbackMethods = entry.attempts;
        if (!entry.plan.fallbackMethods.isEmpty())
        {
            entry.plan.fallbackMethods.removeFirst();
        }
        entry.plan.invalidEmail = entry.workflow->emailAdvertised() && !entry.workflow->emailAvailable();
        qCInfo(UnsubscribePlugin) << "Prepared unsubscribe methods: one-click="
                                 << (entry.workflow->webCapability() == UnsubscribeWorkflow::WebCapability::OneClick)
                                 << "web=" << entry.workflow->webAvailable()
                                 << "email=" << entry.workflow->emailAvailable()
                                 << "invalid-email=" << entry.plan.invalidEmail;
    }
    --mPreparing;
    ++mPrepared;
    if (mProgress)
    {
        mProgress->setValue(mPrepared);
    }
    QTimer::singleShot(0, this, &UnsubscribeBatch::prepareNext);
}

void UnsubscribeBatch::confirm()
{
    mPhase = Phase::Confirming;
    QList<UnsubscribePlanEntry> plan;
    plan.reserve(mEntries.size());
    for (const auto &entry : std::as_const(mEntries))
    {
        plan.append(entry.plan);
    }
    int available = 0;
    for (const auto &entry : plan)
    {
        available += entry.method.has_value();
    }
    qCInfo(UnsubscribePlugin) << "Showing unsubscribe confirmation for" << plan.size() << "message(s);" << available
                             << "have an available method";
    auto *dialog = new UnsubscribeConfirmation(plan, dialogParent(mParent));
    mDialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::finished, this, [this, dialog]() {
        const bool confirmed = dialog->confirmed();
        mDialog = nullptr;
        qCInfo(UnsubscribePlugin) << "Unsubscribe confirmation" << (confirmed ? "accepted" : "canceled");
        if (!confirmed)
        {
            stop();
            return;
        }
        mDeleteAfterSuccess = dialog->deleteAfterSuccess();
        mPhase = Phase::Executing;
        mNext = 0;
        QTimer::singleShot(0, this, &UnsubscribeBatch::executeNext);
    });
    presentConfirmation(dialog);
}

void UnsubscribeBatch::executeNext()
{
    if (mPhase != Phase::Executing)
    {
        return;
    }
    while (mNext < mEntries.size())
    {
        auto &entry = mEntries[mNext];
        if (!entry.workflow || entry.attempts.isEmpty())
        {
            ++mNext;
            continue;
        }
        const qsizetype index = mNext;
        const auto method = entry.attempts.takeFirst();
        auto *workflow = entry.workflow;
        workflow->setDeleteAfterSuccess(mDeleteAfterSuccess);
        qCInfo(UnsubscribePlugin) << "Executing unsubscribe method:" << methodName(method);
        connect(workflow, &UnsubscribeWorkflow::finished, this, [this, workflow, index, method](bool success, const QString &error) {
            disconnect(workflow, &UnsubscribeWorkflow::finished, this, nullptr);
            if (mPhase != Phase::Executing || index >= mEntries.size())
            {
                return;
            }
            auto &entry = mEntries[index];
            if (success)
            {
                if (method == UnsubscribeWorkflow::Method::OneClick)
                {
                    ++mOneClickSent;
                    // Email cleanup is handled separately by the editor
                    // plugin after sending, never merely opening its composer.
                    if (mDeleteAfterSuccess)
                    {
                        moveToTrash(index);
                        return;
                    }
                }
                else if (method == UnsubscribeWorkflow::Method::Email)
                {
                    ++mEmailsOpened;
                }
                else
                {
                    ++mPagesOpened;
                }
                ++mNext;
            }
            else if (!entry.attempts.isEmpty())
            {
                if (!entry.usedFallback)
                {
                    entry.usedFallback = true;
                    ++mFallbackMessages;
                }
                const QString safeError = error.isEmpty()
                    ? i18n("KMail could not complete the unsubscribe action.")
                    : error;
                mFallbackDetails.append(i18nc("Message subject, failure, and the next unsubscribe method",
                                               "%1\n%2\nTrying %3 instead.",
                                               entry.plan.subject,
                                               safeError,
                                               unsubscribeMethodLabel(entry.attempts.constFirst())));
                qCInfo(UnsubscribePlugin) << "Unsubscribe method failed; trying fallback:" << methodName(entry.attempts.constFirst());
            }
            else
            {
                QStringList details;
                if (!error.isEmpty())
                {
                    details.append(error);
                }
                if (entry.plan.invalidEmail)
                {
                    details.append(entry.workflow->emailValidationError());
                }
                if (details.isEmpty())
                {
                    details.append(i18n("KMail could not complete the unsubscribe action."));
                }
                mErrors.append(i18nc("Message subject followed by its unsubscribe error",
                                     "%1\n%2",
                                     entry.plan.subject,
                                     details.join(QLatin1Char('\n'))));
                ++mNext;
            }
            QTimer::singleShot(0, this, &UnsubscribeBatch::executeNext);
        });
        workflow->execute(method);
        return;
    }
    report();
}

void UnsubscribeBatch::moveToTrash(qsizetype index)
{
    auto *move = new UnsubscribeTrashJob(mEntries[index].item.id(), this);
    mJobs.insert(move);
    connect(move, &KJob::result, this, [this, move, index]() {
        mJobs.remove(move);
        trashFinished(index, move->moved(), move->error() ? move->errorText() : QString());
    });
    move->start();
}

void UnsubscribeBatch::trashFinished(qsizetype index, bool moved, const QString &error)
{
    if (mPhase != Phase::Executing || index >= mEntries.size())
    {
        return;
    }
    if (!error.isEmpty())
    {
        mTrashErrors.append(i18nc("Message subject followed by its Trash error", "%1\n%2", mEntries[index].plan.subject, error));
    }
    else if (moved)
    {
        ++mMovedToTrash;
    }
    else
    {
        ++mAlreadyInTrash;
    }
    ++mNext;
    QTimer::singleShot(0, this, &UnsubscribeBatch::executeNext);
}

void UnsubscribeBatch::report()
{
    if (mOneClickSent == 0 && mErrors.isEmpty() && mFallbackDetails.isEmpty())
    {
        // The opened composers/pages already provide the result to the user.
        stop();
        return;
    }
    QStringList lines;
    if (mOneClickSent > 0)
    {
        lines << i18np("Sent %1 one-click unsubscribe request.", "Sent %1 one-click unsubscribe requests.", mOneClickSent);
    }
    if (mMovedToTrash > 0)
    {
        lines << i18np("Moved %1 unsubscribed message to Trash.", "Moved %1 unsubscribed messages to Trash.", mMovedToTrash);
    }
    if (mAlreadyInTrash > 0)
    {
        lines << i18np("%1 unsubscribed message was already in Trash.", "%1 unsubscribed messages were already in Trash.", mAlreadyInTrash);
    }
    if (mEmailsOpened > 0)
    {
        lines << i18np("Opened %1 unsubscribe email for you to send.", "Opened %1 unsubscribe emails for you to send.", mEmailsOpened);
    }
    if (mPagesOpened > 0)
    {
        lines << i18np("Opened %1 unsubscribe page.", "Opened %1 unsubscribe pages.", mPagesOpened);
    }
    if (mFallbackMessages > 0)
    {
        lines << i18np("Used a fallback unsubscribe method for %1 message.",
                      "Used a fallback unsubscribe method for %1 messages.",
                      mFallbackMessages);
    }
    if (!mErrors.isEmpty())
    {
        lines << i18np("%1 unsubscribe action failed.", "%1 unsubscribe actions failed.", mErrors.size());
    }
    if (!mTrashErrors.isEmpty())
    {
        lines << i18np("Could not move %1 unsubscribed message to Trash.", "Could not move %1 unsubscribed messages to Trash.", mTrashErrors.size());
    }
    const auto icon = mErrors.isEmpty() && mTrashErrors.isEmpty() ? QMessageBox::Information : QMessageBox::Warning;
    const QString text = lines.join(QLatin1Char('\n'));
    const QString details = (mFallbackDetails + mErrors + mTrashErrors).join(QStringLiteral("\n\n"));
    QWidget *const parent = dialogParent(mParent);

    // The result is informational. Release the batch before showing it so a
    // stale or obscured result window cannot silently disable later actions.
    stop();
    qCInfo(UnsubscribePlugin) << "Showing unsubscribe results";
    auto *dialog = new QMessageBox(icon, i18n("Unsubscribe Results"), text, QMessageBox::Ok, parent);
    dialog->setTextFormat(Qt::PlainText);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (!details.isEmpty())
    {
        dialog->setDetailedText(details);
    }
    QTimer::singleShot(0, dialog, [dialog]() {
        dialog->setWindowModality(Qt::NonModal);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
}

void UnsubscribeBatch::stop()
{
    mPhase = Phase::Idle;
    const auto jobs = mJobs;
    mJobs.clear();
    for (auto *job : jobs)
    {
        job->disconnect(this);
        job->kill(KJob::Quietly);
    }
    for (const auto &entry : std::as_const(mEntries))
    {
        if (entry.workflow)
        {
            entry.workflow->disconnect(this);
            entry.workflow->deleteLater();
        }
    }
    mEntries.clear();
    if (mProgress)
    {
        mProgress->disconnect(this);
        mProgress->hide();
        mProgress->deleteLater();
        mProgress = nullptr;
    }
    if (mDialog)
    {
        mDialog->disconnect(this);
        mDialog->close();
        mDialog = nullptr;
    }
    mNext = 0;
    mPreparing = 0;
    mPrepared = 0;
    mOneClickSent = 0;
    mEmailsOpened = 0;
    mPagesOpened = 0;
    mFallbackMessages = 0;
    mMovedToTrash = 0;
    mAlreadyInTrash = 0;
    mDeleteAfterSuccess = false;
    mErrors.clear();
    mFallbackDetails.clear();
    mTrashErrors.clear();
    Q_EMIT busyChanged();
}
