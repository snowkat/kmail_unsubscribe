#pragma once

#include <Akonadi/Item>
#include <KJob>

namespace KMailUnsubscribe
{
class UnsubscribeTrashJob : public KJob
{
    Q_OBJECT
public:
    explicit UnsubscribeTrashJob(Akonadi::Item::Id originalId, QObject *parent = nullptr);
    void start() override;
    [[nodiscard]] bool moved() const;

protected:
    bool doKill() override;

private:
    void fail(const QString &message);
    Akonadi::Item::Id mOriginalId;
    bool mMoved = false;
};
}
