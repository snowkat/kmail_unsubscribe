#pragma once

#include <MessageComposer/PluginEditorInit>

class UnsubscribePluginEditorInit : public MessageComposer::PluginEditorInit
{
    Q_OBJECT
public:
    explicit UnsubscribePluginEditorInit(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());
    ~UnsubscribePluginEditorInit() override;

    [[nodiscard]] MessageComposer::PluginEditorInitInterface *createInterface(QObject *parent = nullptr) override;
};
