#include "unsubscribecomposerrequest.h"
#include "unsubscribecomposerrequeststore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class UnsubscribeComposerRequestTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip_data();
    void roundTrip();
    void rejectsUnmarkedText();
    void deletionConsentIsLocalAndSingleUse();
    void unrecognizedOrInvalidConsentKeepsOriginal();
};

void UnsubscribeComposerRequestTest::roundTrip_data()
{
    QTest::addColumn<QString>("body");

    QTest::addRow("empty") << QString();
    QTest::addRow("ascii") << QStringLiteral("Please unsubscribe this address.");
    QTest::addRow("unicode multiline") << QStringLiteral("Zażółć\n日本語\n");
}

void UnsubscribeComposerRequestTest::roundTrip()
{
    QFETCH(QString, body);

    const QString marker = KMailUnsubscribe::makeComposerRequest(body);
    const auto decodedMarker = KMailUnsubscribe::bodyFromComposerRequest(marker);
    QVERIFY(decodedMarker);
    QCOMPARE(*decodedMarker, body);

    const auto decodedSurroundedMarker =
        KMailUnsubscribe::bodyFromComposerRequest(QStringLiteral("signature before\n") + marker + QStringLiteral("\nsignature after"));
    QVERIFY(decodedSurroundedMarker);
    QCOMPARE(*decodedSurroundedMarker, body);
}

void UnsubscribeComposerRequestTest::rejectsUnmarkedText()
{
    QVERIFY(!KMailUnsubscribe::bodyFromComposerRequest(QStringLiteral("normal message")));
    QVERIFY(!KMailUnsubscribe::bodyFromComposerRequest(KMailUnsubscribe::composerRequestPrefix()
                                                       + QStringLiteral("not-a-uuid:Ym9keQ")
                                                       + KMailUnsubscribe::composerRequestSuffix()));
    QVERIFY(!KMailUnsubscribe::bodyFromComposerRequest(KMailUnsubscribe::composerRequestPrefix()
                                                       + QUuid::createUuid().toString(QUuid::WithoutBraces)
                                                       + QStringLiteral(":not+base64")
                                                       + KMailUnsubscribe::composerRequestSuffix()));
}

void UnsubscribeComposerRequestTest::deletionConsentIsLocalAndSingleUse()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    KMailUnsubscribe::ComposerRequestStore store(temporary.path() + QStringLiteral("/requests"));
    const auto request = KMailUnsubscribe::composerRequestFromText(KMailUnsubscribe::makeComposerRequest(QString()));
    QVERIFY(request);
    // A valid body marker alone carries no deletion authority.
    QVERIFY(!store.take(request->token));
    QVERIFY(store.remember(request->token, {1234567890123456789LL, QStringLiteral("Original list message")}));
    const auto consent = store.take(request->token);
    QVERIFY(consent);
    QCOMPARE(consent->originalId, 1234567890123456789LL);
    QCOMPARE(consent->subject, QStringLiteral("Original list message"));
    QVERIFY(!store.take(request->token));
    QCOMPARE(request->body, QString());
}

void UnsubscribeComposerRequestTest::unrecognizedOrInvalidConsentKeepsOriginal()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    KMailUnsubscribe::ComposerRequestStore store(temporary.path());
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVERIFY(!store.remember(QStringLiteral("../outside"), {101, QString()}));
    QVERIFY(!store.take(QStringLiteral("../outside")));
    QVERIFY(!store.remember(token, {-1, QString()}));
    QVERIFY(!store.take(token));
    QVERIFY(store.remember(token, {101, QString()}));
    store.discard(token); // A failed composer launch leaves no consent behind.
    QVERIFY(!store.take(token));
    QFile corrupt(temporary.filePath(token + QStringLiteral(".json")));
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    corrupt.write("{\"originalId\":\"not-an-item-id\"}");
    corrupt.close();
    QVERIFY(!store.take(token));
}

QTEST_GUILESS_MAIN(UnsubscribeComposerRequestTest)

#include "unsubscribecomposerrequesttest.moc"
