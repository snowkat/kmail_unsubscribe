#pragma once

#include "unsubscribeworkflow.h"

#include <QElapsedTimer>
#include <QHash>
#include <QQueue>

namespace KMailUnsubscribe
{
// Display-only cache shared by the toolbar and visible message rows. Executing
// an action always fetches and validates the message again.
class UnsubscribeAvailability : public QObject
{
    Q_OBJECT
public:
    struct State
    {
        bool loaded = false;
        bool email = false;
        UnsubscribeWorkflow::WebCapability web = UnsubscribeWorkflow::WebCapability::None;

        [[nodiscard]] bool available() const
        {
            return email || web != UnsubscribeWorkflow::WebCapability::None;
        }
    };

    explicit UnsubscribeAvailability(QObject *parent = nullptr);
    void requestItems(const Akonadi::Item::List &items);
    [[nodiscard]] State state(Akonadi::Item::Id id) const;

Q_SIGNALS:
    void changed();

private:
    struct Entry
    {
        State state;
        quint64 generation = 0;
        qint64 requestedAt = 0;
        qint64 usedAt = 0;
        bool pending = true;
    };
    struct Verification
    {
        Akonadi::Item::Id id;
        quint64 generation;
    };

    void fetchHeaders(const Akonadi::Item::List &items, quint64 generation);
    void verifyNext();
    void finishVerification(Verification request, UnsubscribeWorkflow *workflow = nullptr);
    void trimCache();

    QHash<Akonadi::Item::Id, Entry> mEntries;
    QQueue<Verification> mVerificationQueue;
    QElapsedTimer mClock;
    quint64 mGeneration = 0;
    int mVerifying = 0;
};
}
