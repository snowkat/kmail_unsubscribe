#include "unsubscribeconfirmation.h"

#include "unsubscribeicons.h"

#include <KLocalizedString>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace KMailUnsubscribe;

UnsubscribeConfirmation::UnsubscribeConfirmation(const QList<UnsubscribePlanEntry> &plan, QWidget *parent)
    : QDialog(parent)
{
    int available = 0;
    int oneClick = 0;
    int loadFailures = 0;
    for (const auto &entry : plan)
    {
        available += entry.method.has_value();
        oneClick += entry.method == UnsubscribeWorkflow::Method::OneClick;
        loadFailures += entry.loadFailed;
    }

    setObjectName(QStringLiteral("unsubscribeConfirmation"));
    setWindowTitle(i18n("Unsubscribe"));
    auto *layout = new QVBoxLayout(this);
    auto *heading = new QHBoxLayout;
    auto *icon = new QLabel(this);
    icon->setPixmap((available > 0 ? unsubscribeIcon(oneClick > 0)
                                  : style()->standardIcon(QStyle::SP_MessageBoxCritical)).pixmap(48, 48));
    heading->addWidget(icon, 0, Qt::AlignTop);
    auto *question = new QLabel(this);
    question->setObjectName(QStringLiteral("unsubscribeQuestion"));
    question->setTextFormat(Qt::PlainText);
    question->setWordWrap(true);
    if (available > 0)
    {
        question->setText(i18np("Do you want to unsubscribe from the following message?",
                               "Do you want to unsubscribe from the following %1 messages?", plan.size()));
    }
    else if (!plan.isEmpty() && loadFailures == plan.size())
    {
        question->setText(i18n("The selected messages could not be loaded to check unsubscribe methods."));
    }
    else
    {
        question->setText(i18n("No unsubscribe method is available for the selected messages."));
    }
    heading->addWidget(question, 1);
    layout->addLayout(heading);

    auto *messages = new QTreeWidget(this);
    messages->setObjectName(QStringLiteral("unsubscribePlan"));
    messages->setHeaderLabels({i18n("Message"), i18n("Unsubscribe sequence")});
    messages->setRootIsDecorated(false);
    messages->setAlternatingRowColors(true);
    messages->setUniformRowHeights(true);
    messages->setSelectionMode(QAbstractItemView::NoSelection);
    messages->setEditTriggers(QAbstractItemView::NoEditTriggers);
    messages->setTextElideMode(Qt::ElideRight);
    messages->header()->setStretchLastSection(false);
    messages->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    messages->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    for (const auto &entry : plan)
    {
        QString method;
        if (!entry.method)
        {
            method = entry.loadFailed ? i18n("Skipped — could not load")
                     : entry.invalidEmail ? i18n("Skipped — invalid email address")
                                          : i18n("Skipped — unavailable");
        }
        else
        {
            method = unsubscribeMethodSequenceLabel(entry);
        }
        auto *row = new QTreeWidgetItem(messages, {entry.subject, method});
        row->setToolTip(0, entry.subject);
    }
    const int rowHeight = qMax(24, fontMetrics().height() + 8);
    messages->setMinimumHeight(messages->header()->sizeHint().height() + rowHeight * qBound(1, int(plan.size()), 8) + 4);
    layout->addWidget(messages, 1);

    if (available > 0 && available < plan.size())
    {
        auto *skipped = new QLabel(i18np("%1 message without an available method will be skipped.",
                                        "%1 messages without an available method will be skipped.", plan.size() - available), this);
        skipped->setWordWrap(true);
        layout->addWidget(skipped);
    }

    if (available > 0)
    {
        mDeleteAfterSuccess = new QCheckBox(i18n("Delete after successful unsubscribe"), this);
        mDeleteAfterSuccess->setObjectName(QStringLiteral("unsubscribeDeleteAfterSuccess"));
        mDeleteAfterSuccess->setChecked(true);
        mDeleteAfterSuccess->setToolTip(i18n("Move messages to Trash after a successful one-click request or after sending the unsubscribe email. "
                                           "Messages requiring a website are kept because completion cannot be confirmed."));
        layout->addWidget(mDeleteAfterSuccess);
    }

    auto *buttons = new QDialogButtonBox(this);
    buttons->setObjectName(QStringLiteral("unsubscribeButtons"));
    if (available > 0)
    {
        mConfirmButton = buttons->addButton(i18n("Unsubscribe"), QDialogButtonBox::AcceptRole);
        mConfirmButton->setObjectName(QStringLiteral("unsubscribeConfirm"));
        mConfirmButton->setAutoDefault(false);
        mConfirmButton->setIcon(unsubscribeIcon(oneClick > 0));
        auto *cancel = buttons->addButton(QDialogButtonBox::Cancel);
        cancel->setObjectName(QStringLiteral("unsubscribeCancel"));
        cancel->setDefault(true);
        cancel->setFocus();
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    else
    {
        auto *close = buttons->addButton(QDialogButtonBox::Ok);
        close->setDefault(true);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::reject);
    }
    layout->addWidget(buttons);
    resize(640, sizeHint().height());
}

bool UnsubscribeConfirmation::confirmed() const
{
    return mConfirmButton && result() == QDialog::Accepted;
}

bool UnsubscribeConfirmation::deleteAfterSuccess() const
{
    return confirmed() && mDeleteAfterSuccess && mDeleteAfterSuccess->isChecked();
}
