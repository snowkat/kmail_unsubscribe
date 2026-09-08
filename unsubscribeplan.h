#pragma once

#include "unsubscribeworkflow.h"

#include <KLocalizedString>
#include <QList>

namespace KMailUnsubscribe
{
struct UnsubscribeCapabilities
{
    bool email = false;
    bool web = false;
    bool oneClick = false;
};

struct UnsubscribePlanEntry
{
    QString subject;
    std::optional<UnsubscribeWorkflow::Method> method;
    bool loadFailed = false;
    bool invalidEmail = false;
    QList<UnsubscribeWorkflow::Method> fallbackMethods;
};

// This sequence is computed before confirmation and kept with the message.
// A failed one-click request can fall back to the regular page, then email.
inline QList<UnsubscribeWorkflow::Method> unsubscribeMethods(const UnsubscribeCapabilities &capabilities)
{
    using Method = UnsubscribeWorkflow::Method;
    QList<Method> methods;
    if (capabilities.oneClick && capabilities.web)
    {
        methods.append(Method::OneClick);
    }
    if (capabilities.web)
    {
        methods.append(Method::Web);
    }
    if (capabilities.email)
    {
        methods.append(Method::Email);
    }
    return methods;
}

inline std::optional<UnsubscribeWorkflow::Method> unsubscribeMethod(const UnsubscribeCapabilities &capabilities)
{
    const auto methods = unsubscribeMethods(capabilities);
    return methods.isEmpty() ? std::nullopt : std::optional<UnsubscribeWorkflow::Method>(methods.constFirst());
}

inline QString unsubscribeMethodLabel(UnsubscribeWorkflow::Method method)
{
    switch (method)
    {
    case UnsubscribeWorkflow::Method::OneClick:
        return i18n("One-click");
    case UnsubscribeWorkflow::Method::Web:
        return i18n("Web");
    case UnsubscribeWorkflow::Method::Email:
        return i18n("Email");
    }
    return {};
}

inline QString unsubscribeMethodSequenceLabel(const UnsubscribePlanEntry &entry)
{
    if (!entry.method)
    {
        return {};
    }
    QStringList labels = {unsubscribeMethodLabel(*entry.method)};
    for (const auto method : entry.fallbackMethods)
    {
        labels.append(unsubscribeMethodLabel(method));
    }
    return labels.join(QStringLiteral(" → "));
}
}
