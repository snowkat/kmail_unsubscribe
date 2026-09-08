#pragma once

#include <QString>

#include <optional>

namespace KMailUnsubscribe
{
struct ComposerDeleteRequest
{
    qint64 originalId = -1;
    QString subject;
};

// Deletion consent stays local, separate from message text. A pasted or quoted
// marker must never authorize moving an arbitrary original message to Trash.
class ComposerRequestStore
{
public:
    explicit ComposerRequestStore(const QString &directory = QString());
    [[nodiscard]] bool remember(const QString &token, const ComposerDeleteRequest &request);
    [[nodiscard]] std::optional<ComposerDeleteRequest> take(const QString &token);
    void discard(const QString &token);

private:
    [[nodiscard]] QString pathForToken(const QString &token) const;
    QString mDirectory;
};
}
