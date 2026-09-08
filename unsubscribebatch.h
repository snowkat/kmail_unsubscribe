#pragma once

#include "unsubscribeplan.h"

#include <QPointer>
#include <QSet>

class KJob;
class QDialog;
class QProgressDialog;

namespace KMailUnsubscribe
{
// Fetches and checks the whole selection before asking once for consent. It
// owns the snapshot being confirmed, independent of later selection changes.
class UnsubscribeBatch : public QObject
{
    Q_OBJECT
public:
    explicit UnsubscribeBatch(QObject *parent = nullptr);
    ~UnsubscribeBatch() override;
    void setParentWidget(QWidget *parent);
    void start(const Akonadi::Item::List &items);
    [[nodiscard]] bool busy() const;

Q_SIGNALS:
    void busyChanged();

private:
    friend class UnsubscribeBatchTest;
    enum class Phase { Idle, Preparing, Confirming, Executing, Reporting };
    struct Entry
    {
        Akonadi::Item item;
        UnsubscribeWorkflow *workflow = nullptr;
        UnsubscribePlanEntry plan;
        QList<UnsubscribeWorkflow::Method> attempts;
        bool prepared = false;
        bool usedFallback = false;
    };

    void createProgressDialog();
    void prepareNext();
    void prepared(qsizetype index);
    void confirm();
    void executeNext();
    virtual void moveToTrash(qsizetype index);
    void trashFinished(qsizetype index, bool moved, const QString &error = QString());
    void report();
    void stop();

    QPointer<QWidget> mParent;
    QPointer<QProgressDialog> mProgress;
    QPointer<QDialog> mDialog;
    QList<Entry> mEntries;
    QSet<KJob *> mJobs;
    QStringList mErrors;
    QStringList mFallbackDetails;
    QStringList mTrashErrors;
    Phase mPhase = Phase::Idle;
    qsizetype mNext = 0;
    int mPreparing = 0;
    int mPrepared = 0;
    int mOneClickSent = 0;
    int mEmailsOpened = 0;
    int mPagesOpened = 0;
    int mFallbackMessages = 0;
    int mMovedToTrash = 0;
    int mAlreadyInTrash = 0;
    bool mDeleteAfterSuccess = false;
};
}
