#pragma once

#include <MessageComposer/PluginEditorInitInterface>

class UnsubscribePluginEditorInitInterface : public MessageComposer::PluginEditorInitInterface
{
    Q_OBJECT
public:
    explicit UnsubscribePluginEditorInitInterface(QObject *parent = nullptr);
    ~UnsubscribePluginEditorInitInterface() override;

    [[nodiscard]] bool exec() override;
};
