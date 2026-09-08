#pragma once

#include "unsubscribemanager.h"

#include <Akonadi/Collection>
#include <Akonadi/Item>
#include <QObject>
#include <QPointer>

#include <optional>

class QWidget;

namespace KMailUnsubscribe
{
class UnsubscribeWorkflow : public QObject
{
    Q_OBJECT
public:
    enum class Method
    {
        OneClick,
        Email,
        Web,
    };

    enum class WebCapability
    {
        None,
        Regular,
        VerifyingOneClick,
        OneClick,
    };

    explicit UnsubscribeWorkflow(QObject *parent = nullptr);

    void setParentWidget(QWidget *parent);
    void setMessageItem(const Akonadi::Item &item, bool verifyOneClick = true);
    void setCurrentCollection(const Akonadi::Collection &collection);
    void setDeleteAfterSuccess(bool enabled);
    void reset();

    [[nodiscard]] bool emailAdvertised();
    [[nodiscard]] bool emailAvailable();
    [[nodiscard]] QString emailValidationError();
    [[nodiscard]] bool webAvailable();
    [[nodiscard]] bool anyMethodAvailable();
    [[nodiscard]] bool hasOneClickCandidate();
    [[nodiscard]] WebCapability webCapability();
    [[nodiscard]] std::optional<Method> resolvedMethod(Method method);
    virtual void execute(Method method);

Q_SIGNALS:
    void stateChanged();
    void finished(bool success, const QString &error = QString());

private:
    [[nodiscard]] QUrl validEmailUrl() const;

    MessageViewer::UnsubscribeManager mUnsubscribeManager;
    Akonadi::Item mMessageItem;
    Akonadi::Collection mCurrentCollection;
    QPointer<QWidget> mParent;
    bool mDeleteAfterSuccess = false;
};
}
