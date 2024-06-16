#ifndef _UNSUBSCRIBEPLUGININTERFACE_H_
#define _UNSUBSCRIBEPLUGININTERFACE_H_

#include <MessageViewer/ViewerPluginInterface>
#include "unsubscribemanager.h"

namespace MessageViewer
{
    class UnsubscribePluginInterface : public MessageViewer::ViewerPluginInterface
    {
        Q_OBJECT
    public:
        explicit UnsubscribePluginInterface(QWidget *parent, KActionCollection *ac = nullptr);
        ~UnsubscribePluginInterface() override;

        [[nodiscard]] QList<QAction *> actions() const override;
        void closePlugin() override;
        void execute() override;
        void setMessageItem(const Akonadi::Item &item) override;
        void updateAction(const Akonadi::Item &item) override;
        [[nodiscard]] ViewerPluginInterface::SpecificFeatureTypes featureTypes() const override
        {
            return ViewerPluginInterface::NeedMessage;
        }

    public slots:
        void getOneClickResult(bool isSuccess, const QString &resultString);

    private:
        UnsubscribeManager mUnsub;
        // Whether we're in the middle of a one-click unsubscribe operation
        bool mUnsubscribing = true;

        QList<QAction *> mActions;
        QWidget *mParent;
    };
}

#endif