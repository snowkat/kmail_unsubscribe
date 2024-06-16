#include "unsubscribeplugin.h"
#include "unsubscribeplugininterface.h"
#include <KActionCollection>
#include <KPluginFactory>

using namespace MessageViewer;
K_PLUGIN_CLASS_WITH_JSON(UnsubscribePlugin, "kmail_unsubscribeplugin.json")

UnsubscribePlugin::UnsubscribePlugin(QObject *parent, const QList<QVariant> &)
    : MessageViewer::ViewerPlugin(parent)
{
}

ViewerPluginInterface *MessageViewer::UnsubscribePlugin::createView(QWidget *parent, KActionCollection *ac)
{
    MessageViewer::ViewerPluginInterface *view = new MessageViewer::UnsubscribePluginInterface(parent, ac);
    return view;
}

#include "unsubscribeplugin.moc"
#include "moc_unsubscribeplugin.cpp"