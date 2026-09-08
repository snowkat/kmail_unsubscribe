#include "unsubscribeplugineditorinit.h"

#include "unsubscribeplugineditorinitinterface.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(UnsubscribePluginEditorInit, "kmail_unsubscribe_editorinitplugin.json")

UnsubscribePluginEditorInit::UnsubscribePluginEditorInit(QObject *parent, const QList<QVariant> &)
    : MessageComposer::PluginEditorInit(parent)
{
}

UnsubscribePluginEditorInit::~UnsubscribePluginEditorInit() = default;

MessageComposer::PluginEditorInitInterface *UnsubscribePluginEditorInit::createInterface(QObject *parent)
{
    return new UnsubscribePluginEditorInitInterface(parent);
}

#include "unsubscribeplugineditorinit.moc"
#include "moc_unsubscribeplugineditorinit.cpp"
