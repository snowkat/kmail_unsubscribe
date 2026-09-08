#include "unsubscriberowactions.h"

#include "unsubscribeicons.h"

#include <Akonadi/EntityTreeModel>
#include <KLocalizedString>
#include <QEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <QSet>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

using namespace KMailUnsubscribe;

UnsubscribeRowActions::UnsubscribeRowActions(QTreeView *view, UnsubscribeAvailability *availability, QObject *parent)
    : QObject(parent)
    , mView(view)
    , mAvailability(availability)
    , mIcon(unsubscribeIcon())
    , mOneClickIcon(unsubscribeIcon(true))
{
    connect(view, &QObject::destroyed, this, &QObject::deleteLater);
    view->viewport()->installEventFilter(this);
    connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view->header(), &QHeaderView::geometriesChanged, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view->header(), &QHeaderView::sectionResized, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view->header(), &QHeaderView::sectionMoved, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view, &QTreeView::expanded, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view, &QTreeView::collapsed, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view->model(), &QAbstractItemModel::modelReset, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view->model(), &QAbstractItemModel::layoutChanged, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view->model(), &QAbstractItemModel::rowsInserted, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view->model(), &QAbstractItemModel::rowsRemoved, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(view->model(), &QAbstractItemModel::dataChanged, this, &UnsubscribeRowActions::scheduleRefresh);
    connect(availability, &UnsubscribeAvailability::changed, this, &UnsubscribeRowActions::scheduleRefresh);
    scheduleRefresh();
}

UnsubscribeRowActions::~UnsubscribeRowActions()
{
    if (mView)
    {
        mView->setProperty("unsubscribeRowActionsAttached", false);
    }
    const auto ids = mButtons.keys();
    for (const auto id : ids)
    {
        removeButtons(id);
    }
}

void UnsubscribeRowActions::setBusy(bool busy)
{
    if (mBusy == busy)
    {
        return;
    }
    mBusy = busy;
    scheduleRefresh();
}

bool UnsubscribeRowActions::eventFilter(QObject *watched, QEvent *event)
{
    // Child tool buttons generate LayoutRequest when their icons/visibility
    // change. Refreshing in response would create a hide/show feedback loop.
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::Hide
        || event->type() == QEvent::StyleChange)
    {
        scheduleRefresh();
    }
    return QObject::eventFilter(watched, event);
}

void UnsubscribeRowActions::scheduleRefresh()
{
    // Do not leave clickable controls bound to stale rows while a sort, scroll,
    // folder switch, or model update is waiting for its layout pass.
    for (const auto &buttons : std::as_const(mButtons))
    {
        if (buttons.button)
        {
            buttons.button->hide();
        }
    }
    if (mRefreshScheduled)
    {
        return;
    }
    mRefreshScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        mRefreshScheduled = false;
        refresh();
    });
}

void UnsubscribeRowActions::removeButtons(Akonadi::Item::Id id)
{
    const auto buttons = mButtons.take(id);
    delete buttons.button.data();
}

void UnsubscribeRowActions::refresh()
{
    if (!mView || !mView->isVisible() || !mView->model())
    {
        return;
    }

    auto *header = mView->header();
    int column = -1;
    for (int i = 0; i < header->count(); ++i)
    {
        if (!header->isSectionHidden(i)
            && mView->model()->headerData(i, Qt::Horizontal).toString() == i18nd("libmessagelist6", "Status"))
        {
            column = i;
            break;
        }
    }
    if (column < 0)
    {
        return;
    }

    auto *viewport = mView->viewport();
    const int iconSize = mView->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, mView);
    const int buttonSize = iconSize + 6;
    const int actionsWidth = buttonSize + 4;
    const int minimumWidth = iconSize * 2 + 8 + actionsWidth;
    const auto ensureStatusWidth = [header, column](int width) {
        const int extra = width - header->sectionSize(column);
        if (extra <= 0)
        {
            return false;
        }
        // The Smart theme has Message and Status columns. Reserve the extra
        // space from Message, otherwise the new icon ends up offscreen to the
        // right of the old Status column and requires horizontal scrolling.
        if (header->count() == 2)
        {
            const int messageColumn = 1 - column;
            const int remaining = header->sectionSize(messageColumn) - extra;
            if (remaining >= qMax(120, header->minimumSectionSize()))
            {
                header->resizeSection(messageColumn, remaining);
            }
        }
        header->resizeSection(column, width);
        return true;
    };
    ensureStatusWidth(minimumWidth);

    const int columnX = header->sectionViewportPosition(column);
    const int columnRight = columnX + header->sectionSize(column);
    const int sampleX = qMax(0, columnX);
    if (sampleX >= viewport->width() || columnRight <= 0)
    {
        return;
    }

    QSet<Akonadi::Item::Id> visible;
    Akonadi::Item::List items;
    for (int y = 0; y < viewport->height();)
    {
        const QModelIndex index = mView->indexAt(QPoint(sampleX, y));
        if (!index.isValid())
        {
            y += 4;
            continue;
        }
        const QRect rect = mView->visualRect(index);
        y = qMax(y + 1, rect.bottom() + 1);
        const auto item = index.data(Akonadi::EntityTreeModel::ItemRole).value<Akonadi::Item>();
        if (!item.isValid() || visible.contains(item.id()))
        {
            continue; // Group/date headers have no Akonadi item.
        }
        visible.insert(item.id());
        items.append(item);

        // The standard Status column has two rows of two native icons. Leave
        // room for larger theme icons as well as the new controls. Using a
        // minimum, instead of repeatedly adding width, survives restarts.
        const int rowMinimumWidth = qMax(iconSize * 2 + 8, rect.height() + 4) + actionsWidth;
        if (ensureStatusWidth(rowMinimumWidth))
        {
            scheduleRefresh();
            return;
        }

        auto it = mButtons.find(item.id());
        if (it == mButtons.end())
        {
            Buttons buttons;
            buttons.item = item;
            auto *button = new QToolButton(viewport);
            buttons.button = button;
            button->setObjectName(QStringLiteral("unsubscribeRowButton"));
            button->setProperty("akonadiItemId", item.id());
            button->setAutoRaise(true);
            button->setFocusPolicy(Qt::NoFocus);
            button->setIconSize(QSize(iconSize, iconSize));
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setAccessibleName(i18n("Unsubscribe"));
            connect(button, &QToolButton::clicked, this, [this, id = item.id()]() {
                Q_EMIT unsubscribeRequested(mButtons.value(id).item);
            });
            it = mButtons.insert(item.id(), buttons);
        }
        it->item = item;
        const auto state = mAvailability->state(item.id());
        const bool oneClick = state.web == UnsubscribeWorkflow::WebCapability::OneClick;
        it->button->setEnabled(state.available() && !mBusy);
        it->button->setIcon(oneClick ? mOneClickIcon : mIcon);
        it->button->setToolTip(!state.loaded ? i18n("Checking unsubscribe methods…")
                              : !state.available() ? i18n("No unsubscribe method is available for this message")
                              : oneClick ? i18n("Unsubscribe from this message; verified one-click is available")
                                         : i18n("Unsubscribe from this message"));
        const int top = rect.top() + qMax(0, (rect.height() - buttonSize) / 2);
        const int left = mView->isRightToLeft() ? columnX + 2 : columnRight - actionsWidth;
        it->button->setGeometry(left, top, buttonSize, buttonSize);
        it->button->show();
        it->button->raise();
    }
    const auto previous = mButtons.keys();
    for (const auto id : previous)
    {
        if (!visible.contains(id))
        {
            removeButtons(id);
        }
    }
    mAvailability->requestItems(items);
}
