#include "rfc8058validator.h"

#include <QTest>

using namespace MessageViewer;

class Rfc8058ValidatorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void postHeaderValue_data();
    void postHeaderValue();
    void postUrl_data();
    void postUrl();
    void oneClickPostUrl_data();
    void oneClickPostUrl();
    void requiredHeaderPair_data();
    void requiredHeaderPair();
    void dkimStatus();
    void signedHeaders_data();
    void signedHeaders();
};

void Rfc8058ValidatorTest::postHeaderValue_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("valid");

    QTest::addRow("exact") << QStringLiteral("List-Unsubscribe=One-Click") << true;
    QTest::addRow("surrounding whitespace") << QStringLiteral(" \tList-Unsubscribe=One-Click \t") << true;
    QTest::addRow("empty") << QString() << false;
    QTest::addRow("wrong value") << QStringLiteral("List-Unsubscribe=No") << false;
    QTest::addRow("extra pair") << QStringLiteral("List-Unsubscribe=One-Click&action=delete") << false;
}

void Rfc8058ValidatorTest::postHeaderValue()
{
    QFETCH(QString, value);
    QFETCH(bool, valid);

    QCOMPARE(Rfc8058::isValidPostHeaderValue(value), valid);
}

void Rfc8058ValidatorTest::postUrl_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<bool>("valid");

    QTest::addRow("https") << QUrl(QStringLiteral("https://example.org/unsubscribe?id=opaque")) << true;
    QTest::addRow("http") << QUrl(QStringLiteral("http://example.org/unsubscribe")) << false;
    QTest::addRow("missing host") << QUrl(QStringLiteral("https:///unsubscribe")) << false;
    QTest::addRow("embedded credentials") << QUrl(QStringLiteral("https://user:password@example.org/unsubscribe")) << false;
}

void Rfc8058ValidatorTest::postUrl()
{
    QFETCH(QUrl, url);
    QFETCH(bool, valid);

    QCOMPARE(Rfc8058::isValidPostUrl(url), valid);
}

void Rfc8058ValidatorTest::oneClickPostUrl_data()
{
    QTest::addColumn<QList<QUrl>>("urls");
    QTest::addColumn<QUrl>("expected");

    const QUrl httpsUrl(QStringLiteral("https://example.org/unsubscribe?id=opaque"));
    QTest::addRow("one https") << QList<QUrl>{httpsUrl} << httpsUrl;
    QTest::addRow("https and mailto") << QList<QUrl>{QUrl(QStringLiteral("mailto:list@example.org")), httpsUrl} << httpsUrl;
    QTest::addRow("missing https") << QList<QUrl>{QUrl(QStringLiteral("mailto:list@example.org"))} << QUrl();
    QTest::addRow("two https") << QList<QUrl>{httpsUrl, QUrl(QStringLiteral("https://example.org/other"))} << QUrl();
    QTest::addRow("http companion") << QList<QUrl>{httpsUrl, QUrl(QStringLiteral("http://example.org/other"))} << QUrl();
}

void Rfc8058ValidatorTest::oneClickPostUrl()
{
    QFETCH(QList<QUrl>, urls);
    QFETCH(QUrl, expected);

    QCOMPARE(Rfc8058::oneClickPostUrl(urls), expected);
}

void Rfc8058ValidatorTest::requiredHeaderPair_data()
{
    QTest::addColumn<QStringList>("headers");
    QTest::addColumn<bool>("valid");

    QTest::addRow("one pair") << QStringList{QStringLiteral("From"), QStringLiteral("List-Unsubscribe"), QStringLiteral("List-Unsubscribe-Post")} << true;
    QTest::addRow("case insensitive") << QStringList{QStringLiteral("list-unsubscribe"), QStringLiteral("LIST-UNSUBSCRIBE-POST")} << true;
    QTest::addRow("duplicate unsubscribe") << QStringList{QStringLiteral("List-Unsubscribe"), QStringLiteral("List-Unsubscribe"), QStringLiteral("List-Unsubscribe-Post")} << false;
    QTest::addRow("duplicate post") << QStringList{QStringLiteral("List-Unsubscribe"), QStringLiteral("List-Unsubscribe-Post"), QStringLiteral("List-Unsubscribe-Post")} << false;
    QTest::addRow("missing post") << QStringList{QStringLiteral("List-Unsubscribe")} << false;
}

void Rfc8058ValidatorTest::requiredHeaderPair()
{
    QFETCH(QStringList, headers);
    QFETCH(bool, valid);

    QCOMPARE(Rfc8058::hasExactlyOneRequiredHeaderPair(headers), valid);
}

void Rfc8058ValidatorTest::dkimStatus()
{
    using Status = DKIMCheckSignatureJob::DKIMStatus;

    QVERIFY(Rfc8058::isCryptographicallyValid(Status::Valid));
    QVERIFY(!Rfc8058::isCryptographicallyValid(Status::Unknown));
    QVERIFY(!Rfc8058::isCryptographicallyValid(Status::Invalid));
    QVERIFY(!Rfc8058::isCryptographicallyValid(Status::EmailNotSigned));
    QVERIFY(!Rfc8058::isCryptographicallyValid(Status::NeedToBeSigned));
}

void Rfc8058ValidatorTest::signedHeaders_data()
{
    QTest::addColumn<QStringList>("headers");
    QTest::addColumn<bool>("valid");

    QTest::addRow("both") << QStringList{QStringLiteral("from"), QStringLiteral("list-unsubscribe"), QStringLiteral("list-unsubscribe-post")} << true;
    QTest::addRow("mixed case") << QStringList{QStringLiteral("List-Unsubscribe"), QStringLiteral("LIST-UNSUBSCRIBE-POST")} << true;
    QTest::addRow("missing post") << QStringList{QStringLiteral("from"), QStringLiteral("list-unsubscribe")} << false;
    QTest::addRow("missing unsubscribe") << QStringList{QStringLiteral("from"), QStringLiteral("list-unsubscribe-post")} << false;
}

void Rfc8058ValidatorTest::signedHeaders()
{
    QFETCH(QStringList, headers);
    QFETCH(bool, valid);

    QCOMPARE(Rfc8058::signatureCoversRequiredHeaders(headers), valid);
}

QTEST_GUILESS_MAIN(Rfc8058ValidatorTest)

#include "rfc8058validatortest.moc"
