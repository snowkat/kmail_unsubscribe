#pragma once

#include "unsubscribeplan.h"

#include <QDialog>

class QPushButton;
class QCheckBox;

namespace KMailUnsubscribe
{
// Side-effect-free confirmation shared by N=1 and larger selections.
class UnsubscribeConfirmation : public QDialog
{
public:
    explicit UnsubscribeConfirmation(const QList<UnsubscribePlanEntry> &plan, QWidget *parent = nullptr);
    [[nodiscard]] bool confirmed() const;
    [[nodiscard]] bool deleteAfterSuccess() const;

private:
    QPushButton *mConfirmButton = nullptr;
    QCheckBox *mDeleteAfterSuccess = nullptr;
};
}
