cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(kmail-unsubscribe VERSION "1.1.0")

include(CTest)

set(KF_MIN_VERSION "6.22.0")
set(KPIM_MIN_VERSION "6.7.0")
set(QT_REQUIRED_VERSION "6.6.0")

# Extra CMake Modules
find_package(ECM ${KF_MIN_VERSION} CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_DIR} ${ECM_KDE_MODULE_DIR})
include(ECMQtDeclareLoggingCategory)
include(KDEInstallDirs)
include(KDECMakeSettings)

find_package(Qt6 ${QT_REQUIRED_VERSION} CONFIG REQUIRED Network Widgets)

find_package(KF6Config ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6GuiAddons ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6XmlGui ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6Parts ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6KIO ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6Codecs ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6I18n ${KF_MIN_VERSION} NO_MODULE)


find_package(KPim6MailCommon ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6IdentityManagementCore ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6MessageComposer ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6MessageCore ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6MessageList ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6MessageViewer ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6Libkdepim ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6PimCommonAkonadi ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6TextEdit ${KPIM_MIN_VERSION} CONFIG REQUIRED)
ki18n_install(po)

set(kmail_unsubscribe_common_SRCS
    rfc8058validator.cpp
    rfc8058validator.h
    unsubscribemanager.cpp
    unsubscribemanager.h
    oneclickunsubscribejob.cpp
    oneclickunsubscribejob.h
    unsubscribecomposerrequest.h
    unsubscribecomposerrequeststore.cpp
    unsubscribecomposerrequeststore.h
    unsubscribeicons.h
    unsubscribeavailability.cpp
    unsubscribeavailability.h
    unsubscribebatch.cpp
    unsubscribebatch.h
    unsubscribeconfirmation.cpp
    unsubscribeconfirmation.h
    unsubscribeplan.h
    unsubscribeworkflow.cpp
    unsubscribeworkflow.h
    unsubscribetrashjob.cpp
    unsubscribetrashjob.h
    webunsubscribeprobe.cpp
    webunsubscribeprobe.h
)

ecm_qt_declare_logging_category(
    kmail_unsubscribe_common_SRCS
    HEADER "unsubscribe_debug.h"
    IDENTIFIER "UnsubscribePlugin"
    CATEGORY_NAME "xyz.datagirl.kpim.unsubscribe"
    DESCRIPTION "Unsubscribe Plugin"
    DEFAULT_SEVERITY Info
    EXPORT 
)

set(BUILD_SHARED_LIBS ON)

add_library(kmail_unsubscribe_common STATIC ${kmail_unsubscribe_common_SRCS})
target_compile_definitions(kmail_unsubscribe_common PUBLIC TRANSLATION_DOMAIN="kmail_unsubscribe")
set_target_properties(kmail_unsubscribe_common PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
target_link_libraries(kmail_unsubscribe_common
    Qt6::Network
    Qt6::Widgets
    KPim6::IdentityManagementCore
    KPim6::MailCommon
    KPim6::MessageCore
    KPim6::PimCommon
    KPim6::Libkdepim
    KPim6::PimCommonAkonadi
    KPim6::MessageViewer
    KF6::KIOCore
    KF6::KIOGui
    KF6::KIOWidgets
    KF6::Codecs
    KF6::I18n)

kcoreaddons_add_plugin(kmail_unsubscribe
    SOURCES
        unsubscribeplugin.cpp
        unsubscribeplugin.h
        unsubscribeplugininterface.cpp
        unsubscribeplugininterface.h
    INSTALL_NAMESPACE pim6/messageviewer/viewerplugin)
target_link_libraries(kmail_unsubscribe
    kmail_unsubscribe_common
    KPim6::MessageViewer
    KF6::XmlGui
    KF6::I18n)
set_target_properties(kmail_unsubscribe PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

kcoreaddons_add_plugin(kmail_unsubscribe_actionplugin
    SOURCES
        unsubscribeactionplugin.cpp
        unsubscribeactionplugin.h
        unsubscriberowactions.cpp
        unsubscriberowactions.h
    INSTALL_NAMESPACE pim6/kmail/mainview)
target_link_libraries(kmail_unsubscribe_actionplugin
    kmail_unsubscribe_common
    Qt6::Widgets
    KPim6::MessageList
    KPim6::PimCommon
    KPim6::PimCommonAkonadi
    KF6::XmlGui
    KF6::I18n)
set_target_properties(kmail_unsubscribe_actionplugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

kcoreaddons_add_plugin(kmail_unsubscribe_editorinitplugin
    SOURCES
        unsubscribecomposerrequest.h
        unsubscribeplugineditorinit.cpp
        unsubscribeplugineditorinit.h
        unsubscribeplugineditorinitinterface.cpp
        unsubscribeplugineditorinitinterface.h
        unsubscribeemailcleanup.cpp
        unsubscribeemailcleanup.h
    INSTALL_NAMESPACE pim6/kmail/plugineditorinit)
target_link_libraries(kmail_unsubscribe_editorinitplugin
    kmail_unsubscribe_common
    Qt6::Widgets
    KPim6::MessageComposer
    KPim6::PimTextEdit)
set_target_properties(kmail_unsubscribe_editorinitplugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_REQUIRED_VERSION} CONFIG REQUIRED Test)
    add_executable(rfc8058validatortest
        autotests/rfc8058validatortest.cpp
        rfc8058validator.cpp
        rfc8058validator.h)
    target_include_directories(rfc8058validatortest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(rfc8058validatortest
        Qt6::Test
        KPim6::MessageViewer)
    set_target_properties(rfc8058validatortest PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON)
    add_test(NAME rfc8058validatortest COMMAND rfc8058validatortest)

    add_executable(unsubscribecomposerrequesttest
        autotests/unsubscribecomposerrequesttest.cpp
        unsubscribecomposerrequest.h
        unsubscribecomposerrequeststore.cpp)
    target_include_directories(unsubscribecomposerrequesttest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(unsubscribecomposerrequesttest Qt6::Test)
    set_target_properties(unsubscribecomposerrequesttest PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON)
    add_test(NAME unsubscribecomposerrequesttest COMMAND unsubscribecomposerrequesttest)

    add_executable(unsubscribeemailcleanuptest
        autotests/unsubscribeemailcleanuptest.cpp
        unsubscribeemailcleanup.cpp)
    target_include_directories(unsubscribeemailcleanuptest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(unsubscribeemailcleanuptest kmail_unsubscribe_common Qt6::Test)
    set_target_properties(unsubscribeemailcleanuptest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    add_test(NAME unsubscribeemailcleanuptest COMMAND unsubscribeemailcleanuptest)

    add_executable(oneclickunsubscribejobtest autotests/oneclickunsubscribejobtest.cpp)
    target_include_directories(oneclickunsubscribejobtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(oneclickunsubscribejobtest kmail_unsubscribe_common Qt6::Test)
    set_target_properties(oneclickunsubscribejobtest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    add_test(NAME oneclickunsubscribejobtest COMMAND oneclickunsubscribejobtest)

    add_executable(unsubscribeworkflowtest
        autotests/unsubscribeworkflowtest.cpp)
    target_include_directories(unsubscribeworkflowtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(unsubscribeworkflowtest
        kmail_unsubscribe_common
        Qt6::Test)
    set_target_properties(unsubscribeworkflowtest PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON)
    add_test(NAME unsubscribeworkflowtest COMMAND unsubscribeworkflowtest)

    add_executable(webunsubscribeprobetest autotests/webunsubscribeprobetest.cpp)
    target_include_directories(webunsubscribeprobetest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(webunsubscribeprobetest kmail_unsubscribe_common Qt6::Test)
    set_target_properties(webunsubscribeprobetest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    add_test(NAME webunsubscribeprobetest COMMAND webunsubscribeprobetest)

    add_executable(unsubscribeconfirmationtest autotests/unsubscribeconfirmationtest.cpp)
    target_include_directories(unsubscribeconfirmationtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(unsubscribeconfirmationtest kmail_unsubscribe_common Qt6::Test)
    set_target_properties(unsubscribeconfirmationtest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    add_test(NAME unsubscribeconfirmationtest COMMAND unsubscribeconfirmationtest)
    set_tests_properties(unsubscribeconfirmationtest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(unsubscribebatchtest autotests/unsubscribebatchtest.cpp)
    target_include_directories(unsubscribebatchtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(unsubscribebatchtest kmail_unsubscribe_common Qt6::Test)
    set_target_properties(unsubscribebatchtest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    add_test(NAME unsubscribebatchtest COMMAND unsubscribebatchtest)
    set_tests_properties(unsubscribebatchtest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # Manual integration check: requires a running Akonadi session and two
    # explicitly supplied item IDs. It is intentionally not a CTest test.
    add_executable(unsubscribenativeprobe EXCLUDE_FROM_ALL autotests/unsubscribenativeprobe.cpp)
    target_link_libraries(unsubscribenativeprobe Qt6::Widgets Qt6::WebEngineCore Qt6::Test KF6::CoreAddons KF6::XmlGui KF6::I18n
                          KPim6::MessageList KPim6::MessageViewer KPim6::PimCommonAkonadi)
    set_target_properties(unsubscribenativeprobe PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    add_dependencies(unsubscribenativeprobe kmail_unsubscribe_actionplugin kmail_unsubscribe)
endif()
