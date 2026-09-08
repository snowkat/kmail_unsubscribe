#include "unsubscribebatch.h"

#include <KLocalizedString>
#include <QCheckBox>
#include <QDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QTest>

namespace KMailUnsubscribe
{
using Method = UnsubscribeWorkflow::Method;

// Only the two external side effects are substituted. The real confirmation,
// fallback routing, cleanup decision, and result reporting run unchanged.
class SimulatedWorkflow : public UnsubscribeWorkflow
{
public:
    SimulatedWorkflow(QList<Method> &executed, const QList<bool> &results, QObject *parent)
        : UnsubscribeWorkflow(parent), mExecuted(executed), mResults(results)
    {
    }

    void execute(Method method) override
    {
        QVERIFY(!mResults.isEmpty());
        mExecuted.append(method);
        const bool success = mResults.takeFirst();
        Q_EMIT finished(success, success ? QString() : QStringLiteral("Synthetic unsubscribe failure."));
    }

private:
    QList<Method> &mExecuted;
    QList<bool> mResults;
};

class SimulatedBatch : public UnsubscribeBatch
{
public:
    using UnsubscribeBatch::UnsubscribeBatch;
    QList<qsizetype> trashRequests;

private:
    void moveToTrash(qsizetype index) override
    {
        trashRequests.append(index);
        // The test completes the asynchronous move explicitly. No Akonadi
        // job is created and no real message is fetched, moved, or deleted.
    }
};

class UnsubscribeBatchTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cancelKeepsMessages();
    void uncheckedKeepsSuccessfulMessages();
    void mixedResultsOnlyTrashConfirmedUnsubscribes();
    void quickPreparationDoesNotFlashProgress();
    void slowPreparationShowsCancelableProgress();

private:
    void addEntry(UnsubscribeBatch &batch, Akonadi::Item::Id id, const QList<Method> &methods,
                  const QList<bool> &results, QList<Method> &executed);
};

void UnsubscribeBatchTest::initTestCase()
{
    KLocalizedString::setApplicationDomain("kmail_unsubscribe");
    KLocalizedString::setLanguages({QStringLiteral("en_US")});
}

void UnsubscribeBatchTest::quickPreparationDoesNotFlashProgress()
{
    QWidget parent;
    QList<Method> executed;
    SimulatedBatch batch;
    batch.setParentWidget(&parent);
    addEntry(batch, 101, {Method::Email}, {true}, executed);
    batch.mPhase = UnsubscribeBatch::Phase::Preparing;
    batch.createProgressDialog();
    QPointer<QProgressDialog> progress = batch.mProgress;
    QVERIFY(progress);
    QVERIFY(!progress->isVisible());

    int progressShows = 0;
    class ShowCounter : public QObject
    {
    public:
        explicit ShowCounter(int &count) : mCount(count) {}
        bool eventFilter(QObject *, QEvent *event) override
        {
            mCount += event->type() == QEvent::Show;
            return false;
        }
        int &mCount;
    } counter(progressShows);
    progress->installEventFilter(&counter);
    batch.mPrepared = batch.mEntries.size();
    batch.prepareNext();
    auto *dialog = parent.findChild<QDialog *>(QStringLiteral("unsubscribeConfirmation"));
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->isVisible());
    // Let the delayed-show deadline pass while confirmation is displayed.
    QTest::qWait(1000);
    QVERIFY(progress.isNull());
    QCOMPARE(progressShows, 0);
    QVERIFY(executed.isEmpty());
    dialog->reject();
    QVERIFY(!batch.busy());
}

void UnsubscribeBatchTest::slowPreparationShowsCancelableProgress()
{
    QWidget parent;
    QList<Method> executed;
    SimulatedBatch batch;
    batch.setParentWidget(&parent);
    addEntry(batch, 101, {Method::Email}, {true}, executed);
    batch.mPhase = UnsubscribeBatch::Phase::Preparing;
    batch.createProgressDialog();
    auto *progress = batch.mProgress.data();
    QTest::qWait(150);
    QVERIFY(!progress->isVisible());
    QTRY_VERIFY(progress->isVisible());
    auto *cancel = progress->findChild<QPushButton *>();
    QVERIFY(cancel);
    cancel->click();
    QVERIFY(!batch.busy());
    QVERIFY(executed.isEmpty());
    QVERIFY(batch.trashRequests.isEmpty());
}

void UnsubscribeBatchTest::addEntry(UnsubscribeBatch &batch, Akonadi::Item::Id id,
                                   const QList<Method> &methods, const QList<bool> &results, QList<Method> &executed)
{
    UnsubscribeBatch::Entry entry;
    entry.item = Akonadi::Item(id);
    entry.plan.subject = QStringLiteral("List %1").arg(id);
    entry.attempts = methods;
    if (!methods.isEmpty())
    {
        entry.plan.method = methods.constFirst();
        entry.plan.fallbackMethods = methods.sliced(1);
        entry.workflow = new SimulatedWorkflow(executed, results, &batch);
    }
    batch.mEntries.append(entry);
}

void UnsubscribeBatchTest::cancelKeepsMessages()
{
    QWidget parent;
    QList<Method> executed;
    SimulatedBatch batch;
    batch.setParentWidget(&parent);
    addEntry(batch, 101, {Method::OneClick, Method::Web, Method::Email}, {true}, executed);
    batch.confirm();
    auto *dialog = parent.findChild<QDialog *>(QStringLiteral("unsubscribeConfirmation"));
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->isVisible());
    QVERIFY(dialog->findChild<QCheckBox *>()->isChecked());
    QVERIFY(executed.isEmpty());
    QVERIFY(batch.trashRequests.isEmpty());
    dialog->findChild<QPushButton *>(QStringLiteral("unsubscribeCancel"))->click();
    QVERIFY(!batch.busy());
    QVERIFY(executed.isEmpty());
    QVERIFY(batch.trashRequests.isEmpty());
}

void UnsubscribeBatchTest::uncheckedKeepsSuccessfulMessages()
{
    QWidget parent;
    QList<Method> executed;
    SimulatedBatch batch;
    batch.setParentWidget(&parent);
    addEntry(batch, 101, {Method::OneClick, Method::Web, Method::Email}, {true}, executed);
    batch.confirm();
    auto *dialog = parent.findChild<QDialog *>(QStringLiteral("unsubscribeConfirmation"));
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->isVisible());
    dialog->findChild<QCheckBox *>()->setChecked(false);
    dialog->findChild<QPushButton *>(QStringLiteral("unsubscribeConfirm"))->click();
    QTRY_VERIFY(!batch.busy());
    QCOMPARE(executed, QList<Method>{Method::OneClick});
    QVERIFY(batch.trashRequests.isEmpty());
    auto *result = parent.findChild<QMessageBox *>();
    QVERIFY(result);
    QVERIFY(result->text().contains(QStringLiteral("Sent 1 one-click")));
    QVERIFY(!result->text().contains(QStringLiteral("Trash")));
}

void UnsubscribeBatchTest::mixedResultsOnlyTrashConfirmedUnsubscribes()
{
    QWidget parent;
    QList<Method> executed[6];
    SimulatedBatch batch;
    batch.setParentWidget(&parent);
    const QList<Method> allMethods = {Method::OneClick, Method::Web, Method::Email};
    addEntry(batch, 101, allMethods, {true}, executed[0]);
    addEntry(batch, 102, allMethods, {false, true}, executed[1]);
    addEntry(batch, 103, {Method::Web, Method::Email}, {false, true}, executed[2]);
    addEntry(batch, 104, {}, {}, executed[3]);
    addEntry(batch, 105, allMethods, {true}, executed[4]);
    addEntry(batch, 106, allMethods, {false, false, false}, executed[5]);
    batch.confirm();
    auto *dialog = parent.findChild<QDialog *>(QStringLiteral("unsubscribeConfirmation"));
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->isVisible());
    QVERIFY(dialog->findChild<QCheckBox *>()->isChecked());
    dialog->findChild<QPushButton *>(QStringLiteral("unsubscribeConfirm"))->click();

    QTRY_COMPARE(batch.trashRequests.size(), 1);
    QCOMPARE(batch.trashRequests.constFirst(), qsizetype(0));
    QCOMPARE(batch.mEntries[0].item.id(), Akonadi::Item::Id(101));
    QVERIFY(batch.busy()); // Wait for the move before reporting success.
    batch.trashFinished(0, true);
    QTRY_COMPARE(batch.trashRequests.size(), 2);
    QCOMPARE(batch.trashRequests.constLast(), qsizetype(4));
    QCOMPARE(batch.mEntries[4].item.id(), Akonadi::Item::Id(105));
    // A failed Trash move must not retry an already successful unsubscribe.
    batch.trashFinished(4, false, QStringLiteral("Synthetic Trash failure."));
    QTRY_VERIFY(!batch.busy());
    QCOMPARE(batch.trashRequests.size(), 2);
    QCOMPARE(executed[0], QList<Method>{Method::OneClick});
    QCOMPARE(executed[1], (QList<Method>{Method::OneClick, Method::Web}));
    QCOMPARE(executed[2], (QList<Method>{Method::Web, Method::Email}));
    QVERIFY(executed[3].isEmpty());
    QCOMPARE(executed[4], QList<Method>{Method::OneClick});
    QCOMPARE(executed[5], allMethods);
    auto *result = parent.findChild<QMessageBox *>();
    QVERIFY(result);
    QCOMPARE(result->icon(), QMessageBox::Warning);
    QVERIFY(result->text().contains(QStringLiteral("Moved 1 unsubscribed message to Trash.")));
    QVERIFY(result->text().contains(QStringLiteral("Could not move 1 unsubscribed message to Trash.")));
    QVERIFY(result->text().contains(QStringLiteral("1 unsubscribe action failed.")));
    QVERIFY(result->detailedText().contains(QStringLiteral("List 105\nSynthetic Trash failure.")));
}
}

QTEST_MAIN(KMailUnsubscribe::UnsubscribeBatchTest)
#include "unsubscribebatchtest.moc"
