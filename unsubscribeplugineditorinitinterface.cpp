#include "unsubscribeplugineditorinitinterface.h"

#include "unsubscribecomposerrequest.h"
#include "unsubscribecomposerrequeststore.h"
#include "unsubscribeemailcleanup.h"
#include "unsubscribe_debug.h"

#include <KPIMTextEdit/RichTextComposer>
#include <MessageComposer/ComposerViewBase>
#include <QAction>
#include <QApplication>
#include <QPointer>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

UnsubscribePluginEditorInitInterface::UnsubscribePluginEditorInitInterface(QObject *parent)
    : MessageComposer::PluginEditorInitInterface(parent)
{
}

UnsubscribePluginEditorInitInterface::~UnsubscribePluginEditorInitInterface() = default;

bool UnsubscribePluginEditorInitInterface::exec()
{
    QPointer<KPIMTextEdit::RichTextComposer> editor = richTextEditor();
    QPointer<QWidget> composerWindow = parentWidget();

    // KMail initializes editor plugins before it loads the message and queues
    // its automatic signature. This first turn observes the loaded marker; the
    // second turn runs after the queued signature insertion.
    QTimer::singleShot(0, this, [this, editor, composerWindow]() {
        if (!editor)
        {
            return;
        }

        const auto request = KMailUnsubscribe::composerRequestFromText(editor->toPlainText());
        if (!request)
        {
            return;
        }

        QTimer::singleShot(0, this, [editor, composerWindow, request = *request]() {
            if (!editor)
            {
                return;
            }

            editor->setPlainText(request.body);
            editor->moveCursor(QTextCursor::Start);

            if (!composerWindow)
            {
                return;
            }

            if (const auto deletion = KMailUnsubscribe::ComposerRequestStore().take(request.token))
            {
                if (auto *composer = composerWindow->findChild<MessageComposer::ComposerViewBase *>())
                {
                    // This outlives the composer and the batch: closing an
                    // unsent draft keeps the original, while a queued email
                    // is monitored until the dispatcher confirms sending.
                    new KMailUnsubscribe::UnsubscribeEmailCleanup(composer, *deletion, qApp);
                }
                else
                {
                    qCWarning(UnsubscribePlugin) << "Could not attach unsubscribe send tracking; original message will be kept";
                }
            }

            const auto buttons = composerWindow->findChildren<QToolButton *>();
            for (QToolButton *const button : buttons)
            {
                const QAction *const action = button->defaultAction();
                if (action && (action->objectName() == QLatin1StringView("send_default_via")
                               || action->objectName() == QLatin1StringView("send_alternative_via")))
                {
                    // The main part now triggers KMail's existing direct-send
                    // handler. The arrow retains the explicit transport menu.
                    button->setPopupMode(QToolButton::MenuButtonPopup);
                }
            }
        });
    });

    return true;
}

#include "moc_unsubscribeplugineditorinitinterface.cpp"
