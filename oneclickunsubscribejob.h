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
        void slotError(QNetworkReply::NetworkError error);

    signals:
        /**
         * @brief Triggered on job completion/failure.
         *
         * @param success Whether the job was successful. If true, sslError and error should be ignored.
         * @param sslError If unsuccessful, whether one or more SSL errors occurred.
         * @param error If unsuccessful, the error code returned by the QNetworkRequest.
         */
        void result(const UnsubscribeManager::Result &data);

    private:
        QNetworkAccessManager *const mNetworkAccessManager;
        QUrl mUrl;
        bool sentResult = false;
        QNetworkReply *mReply = nullptr;
    };
}

#endif /* !_ONECLICKUNSUBSCRIBEJOB_H_ */