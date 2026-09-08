#include "unsubscribeworkflow.h"

#include <Akonadi/Item>
#include <KMime/Message>
#include <QSignalSpy>
#include <QTest>

class UnsubscribeWorkflowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void capabilities_data();
    void capabilities();
    void sameItemWithCompleteHeaders();
    void malformedEmailIsUnavailable();
    void malformedListUnsubscribeKeepsMailtoFallback();
    void oneClickDoesNotDowngradeToWeb();
};

namespace
{
Akonadi::Item makeItem(Akonadi::Item::Id id, const QByteArray &unsubscribeHeaders)
{
    const QByteArray content = QByteArrayLiteral("From: sender@example.org\r\n")
        + QByteArrayLiteral("To: recipient@example.org\r\n")
        + QByteArrayLiteral("Subject: List message\r\n")
        + unsubscribeHeaders
        + QByteArrayLiteral("MIME-Version: 1.0\r\nContent-Type: text/plain\r\n\r\nBody\r\n");
    auto message = std::make_shared<KMime::Message>();
    message->setContent(content);
    message->parse();

    Akonadi::Item item(id);
    item.setMimeType(QStringLiteral("message/rfc822"));
    item.setPayload(message);
    return item;
}
}

void UnsubscribeWorkflowTest::capabilities_data()
{
    QTest::addColumn<QByteArray>("headers");
    QTest::addColumn<bool>("email");
    QTest::addColumn<bool>("web");
    QTest::addColumn<bool>("oneClickCandidate");

    QTest::addRow("none") << QByteArray() << false << false << false;
    QTest::addRow("email") << QByteArrayLiteral("List-Unsubscribe: <mailto:list-unsubscribe@example.org>\r\n") << true << false << false;
    QTest::addRow("web") << QByteArrayLiteral("List-Unsubscribe: <https://example.org/unsubscribe/token>\r\n") << false << true << false;
    QTest::addRow("both")
        << QByteArrayLiteral("List-Unsubscribe: <mailto:list-unsubscribe@example.org>, <https://example.org/unsubscribe/token>\r\n")
        << true << true << false;
    QTest::addRow("one-click-candidate")
        << QByteArrayLiteral("List-Unsubscribe: <https://example.org/unsubscribe/token>\r\n"
                             "List-Unsubscribe-Post: List-Unsubscribe=One-Click\r\n")
        << false << true << true;
}

void UnsubscribeWorkflowTest::capabilities()
{
    QFETCH(QByteArray, headers);
    QFETCH(bool, email);
    QFETCH(bool, web);
    QFETCH(bool, oneClickCandidate);

    KMailUnsubscribe::UnsubscribeWorkflow workflow;
    workflow.setMessageItem(makeItem(42, headers), false);

    QCOMPARE(workflow.emailAvailable(), email);
    QCOMPARE(workflow.webAvailable(), web);
    QCOMPARE(workflow.anyMethodAvailable(), email || web);
    QCOMPARE(workflow.hasOneClickCandidate(), oneClickCandidate);
    QCOMPARE(workflow.webCapability(), web
                 ? KMailUnsubscribe::UnsubscribeWorkflow::WebCapability::Regular
                 : KMailUnsubscribe::UnsubscribeWorkflow::WebCapability::None);

    const auto emailMethod = workflow.resolvedMethod(KMailUnsubscribe::UnsubscribeWorkflow::Method::Email);
    const auto webMethod = workflow.resolvedMethod(KMailUnsubscribe::UnsubscribeWorkflow::Method::Web);
    QCOMPARE(emailMethod.has_value(), email);
    QCOMPARE(webMethod.has_value(), web);
    if (email)
    {
        QCOMPARE(*emailMethod, KMailUnsubscribe::UnsubscribeWorkflow::Method::Email);
    }
    if (web)
    {
        QCOMPARE(*webMethod, KMailUnsubscribe::UnsubscribeWorkflow::Method::Web);
    }
}

void UnsubscribeWorkflowTest::sameItemWithCompleteHeaders()
{
    KMailUnsubscribe::UnsubscribeWorkflow workflow;
    workflow.setMessageItem(makeItem(42, {}), false);
    QVERIFY(!workflow.anyMethodAvailable());
    workflow.setMessageItem(makeItem(42, QByteArrayLiteral("List-Unsubscribe: <mailto:leave@example.org>\r\n")), false);
    QVERIFY(workflow.emailAvailable());
    QVERIFY(!workflow.webAvailable());
    workflow.setMessageItem(makeItem(42, {}), false);
    QVERIFY(!workflow.anyMethodAvailable());
}

void UnsubscribeWorkflowTest::malformedEmailIsUnavailable()
{
    KMailUnsubscribe::UnsubscribeWorkflow workflow;
    workflow.setMessageItem(makeItem(42, QByteArrayLiteral("List-Unsubscribe: <mailto:unsubscribe>\r\n")), false);

    QVERIFY(workflow.emailAdvertised());
    QVERIFY(!workflow.emailAvailable());
    QVERIFY(!workflow.anyMethodAvailable());
    QVERIFY(!workflow.emailValidationError().isEmpty());
}

void UnsubscribeWorkflowTest::malformedListUnsubscribeKeepsMailtoFallback()
{
    // MessageCore's generic List-Unsubscribe parser treats the second URI as
    // part of the malformed first one. The plugin must still find this
    // explicitly bracketed RFC 2369 mailto fallback without using sender data.
    KMailUnsubscribe::UnsubscribeWorkflow workflow;
    workflow.setMessageItem(makeItem(42,
                                     QByteArrayLiteral("List-Unsubscribe: <https://example.org/broken <mailto:leave@example.org>\r\n")),
                            false);

    QVERIFY(workflow.emailAdvertised());
    QVERIFY(workflow.emailAvailable());
    const auto emailMethod = workflow.resolvedMethod(KMailUnsubscribe::UnsubscribeWorkflow::Method::Email);
    QVERIFY(emailMethod.has_value());
    QCOMPARE(*emailMethod, KMailUnsubscribe::UnsubscribeWorkflow::Method::Email);
}

void UnsubscribeWorkflowTest::oneClickDoesNotDowngradeToWeb()
{
    KMailUnsubscribe::UnsubscribeWorkflow workflow;
    workflow.setMessageItem(makeItem(42, QByteArrayLiteral("List-Unsubscribe: <https://example.org/leave>\r\n"
                                                          "List-Unsubscribe-Post: List-Unsubscribe=One-Click\r\n")), false);
    QVERIFY(workflow.webAvailable());
    QSignalSpy finished(&workflow, &KMailUnsubscribe::UnsubscribeWorkflow::finished);
    workflow.execute(KMailUnsubscribe::UnsubscribeWorkflow::Method::OneClick);
    QCOMPARE(finished.size(), 1);
    QVERIFY(!finished.first().first().toBool());
}

QTEST_GUILESS_MAIN(UnsubscribeWorkflowTest)

#include "unsubscribeworkflowtest.moc"
