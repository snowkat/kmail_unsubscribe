#include "webunsubscribeprobe.h"

#include <QNetworkProxy>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <memory>

using namespace KMailUnsubscribe;

class WebUnsubscribeProbeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void httpResponse_data();
    void httpResponse();
};

void WebUnsubscribeProbeTest::initTestCase()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
}

void WebUnsubscribeProbeTest::httpResponse_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("contentType");
    QTest::addColumn<bool>("shouldOpenInBrowser");
    QTest::addColumn<bool>("jsonResponse");

    QTest::addRow("html") << 200 << QByteArrayLiteral("text/html; charset=utf-8") << true << false;
    QTest::addRow("json") << 200 << QByteArrayLiteral("application/json") << false << true;
    QTest::addRow("problem-json") << 200 << QByteArrayLiteral("application/problem+json") << false << true;
    QTest::addRow("redirect") << 302 << QByteArray() << true << false;
    QTest::addRow("not-found") << 404 << QByteArrayLiteral("application/json") << false << true;
    QTest::addRow("gone") << 410 << QByteArray() << false << false;
    QTest::addRow("server-error") << 500 << QByteArrayLiteral("text/html") << true << false;
}

void WebUnsubscribeProbeTest::httpResponse()
{
    QFETCH(int, status);
    QFETCH(QByteArray, contentType);
    QFETCH(bool, shouldOpenInBrowser);
    QFETCH(bool, jsonResponse);

    QByteArray request;
    int requestCount = 0;
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        auto *socket = server.nextPendingConnection();
        ++requestCount;
        auto incoming = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket, incoming]() {
            incoming->append(socket->readAll());
            if (!incoming->contains("\r\n\r\n") || socket->property("responded").toBool())
            {
                return;
            }
            socket->setProperty("responded", true);
            request = *incoming;
            QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + " Test response\r\n"
                "Content-Length: 0\r\nConnection: close\r\n";
            if (!contentType.isEmpty())
            {
                response += "Content-Type: " + contentType + "\r\n";
            }
            if (status == 302)
            {
                response += "Location: /interactive-page\r\n";
            }
            socket->write(response + "\r\n");
            socket->disconnectFromHost();
        });
    });

    const QByteArray target = "/unsubscribe/private-token%2Fexample?list=test%2Bvalue";
    QUrl url = QUrl::fromEncoded("http://127.0.0.1:" + QByteArray::number(server.serverPort()) + target);
    QPointer<WebUnsubscribeProbe> probe = new WebUnsubscribeProbe(url, this);
    QSignalSpy finished(probe, &WebUnsubscribeProbe::finished);
    probe->start();

    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(probe.isNull(), 1000);
    QCOMPARE(requestCount, 1);
    QVERIFY(request.startsWith("HEAD " + target + " HTTP/1.1\r\n"));
    QVERIFY(request.endsWith("\r\n\r\n"));
    const QByteArray headers = request.left(request.indexOf("\r\n\r\n")).toLower();
    QVERIFY(!headers.contains("\r\ncookie:"));
    QVERIFY(!headers.contains("\r\nauthorization:"));
    QCOMPARE(finished.first().at(0).toBool(), shouldOpenInBrowser);
    QCOMPARE(finished.first().at(1).toInt(), status);
    QCOMPARE(finished.first().at(2).toBool(), jsonResponse);
}

QTEST_GUILESS_MAIN(WebUnsubscribeProbeTest)

#include "webunsubscribeprobetest.moc"
