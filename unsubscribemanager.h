#ifndef _UNSUBSCRIBEMANAGER_H_
#define _UNSUBSCRIBEMANAGER_H_

#include <QObject>
#include <Akonadi/Item>
#include <KMime/KMimeMessage>
#include <MessageCore/MailingList>
#include <MessageViewer/DKIMManager>

#define LIST_UNSUBSCRIBE_POST_HDR "List-Unsubscribe-Post"
#define LIST_UNSUBSCRIBE_POST_VALUE "List-Unsubscribe=One-Click"

namespace MessageViewer
{
    class UnsubscribeManager : public QObject
    {
        Q_OBJECT
    public:
        explicit UnsubscribeManager(QObject *parent = nullptr);
        ~UnsubscribeManager() override;

        struct Result
        {
            enum _errorType
            {
                None = 0,
                NetworkError,
                SslError,
            } Type;
            QString ErrorString;
        };

        enum Status
        {
            /// @brief No unsubscribe method is available.
            None,
            /// @brief Unsubscribe is available, but not as one-click unsubscribe.
            NoOneClick,
            /**
             * @brief One-Click Unsubscribe is available, but DKIM didn't verify.
             *
             * Note: This state is considered non-compliant with RFC 8058.
             */
            InvalidOneClick,
            /// @brief One-Click Unsubscribe is available.
            ValidOneClick,
        };

        /**
         * @brief Sets the current message item.
         *
         * @param item The current message item.
         */
        void setMessageItem(const Akonadi::Item &item);

        /**
         * @brief Tests whether the current message item has been set.
         */
        bool hasMessage();

        /**
         * @brief Tests if the message has information to programmatically unsubscribe
         *
         * @return true if the message can be unsubscribed from.
         * @return false if the message cannot be unsubscribed from.
         */
        Status unsubscribeStatus();

        /**
         * @brief Performs a One-Click unsubscribe.
         */
        void doOneClick();

        /**
         * @brief Get the URL for One-Click Unsubscribe.
         *
         * @return QUrl The URL. If none was found, the URL will be empty.
         */
        QUrl oneClickUrl();

        /**
         * @brief Get the best Unsubscribe URL.
         * Unlike oneClickUrl, this will check for any usable Unsubscribe URL.
         * The search order is currently HTTPS, MAILTO, and HTTP URLs.
         *
         * @return QUrl The best available Unsubscribe URL, per the above heuristic
         */
        QUrl getUrl();

        /**
         * @brief Resets the object's state.
         * @remark This does not cancel any running One-Click Unsubscribe jobs.
         *
         */
        void reset();
    public slots:
        /**
         * @brief This is used as a slot for DKIMManager::result.
         * This signal can only be signalled once-- further signals will be
         * ignored.
         *
         * @param checkResult
         * @param id
         */
        void getDkimResult(const MessageViewer::DKIMCheckSignatureJob::CheckSignatureResult &checkResult, Akonadi::Item::Id id);

        void checkResult(const Result &result);

    signals:
        void oneClickResult(bool isSuccess, const QString &resultString);

    private:
        // message info
        KMime::Message::Ptr mMessage = nullptr;
        MessageCore::MailingList mList;
        Akonadi::Item::Id mItemId = -1;

        // Cached by oneClickUrl()
        QUrl mPostUrl;

        // Used to check DKIM, for RFC 8058 compliance
        DKIMManager mDkimMgr;
        bool mDKIMValid = false;
    };
}

#endif /* !_UNSUBSCRIBEMANAGER_H_ */