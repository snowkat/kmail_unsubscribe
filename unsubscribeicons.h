#pragma once

#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace KMailUnsubscribe
{
inline QIcon unsubscribeIcon(bool oneClick = false)
{
    const QIcon baseIcon = QIcon::fromTheme(QStringLiteral("news-unsubscribe"), QIcon::fromTheme(QStringLiteral("list-remove")));
    if (!oneClick)
    {
        return baseIcon;
    }
    QIcon result;

    for (const int size : {16, 22, 24, 32, 48})
    {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        baseIcon.paint(&painter, QRect(0, 0, size, size), Qt::AlignCenter, QIcon::Normal);

        const qreal badgeSize = size * 0.48;
        const QRectF badge(size - badgeSize, size - badgeSize, badgeSize, badgeSize);
        painter.setPen(QPen(QColor(255, 255, 255, 220), qMax(1.0, size / 24.0)));
        painter.setBrush(QColor(QStringLiteral("#f0a30a")));
        painter.drawEllipse(badge.adjusted(0.5, 0.5, -0.5, -0.5));

        painter.setPen(QPen(Qt::white, qMax(1.5, size / 9.0), Qt::SolidLine, Qt::RoundCap));
        {
            QPainterPath bolt;
            bolt.moveTo(badge.left() + badge.width() * 0.58, badge.top() + badge.height() * 0.15);
            bolt.lineTo(badge.left() + badge.width() * 0.28, badge.top() + badge.height() * 0.57);
            bolt.lineTo(badge.left() + badge.width() * 0.50, badge.top() + badge.height() * 0.57);
            bolt.lineTo(badge.left() + badge.width() * 0.40, badge.top() + badge.height() * 0.86);
            bolt.lineTo(badge.left() + badge.width() * 0.74, badge.top() + badge.height() * 0.43);
            bolt.lineTo(badge.left() + badge.width() * 0.52, badge.top() + badge.height() * 0.43);
            bolt.closeSubpath();
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::white);
            painter.drawPath(bolt);
        }

        painter.end();
        result.addPixmap(pixmap);
    }

    return result;
}
}
