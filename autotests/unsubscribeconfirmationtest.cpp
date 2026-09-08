#include "unsubscribeconfirmation.h"
#include "unsubscribeplan.h"

#include <KLocalizedString>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <QTreeWidget>

using namespace KMailUnsubscribe;
using Method = UnsubscribeWorkflow::Method;
using Capabilities = UnsubscribeCapabilities;
using PlanEntry = UnsubscribePlanEntry;
Q_DECLARE_METATYPE(QList<PlanEntry>)

class UnsubscribeConfirmationTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void preferredMethod_data();
    void preferredMethod();
    void fallbackSequence();
    void dialogOptions_data();
    void dialogOptions();
    void confirmationRecordsConsent();
    void deletePreferenceForWholeSelection_data();
    void deletePreferenceForWholeSelection();
    void listsEveryMessageAndItsMethod();
    void noMethodsShowsAnError();
    void loadFailuresAreDistinguished();
};

void UnsubscribeConfirmationTest::initTestCase()
{
    KLocalizedString::setApplicationDomain("kmail_unsubscribe");
    KLocalizedString::setLanguages({QStringLiteral("en_US")});
}

void UnsubscribeConfirmationTest::preferredMethod_data()
{
    QTest::addColumn<bool>("email");
    QTest::addColumn<bool>("web");
    QTest::addColumn<bool>("oneClick");
    QTest::addColumn<int>("method");
    QTest::addRow("one-click-over-web-and-email") << true << true << true << int(Method::OneClick);
    QTest::addRow("one-click-over-web") << false << true << true << int(Method::OneClick);
    QTest::addRow("web-over-email") << true << true << false << int(Method::Web);
    QTest::addRow("web-only") << false << true << false << int(Method::Web);
    QTest::addRow("email-only") << true << false << false << int(Method::Email);
    QTest::addRow("none") << false << false << false << -1;
    QTest::addRow("one-click-needs-web") << false << false << true << -1;
    QTest::addRow("email-without-a-one-click-url") << true << false << true << int(Method::Email);
}

void UnsubscribeConfirmationTest::preferredMethod()
{
    QFETCH(bool, email);
    QFETCH(bool, web);
    QFETCH(bool, oneClick);
    QFETCH(int, method);
    const auto actual = unsubscribeMethod({email, web, oneClick});
    QCOMPARE(actual ? int(*actual) : -1, method);
}

void UnsubscribeConfirmationTest::fallbackSequence()
{
    const auto allMethods = unsubscribeMethods({true, true, true});
    QCOMPARE(allMethods.size(), 3);
    QCOMPARE(int(allMethods.at(0)), int(Method::OneClick));
    QCOMPARE(int(allMethods.at(1)), int(Method::Web));
    QCOMPARE(int(allMethods.at(2)), int(Method::Email));

    const auto webAndEmail = unsubscribeMethods({true, true, false});
    QCOMPARE(webAndEmail.size(), 2);
    QCOMPARE(int(webAndEmail.at(0)), int(Method::Web));
    QCOMPARE(int(webAndEmail.at(1)), int(Method::Email));
}

void UnsubscribeConfirmationTest::dialogOptions_data()
{
    QTest::addColumn<QList<PlanEntry>>("plan");
    QTest::addColumn<bool>("available");
    QTest::addRow("one-click") << QList<PlanEntry>{{"List A", Method::OneClick}} << true;
    QTest::addRow("web") << QList<PlanEntry>{{"List A", Method::Web}} << true;
    QTest::addRow("email") << QList<PlanEntry>{{"List A", Method::Email}} << true;
    QTest::addRow("mixed") << QList<PlanEntry>{{"List A", Method::OneClick}, {"List B", Method::Web}, {"List C", Method::Email}} << true;
    QTest::addRow("mixed-with-skipped") << QList<PlanEntry>{{"List A", Method::Web}, {"Other message", std::nullopt}} << true;
    QTest::addRow("none") << QList<PlanEntry>{{"Other message", std::nullopt}} << false;
    QTest::addRow("load-failed") << QList<PlanEntry>{{"Other message", std::nullopt, true}} << false;
    QTest::addRow("invalid-email") << QList<PlanEntry>{{"Other message", std::nullopt, false, true}} << false;
    QTest::addRow("empty") << QList<PlanEntry>{} << false;
}

void UnsubscribeConfirmationTest::dialogOptions()
{
    QFETCH(QList<PlanEntry>, plan);
    QFETCH(bool, available);
    UnsubscribeConfirmation dialog(plan);
    QCOMPARE(bool(dialog.findChild<QPushButton *>(QStringLiteral("unsubscribeConfirm"))), available);
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("unsubscribeEmail")));
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("unsubscribeWeb")));
    const auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("unsubscribeButtons"));
    QVERIFY(buttons);
    QCOMPARE(bool(buttons->button(QDialogButtonBox::Cancel)), available);
    QCOMPARE(bool(buttons->button(QDialogButtonBox::Ok)), !available);
    QVERIFY(!dialog.confirmed());
    QVERIFY(!dialog.deleteAfterSuccess());
    const auto checkboxes = dialog.findChildren<QCheckBox *>();
    QCOMPARE(checkboxes.size(), available ? 1 : 0);
    if (available)
    {
        QVERIFY(checkboxes.constFirst()->isChecked());
    }
    dialog.show();
    if (available)
    {
        QVERIFY(buttons->button(QDialogButtonBox::Cancel)->isDefault());
    }
    QTest::keyClick(&dialog, Qt::Key_Escape);
    QVERIFY(!dialog.confirmed());
    QVERIFY(!dialog.deleteAfterSuccess());
}

void UnsubscribeConfirmationTest::confirmationRecordsConsent()
{
    const QList<PlanEntry> plan = {{"List A", Method::OneClick}, {"List B", Method::Web}, {"List C", Method::Email}};
    UnsubscribeConfirmation dialog(plan);
    QVERIFY(!dialog.confirmed());
    auto *confirm = dialog.findChild<QPushButton *>(QStringLiteral("unsubscribeConfirm"));
    QVERIFY(confirm);
    // This dialog records consent only; the test never executes the plan.
    confirm->click();
    QVERIFY(dialog.confirmed());

    UnsubscribeConfirmation canceled(plan);
    canceled.findChild<QPushButton *>(QStringLiteral("unsubscribeCancel"))->click();
    QVERIFY(!canceled.confirmed());
}

void UnsubscribeConfirmationTest::deletePreferenceForWholeSelection_data()
{
    QTest::addColumn<int>("messageCount");
    QTest::addColumn<bool>("checked");
    QTest::addColumn<bool>("accept");
    QTest::addRow("single-default") << 1 << true << true;
    QTest::addRow("single-keep") << 1 << false << true;
    QTest::addRow("single-cancel") << 1 << true << false;
    QTest::addRow("mixed-default") << 3 << true << true;
    QTest::addRow("mixed-keep") << 3 << false << true;
    QTest::addRow("mixed-cancel") << 3 << true << false;
}

void UnsubscribeConfirmationTest::deletePreferenceForWholeSelection()
{
    QFETCH(int, messageCount);
    QFETCH(bool, checked);
    QFETCH(bool, accept);
    QList<PlanEntry> plan = {{"List A", Method::OneClick}};
    if (messageCount > 1)
    {
        plan.append({"List B", Method::Web});
        plan.append({"List C", Method::Email});
    }
    UnsubscribeConfirmation dialog(plan);
    const auto boxes = dialog.findChildren<QCheckBox *>();
    QCOMPARE(boxes.size(), 1);
    auto *checkbox = boxes.constFirst();
    QCOMPARE(checkbox->text(), QStringLiteral("Delete after successful unsubscribe"));
    QVERIFY(checkbox->isChecked());
    checkbox->setChecked(checked);
    // The checked default alone is never consent to delete.
    QVERIFY(!dialog.deleteAfterSuccess());
    dialog.findChild<QPushButton *>(accept ? QStringLiteral("unsubscribeConfirm") : QStringLiteral("unsubscribeCancel"))->click();
    QCOMPARE(dialog.deleteAfterSuccess(), accept && checked);

    // This is a per-operation choice, not a saved preference for another batch.
    UnsubscribeConfirmation nextDialog(plan);
    QVERIFY(nextDialog.findChild<QCheckBox *>()->isChecked());
    QVERIFY(!nextDialog.deleteAfterSuccess());
}

void UnsubscribeConfirmationTest::listsEveryMessageAndItsMethod()
{
    const QList<PlanEntry> plan = {
        {"<b>Same subject</b>", Method::OneClick, false, false, {Method::Web, Method::Email}},
        {"<b>Same subject</b>", Method::Web},
        {"Mail-only list", Method::Email},
        {"Ordinary message", std::nullopt},
        {"Offline message", std::nullopt, true},
        {"Invalid email list", std::nullopt, false, true},
    };
    UnsubscribeConfirmation dialog(plan);
    const auto *messages = dialog.findChild<QTreeWidget *>(QStringLiteral("unsubscribePlan"));
    QVERIFY(messages);
    QCOMPARE(messages->topLevelItemCount(), plan.size());
    QCOMPARE(messages->selectionMode(), QAbstractItemView::NoSelection);
    QCOMPARE(messages->editTriggers(), QAbstractItemView::NoEditTriggers);
    const QStringList methods = {
        "One-click → Web → Email",
        "Web",
        "Email",
        "Skipped — unavailable",
        "Skipped — could not load",
        "Skipped — invalid email address",
    };
    for (qsizetype i = 0; i < plan.size(); ++i)
    {
        const auto *row = messages->topLevelItem(i);
        QCOMPARE(row->text(0), plan[i].subject);
        QCOMPARE(row->text(1), methods[i]);
    }
    const auto *question = dialog.findChild<QLabel *>(QStringLiteral("unsubscribeQuestion"));
    QVERIFY(question);
    QCOMPARE(question->textFormat(), Qt::PlainText);
    QVERIFY(question->text().contains(QString::number(plan.size())));
}

void UnsubscribeConfirmationTest::noMethodsShowsAnError()
{
    UnsubscribeConfirmation dialog({{"Ordinary message", std::nullopt}});
    const auto *question = dialog.findChild<QLabel *>(QStringLiteral("unsubscribeQuestion"));
    QVERIFY(question->text().contains(QStringLiteral("No unsubscribe method")));
    const auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("unsubscribeButtons"));
    QVERIFY(buttons->button(QDialogButtonBox::Ok));
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("unsubscribeConfirm")));
    buttons->button(QDialogButtonBox::Ok)->click();
    QVERIFY(!dialog.confirmed());
    QVERIFY(!dialog.deleteAfterSuccess());
}

void UnsubscribeConfirmationTest::loadFailuresAreDistinguished()
{
    UnsubscribeConfirmation dialog({{"Offline message", std::nullopt, true}});
    const auto *question = dialog.findChild<QLabel *>(QStringLiteral("unsubscribeQuestion"));
    QVERIFY(question->text().contains(QStringLiteral("could not be loaded")));
    QVERIFY(!dialog.confirmed());
}

QTEST_MAIN(UnsubscribeConfirmationTest)
#include "unsubscribeconfirmationtest.moc"
