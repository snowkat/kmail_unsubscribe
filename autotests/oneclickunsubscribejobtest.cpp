#include "oneclickunsubscribejob.h"

#include <KLocalizedString>
#include <QNetworkProxy>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace MessageViewer;

class OneClickUnsubscribeJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void httpResponse_data();
    void httpResponse();
    void networkFailureDoesNotExposeTarget();
};

void OneClickUnsubscribeJobTest::initTestCase()
{
    KLocalizedString::setApplicationDomain("kmail_unsubscribe");
    KLocalizedString::setLanguages({QStringLiteral("en_US")});
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
}

void OneClickUnsubscribeJobTest::httpResponse_data()
{
    QTest::addColumn<int>("status");
    QTest::addRow("ok") << 200;
    QTest::addRow("no-content") << 204;
    QTest::addRow("redirect") << 302;
    QTest::addRow("not-found") << 404;
    QTest::addRow("gone") << 410;
    QTest::addRow("rate-limited") << 429;
    QTest::addRow("server-error") << 500;
}

void OneClickUnsubscribeJobTest::httpResponse()
{
    QFETCH(int, status);
    QByteArray request;
    int requestCount = 0;
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        while (server.hasPendingConnections())
        {
            auto *socket = server.nextPendingConnection();
            ++requestCount;
            auto incoming = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, incoming]() {
                incoming->append(socket->readAll());
                const qsizetype end = incoming->indexOf("\r\n\r\n");
                if (end < 0 || socket->property("responded").toBool())
                {
                    return;
                }
                qint64 length = -1;
                for (const auto &line : incoming->left(end).split('\n'))
                {
                    if (line.toLower().startsWith("content-length:"))
                    {
                        length = line.mid(sizeof("content-length:") - 1).trimmed().toLongLong();
                    }
                }
                if (length < 0 || incoming->size() < end + 4 + length)
                {
                    return;
                }
                socket->setProperty("responded", true);
                request = *incoming;
                QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + " Test response\r\n"
                    "Content-Length: 0\r\nConnection: close\r\n";
                if (status == 302)
                {
                    response += "Location: /must-not-follow\r\n";
                }
                socket->write(response + "\r\n");
                socket->disconnectFromHost();
            });
        }
    });

    // Exercise the HTTP transport only, using a loopback server and synthetic
    // tokens. The production workflow separately requires verified HTTPS URLs.
    const QByteArray target = "/unsubscribe/private-token%2Fexample?list=test%2Bvalue";
    QUrl url = QUrl::fromEncoded("http://127.0.0.1:" + QByteArray::number(server.serverPort()) + target);
    UnsubscribeManager manager;
    QSignalSpy finished(&manager, &UnsubscribeManager::oneClickResult);
    QPointer<OneClickUnsubscribeJob> job = new OneClickUnsubscribeJob(url, &manager);
    UnsubscribeManager::Result received{UnsubscribeManager::Result::None, {}};
    connect(job, &OneClickUnsubscribeJob::result, &manager, [&](const auto &result) { received = result; });
    connect(job, &OneClickUnsubscribeJob::result, &manager, &UnsubscribeManager::checkResult);
    job->start();
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(job.isNull(), 1000);
    QTest::qWait(50);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(requestCount, 1); // Including 302: no redirect or fallback request.

    QVERIFY(request.startsWith("POST " + target + " HTTP/1.1\r\n"));
    const qsizetype end = request.indexOf("\r\n\r\n");
    const QByteArray headers = request.left(end).toLower();
    QVERIFY(headers.contains("content-type: multipart/form-data; boundary="));
    QVERIFY(!headers.contains("\r\ncookie:"));
    QVERIFY(!headers.contains("\r\nauthorization:"));
    const QByteArray body = request.mid(end + 4);
    // MIME header names are case-insensitive; the form field and its value
    // must retain the exact spelling required by RFC 8058.
    const QByteArray disposition = "content-disposition:";
    QVERIFY2(body.toLower().count(disposition) == 1, body.constData());
    const qsizetype valueStart = body.toLower().indexOf(disposition) + disposition.size();
    QVERIFY2(body.mid(valueStart).startsWith(" form-data; name=\"List-Unsubscribe\"\r\n\r\nOne-Click\r\n"), body.constData());

    const bool success = status >= 200 && status < 300;
    QCOMPARE(finished.first().first().toBool(), success);
    QCOMPARE(received.Type, success ? UnsubscribeManager::Result::None : UnsubscribeManager::Result::HttpError);
    const QString error = finished.first().at(1).toString();
    if (success)
    {
        QVERIFY(error.isEmpty());
    }
    else
    {
        QCOMPARE(received.HttpStatus, status);
        QCOMPARE(received.ServerHost, QStringLiteral("127.0.0.1"));
        QVERIFY(error.contains(QStringLiteral("HTTP %1").arg(status)));
        QVERIFY(error.contains(QStringLiteral("not confirmed")));
        QVERIFY(!error.contains(QStringLiteral("private-token")));
        QVERIFY(!error.contains(QStringLiteral("test%2Bvalue")));
        if (status == 404)
        {
            QVERIFY(error.contains(QStringLiteral("not found")));
            QVERIFY(error.contains(QStringLiteral("may be outdated")));
        }
    }
}

void OneClickUnsubscribeJobTest::networkFailureDoesNotExposeTarget()
{
    // Reserve a local port, then release it without accepting connections.
    // The resulting transport failure exercises the privacy-safe error path.
    QTcpServer reservation;
    QVERIFY(reservation.listen(QHostAddress::LocalHost));
    const quint16 port = reservation.serverPort();
    reservation.close();

    QUrl url = QUrl::fromEncoded("http://127.0.0.1:" + QByteArray::number(port)
                                 + "/unsubscribe/private-token%2Fexample?list=test%2Bvalue");
    UnsubscribeManager manager;
    QSignalSpy finished(&manager, &UnsubscribeManager::oneClickResult);
    QPointer<OneClickUnsubscribeJob> job = new OneClickUnsubscribeJob(url, &manager);
    UnsubscribeManager::Result received{UnsubscribeManager::Result::None, {}};
    connect(job, &OneClickUnsubscribeJob::result, &manager, [&](const auto &result) { received = result; });
    connect(job, &OneClickUnsubscribeJob::result, &manager, &UnsubscribeManager::checkResult);
    job->start();

    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QCOMPARE(received.Type, UnsubscribeManager::Result::NetworkError);
    QCOMPARE(received.ServerHost, QStringLiteral("127.0.0.1"));
    const QString error = finished.first().at(1).toString();
    QVERIFY(error.contains(QStringLiteral("could not be reached")));
    QVERIFY(error.contains(QStringLiteral("not confirmed")));
    QVERIFY(!error.contains(QStringLiteral("private-token")));
    QVERIFY(!error.contains(QStringLiteral("test%2Bvalue")));
}

QTEST_GUILESS_MAIN(OneClickUnsubscribeJobTest)

#include "oneclickunsubscribejobtest.moc"
