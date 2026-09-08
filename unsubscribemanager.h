#ifndef _UNSUBSCRIBEMANAGER_H_
#define _UNSUBSCRIBEMANAGER_H_

#include <QObject>
#include <Akonadi/Item>
#include <KMime/Message>
#include <MessageCore/MailingList>
#include <MessageViewer/DKIMManager>
#include <QList>
#include <QTimer>

#include <memory>

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
                HttpError,
            } Type;
            QString ErrorString;
            int HttpStatus = 0;
            QString ServerHost;
        };

        enum Status
        {
            /// @brief No unsubscribe method is available.
            None,
            /// @brief Unsubscribe is available, but not as one-click unsubscribe.
            NoOneClick,
            /// @brief One-click headers are present and DKIM verification is running.
            CheckingOneClick,
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
        void setMessageItem(const Akonadi::Item &item, bool verifyOneClick = true);

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
        [[nodiscard]] bool doOneClick();

        /**
         * @brief Get the URL for One-Click Unsubscribe.
         *
         * @return QUrl The URL. If none was found, the URL will be empty.
         */
        QUrl oneClickUrl();

        /**
         * @brief Tests whether structurally valid RFC 8058 headers are present.
         */
        [[nodiscard]] bool hasOneClickCandidate();

        /**
         * @brief Tests whether DKIM verification for the candidate is running.
         */
        [[nodiscard]] bool oneClickVerificationPending() const;

        /**
         * @brief Get every advertised email unsubscribe URL.
         *
         * The normal MessageCore parser is used first. A narrowly scoped raw
         * header recovery is also included for a valid bracketed mailto URI
         * that appears after another malformed List-Unsubscribe URI.
         *
         * @return The advertised mailto URLs, or an empty list when unavailable.
         */
        [[nodiscard]] QList<QUrl> emailUrls() const;

        /**
         * @brief Get the first advertised email unsubscribe URL.
         *
         * @return The first mailto URL, or an empty URL when unavailable.
         */
        QUrl emailUrl() const;

        /**
         * @brief Get the advertised web unsubscribe URL.
         *
         * HTTPS is preferred over HTTP. A cached RFC 8058 one-click URL is
         * preferred when present.
         *
         * @return The preferred web URL, or an empty URL when unavailable.
         */
        QUrl webUrl();

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
        void unsubscribeStatusChanged();

    private:
        [[nodiscard]] bool hasValidOneClickHeaders() const;
        [[nodiscard]] bool dkimSignatureCoversOneClickHeaders() const;

        // message info
        std::shared_ptr<KMime::Message> mMessage = nullptr;
        MessageCore::MailingList mList;
        Akonadi::Item::Id mItemId = -1;

        // Cached by oneClickUrl()
        QUrl mPostUrl;

        // Used to check DKIM, for RFC 8058 compliance
        std::unique_ptr<DKIMManager> mDkimMgr;
        bool mDKIMValid = false;
        bool mOneClickCandidate = false;
        bool mDkimVerificationPending = false;
        QTimer mDkimTimeout;
    };
}

#endif /* !_UNSUBSCRIBEMANAGER_H_ */
