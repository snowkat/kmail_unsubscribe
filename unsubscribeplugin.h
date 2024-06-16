#ifndef _UNSUBSCRIBEPLUGIN_H
#define _UNSUBSCRIBEPLUGIN_H

#include <MessageViewer/ViewerPlugin>
#include <QVariant>

namespace MessageViewer
{
    class UnsubscribePlugin : public MessageViewer::ViewerPlugin
    {
        Q_OBJECT
    public:
        explicit UnsubscribePlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());

        [[nodiscard]] ViewerPluginInterface *createView(QWidget *parent, KActionCollection *ac) override;
        [[nodiscard]] QString viewerPluginName() const override
        {
            return QStringLiteral("oneclick-unsubscribe");
        };
    };
}

#endif
