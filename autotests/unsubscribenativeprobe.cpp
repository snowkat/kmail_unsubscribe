// Manual read-only integration probe using the installed KDE message list.
// Pass one existing Akonadi item with List-Unsubscribe, then one without it.
// It only fetches messages and cancels confirmations; it never unsubscribes.
#include <Akonadi/EntityTreeModel>
#include <KActionCollection>
#include <KActionMenu>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KMime/Message>
#include <MessageList/Pane>
#include <MessageList/ThemeComboBox>
#include <MessageViewer/MessageViewerSettings>
#include <MessageViewer/Viewer>
#include <PimCommon/GenericPlugin>
#include <PimCommonAkonadi/GenericPluginInterface>
#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWebEngineUrlScheme>
#include <iostream>

using ETM = Akonadi::EntityTreeModel;

class FolderModel : public QStandardItemModel
{
public:
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        // Required by EntityMimeTypeFilterModel's ItemListHeaders proxy.
        if (role % ETM::TerminalUserRole == ETM::ColumnCountRole) return 1;
        return QStandardItemModel::data(index, role);
    }
};

class LayoutObserver : public QObject
{
public:
    int requests = 0;
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::LayoutRequest) ++requests;
        return QObject::eventFilter(watched, event);
    }
};

QModelIndex findItem(QAbstractItemModel *model, qint64 id, const QModelIndex &parent = {})
{
    for (int row = 0; row < model->rowCount(parent); ++row) {
        const auto index = model->index(row, 0, parent);
        if (index.data(ETM::ItemIdRole).isValid() && index.data(ETM::ItemIdRole).toLongLong() == id) return index;
        const auto found = findItem(model, id, index);
        if (found.isValid()) return found;
    }
    return {};
}

bool waitFor(const std::function<bool()> &condition, int timeout = 10000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout) QTest::qWait(50);
    return condition();
}

int main(int argc, char **argv)
{
    QTemporaryDir config;
    const QString originalConfig = qEnvironmentVariable("XDG_CONFIG_HOME", QDir::homePath() + QStringLiteral("/.config"));
    QDir().mkpath(config.path() + QStringLiteral("/akonadi"));
    // Only copy the local server connection address, not KMail preferences.
    if (!QFile::copy(originalConfig + QStringLiteral("/akonadi/akonadiconnectionrc"),
                     config.path() + QStringLiteral("/akonadi/akonadiconnectionrc"))) return 65;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    // Match KMail's WebEngine initialization for its local message renderer.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QWebEngineUrlScheme cidScheme("cid");
    cidScheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    cidScheme.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::ContentSecurityPolicyIgnored
                       | QWebEngineUrlScheme::LocalScheme | QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(cidScheme);
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kmail_unsubscribe");
    KLocalizedString::setLanguages({QStringLiteral("en_US")});
    if (argc != 3) return 64;
    const qint64 listId = QByteArray(argv[1]).toLongLong();
    const qint64 plainId = QByteArray(argv[2]).toLongLong();
    auto check = [](bool value, const char *description) {
        std::cout << (value ? "PASS " : "FAIL ") << description << std::endl;
        return value;
    };
    // The source model and its selection model must outlive the native pane.
    FolderModel model;
    QItemSelectionModel folderSelection(&model);
    QWidget parent;
    auto *layout = new QVBoxLayout(&parent);
    Akonadi::Collection collection(999999);
    collection.setName(QStringLiteral("Probe inbox"));
    collection.setContentMimeTypes({QStringLiteral("message/rfc822")});
    collection.setParentCollection(Akonadi::Collection::root());
    auto *folder = new QStandardItem(QStringLiteral("Probe inbox"));
    folder->setData(QVariant::fromValue(collection), ETM::CollectionRole);
    folder->setData(collection.id(), ETM::CollectionIdRole);
    folder->setData(Akonadi::Collection::mimeType(), ETM::MimeTypeRole);
    model.appendRow(folder);
    for (qint64 id : {listId, plainId}) {
        // Deliberately envelope-only, with synthetic list text.
        auto message = std::make_shared<KMime::Message>();
        message->setContent(QByteArray("From: sender@example.org\nTo: recipient@example.org\nSubject: ")
                            + (id == listId ? "Mailing list example" : "Ordinary message")
                            + "\nDate: Tue, 8 Sep 2026 08:00:00 +0200\nMessage-ID: <" + QByteArray::number(id) + "@example.org>\n\n");
        message->parse();
        Akonadi::Item item(id);
        item.setMimeType(QStringLiteral("message/rfc822"));
        item.setPayload(message);
        item.setParentCollection(collection);
        auto *row = new QStandardItem;
        row->setData(QVariant::fromValue(item), ETM::ItemRole);
        row->setData(item.id(), ETM::ItemIdRole);
        row->setData(item.mimeType(), ETM::MimeTypeRole);
        row->setData(QVariant::fromValue(collection), ETM::ParentCollectionRole);
        folder->appendRow(row);
    }
    auto *pane = new MessageList::Pane(false, &model, &folderSelection, &parent);
    // Pane constructs the theme manager; the combo box must be created after it.
    {
        MessageList::Utils::ThemeComboBox themes(nullptr);
        themes.slotLoadThemes();
        bool found = false;
        for (int i = 0; i < themes.count(); ++i) {
            if (themes.itemText(i).contains(QStringLiteral("Clickable Status"))) {
                themes.setCurrentIndex(i);
                themes.writeDefaultConfig();
                found = true;
            }
        }
        if (!check(found, "native clickable Status theme selected")) return 1;
    }
    auto *toolbar = new QToolBar(&parent);
    layout->addWidget(toolbar);
    layout->addWidget(pane);
    KActionCollection actions(&parent);
    auto *mailingList = new KActionMenu(QStringLiteral("Mailing List"), &actions);
    actions.addAction(QStringLiteral("mailing_list"), mailingList);
    auto *nativeEmail = mailingList->menu()->addAction(QStringLiteral("Unsubscribe from List (email)"));
    nativeEmail->setData(QUrl(QStringLiteral("mailto:leave@example.org")));
    auto *nativeWeb = mailingList->menu()->addAction(QStringLiteral("Unsubscribe from List (web)"));
    nativeWeb->setData(QUrl(QStringLiteral("https://example.org/preferences")));
    PimCommon::GenericPluginInterface *plugin = nullptr;
    for (const auto &meta : KPluginMetaData::findPlugins(QStringLiteral("pim6/kmail/mainview"))) {
        if (!meta.fileName().endsWith(QStringLiteral("/kmail_unsubscribe_actionplugin.so"))) continue;
        std::cout << "LOADED " << meta.fileName().toStdString() << std::endl;
        const auto result = KPluginFactory::instantiatePlugin<PimCommon::GenericPlugin>(meta, &parent);
        if (!check(bool(result.plugin), "plugin loader")) return 2;
        plugin = static_cast<PimCommon::GenericPluginInterface *>(result.plugin->createInterface(&parent));
        plugin->setParentWidget(&parent);
        plugin->createAction(&actions);
        QObject::connect(plugin, &PimCommon::GenericPluginInterface::emitPluginActivated, &parent,
                         [pane, plugin]() { plugin->setItems(pane->selectionAsMessageItemList()); plugin->exec(); });
    }
    if (!check(plugin != nullptr, "main-view plugin discovered")) return 3;
    auto *action = actions.action(QStringLiteral("kmail_unsubscribe_toolbar"));
    if (!check(action && !actions.action(QStringLiteral("kmail_unsubscribe_email"))
                   && !actions.action(QStringLiteral("kmail_unsubscribe_web")), "exactly one unsubscribe toolbar action")) return 4;
    toolbar->addAction(action);
    parent.resize(1000, 380);
    parent.show();
    folderSelection.setCurrentIndex(model.index(0, 0), QItemSelectionModel::ClearAndSelect);
    pane->setCurrentFolder(collection, model.index(0, 0), MessageList::Core::PreSelectNone);
    QTreeView *view = nullptr;
    for (auto *candidate : pane->findChildren<QTreeView *>())
        if (candidate->inherits("MessageList::Core::View") && candidate->isVisible()) view = candidate;
    if (!check(view != nullptr, "native message-list view")) return 5;
    if (!check(waitFor([&]() { return findItem(view->model(), listId).isValid(); }), "native model populated")) return 6;
    view->expandAll();
    QTest::qWait(150);
    for (int i = 0; i < view->header()->count(); ++i)
        std::cout << "COLUMN " << i << " x=" << view->header()->sectionViewportPosition(i)
                  << " width=" << view->header()->sectionSize(i) << " viewport=" << view->viewport()->width() << std::endl;
    const auto listIndex = findItem(view->model(), listId);
    const auto plainIndex = findItem(view->model(), plainId);
    const auto envelope = listIndex.data(ETM::ItemRole).value<Akonadi::Item>();
    if (!check(envelope.hasPayload<std::shared_ptr<KMime::Message>>()
                   && !envelope.payload<std::shared_ptr<KMime::Message>>()->headerByType("List-Unsubscribe"), "native list supplies abbreviated headers")) return 7;
    view->selectionModel()->setCurrentIndex(listIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (!check(waitFor([&]() { return action->isEnabled(); }), "toolbar enables after fetching unsubscribe methods")) return 8;
    view->selectionModel()->setCurrentIndex(plainIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (!check(waitFor([&]() { return !action->isEnabled(); }), "message without unsubscribe disables toolbar")) return 9;
    view->selectionModel()->select(listIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    if (!check(waitFor([&]() { return action->isEnabled(); }), "mixed selection enables toolbar")) return 10;

    QMenu menu;
    menu.setObjectName(QStringLiteral("akonadi_messagelist_contextmenu"));
    menu.addAction(mailingList);
    menu.show();
    QTest::qWait(100);
    if (!check(menu.actions().contains(action) && action->isIconVisibleInMenu() && !action->icon().isNull(), "list context menu has unsubscribe text and icon")) return 11;
    if (!check(!nativeEmail->isVisible() && !nativeWeb->isVisible() && !mailingList->isVisible(),
                   "single direct action replaces the native email/web unsubscribe submenu")) return 26;
    menu.grab().save(QStringLiteral("/tmp/kmail-unsubscribe-single-menu.png"));
    menu.hide();
    QTest::qWait(100);

    // KMail rebuilds this menu when the message changes. Preserve unrelated
    // commands, including ones that use the same destination URL.
    auto *nativeHelp = mailingList->menu()->addAction(QStringLiteral("Request Help (web)"));
    nativeHelp->setData(nativeWeb->data());
    nativeEmail->setVisible(true);
    nativeWeb->setVisible(true);
    menu.show();
    QTest::qWait(100);
    if (!check(mailingList->isVisible() && nativeHelp->isVisible()
                   && !nativeEmail->isVisible() && !nativeWeb->isVisible()
                   && menu.actions().count(action) == 1,
                   "reopening retains other mailing-list commands without duplicate unsubscribe entries")) return 27;
    menu.hide();
    QTest::qWait(100);

    int enabledRows = 0;
    int disabledRows = 0;
    for (auto *button : view->viewport()->findChildren<QToolButton *>(QStringLiteral("unsubscribeRowButton"))) {
        if (!button->isVisible()) continue;
        if (!check(view->viewport()->rect().contains(button->geometry()) && !button->visibleRegion().isEmpty(),
                       "row icon is fully visible without horizontal scrolling")) return 18;
        if (button->property("akonadiItemId").toLongLong() == listId && button->isEnabled()) ++enabledRows;
        if (button->property("akonadiItemId").toLongLong() == plainId && !button->isEnabled()) ++disabledRows;
    }
    parent.grab().save(QStringLiteral("/tmp/kmail-unsubscribe-single-rows.png"));
    if (!check(enabledRows == 1 && disabledRows == 1, "each row enables or disables unsubscribe from its own methods")) return 12;
    LayoutObserver observer;
    view->viewport()->installEventFilter(&observer);
    QTest::qWait(250);
    if (!check(observer.requests < 20, "row controls settle without a layout feedback loop")) return 19;

    int rowControllers = 0;
    for (auto *rows : plugin->findChildren<QObject *>()) {
        if (QByteArray(rows->metaObject()->className()) != "KMailUnsubscribe::UnsubscribeRowActions") continue;
        ++rowControllers;
        // Observe row dispatch without starting an unsubscribe operation.
        QObject::disconnect(rows, nullptr, plugin, nullptr);
        QSignalSpy spy(rows, SIGNAL(unsubscribeRequested(Akonadi::Item)));
        for (auto *button : view->viewport()->findChildren<QToolButton *>(QStringLiteral("unsubscribeRowButton")))
            if (button->property("akonadiItemId").toLongLong() == listId) button->click();
        if (!check(spy.size() == 1 && spy.first().first().value<Akonadi::Item>().id() == listId,
                       "row click dispatches its own message")) return 13;
        if (!check(pane->selectionAsMessageItemList().size() == 2, "row click preserves multi-selection")) return 14;
    }
    if (!check(rowControllers == 1, "row controller attached once")) return 15;

    // Exercise the real fetch/validation/confirmation path; always choose Cancel.
    action->trigger();
    QDialog *confirmation = nullptr;
    if (!check(waitFor([&]() {
            for (auto *window : QApplication::topLevelWidgets()) {
                auto *box = qobject_cast<QDialog *>(window);
                if (box && box->objectName() == QStringLiteral("unsubscribeConfirmation") && box->isVisible()) confirmation = box;
            }
            return confirmation != nullptr;
        }, 45000), "selection reaches one confirmation before execution")) return 16;
    confirmation->grab().save(QStringLiteral("/tmp/kmail-unsubscribe-single-confirmation.png"));
    auto *cancel = confirmation->findChild<QPushButton *>(QStringLiteral("unsubscribeCancel"));
    if (!check(cancel && confirmation->findChild<QPushButton *>(QStringLiteral("unsubscribeConfirm"))
                   && !confirmation->findChild<QPushButton *>(QStringLiteral("unsubscribeEmail"))
                   && !confirmation->findChild<QPushButton *>(QStringLiteral("unsubscribeWeb")),
                   "selection has one confirmation and no method choices")) return 28;
    cancel->click();
    if (!check(waitFor([&]() { return action->isEnabled(); }), "Cancel ends the whole operation")) return 17;

    // Verify the real reader-box insertion, starting with the synthetic envelope.
    // The Akonadi monitor can load the supplied item's real content afterward.
    // Do not let the viewer store a DKIM result for the synthetic seed content.
    MessageViewer::MessageViewerSettings::self()->setEnabledDkim(false);
    auto *viewer = new MessageViewer::Viewer(&parent, &parent, &actions);
    layout->addWidget(viewer, 1);
    pane->setMaximumHeight(260);
    parent.resize(1000, 720);
    viewer->setHtmlLoadExtDefault(false);
    viewer->setMessageItem(envelope, MimeTreeParser::Force);
    auto *headerButton = viewer->findChild<QToolButton *>(QStringLiteral("unsubscribeHeaderButton"));
    if (!check(headerButton != nullptr, "native message viewer contains the single unsubscribe button")) return 20;
    if (!check(waitFor([&]() { return headerButton->isVisible() && headerButton->isEnabled(); }),
                   "preview header button reflects the displayed message")) return 21;
    auto *content = viewer->findChild<QWidget *>(QStringLiteral("mViewer"));
    if (!check(content && headerButton->mapTo(viewer, QPoint()).y() < content->mapTo(viewer, QPoint()).y(),
                   "unsubscribe button is above the From/To content")) return 22;
    if (!check(headerButton->parentWidget()->height() <= headerButton->height() + 8,
                   "preview button occupies a compact header row")) return 25;
    QTest::qWait(1000);
    viewer->grab().save(QStringLiteral("/tmp/kmail-unsubscribe-single-header.png"));
    parent.grab().save(QStringLiteral("/tmp/kmail-unsubscribe-single-window.png"));
    headerButton->click();
    confirmation = nullptr;
    if (!check(waitFor([&]() {
            for (auto *window : QApplication::topLevelWidgets()) {
                auto *box = qobject_cast<QDialog *>(window);
                if (box && box->objectName() == QStringLiteral("unsubscribeConfirmation") && box->isVisible()) confirmation = box;
            }
            return confirmation != nullptr;
        }, 45000), "preview uses the same confirmation flow for N=1")) return 23;
    cancel = confirmation->findChild<QPushButton *>(QStringLiteral("unsubscribeCancel"));
    if (!check(cancel != nullptr, "preview confirmation has Cancel")) return 29;
    cancel->click();
    if (!check(waitFor([&]() { return headerButton->isEnabled(); }), "Cancel also ends preview operation")) return 24;
    parent.hide();
    return 0;
}
