#ifndef _ONECLICKUNSUBSCRIBEJOB_H_
#define _ONECLICKUNSUBSCRIBEJOB_H_

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "unsubscribemanager.h"

namespace MessageViewer
{
    class OneClickUnsubscribeJob : public QObject
    {
        Q_OBJECT
    public:
        explicit OneClickUnsubscribeJob(QUrl &oneClickUrl, UnsubscribeManager *parent);
        ~OneClickUnsubscribeJob() = default;

        /**
         * @brief Starts the unsubscribe job.
         *
         */
        void start();

        // Slots are used for connection to mNetworkAccessManager
    public slots:
        void slotFinished(QNetworkReply *reply);
        void slotSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

    signals:
        /**
         * @brief Triggered on job completion/failure.
         *
         * @param data Success, HTTP rejection, or transport failure details.
         */
        void result(const UnsubscribeManager::Result &data);

    private:
        void reportResult(const UnsubscribeManager::Result &result);

        QNetworkAccessManager *const mNetworkAccessManager;
        QUrl mUrl;
        bool sentResult = false;
    };
}

#endif /* !_ONECLICKUNSUBSCRIBEJOB_H_ */
