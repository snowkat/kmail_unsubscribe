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
find_package(KF6I18n ${KF_MIN_VERSION} NO_MODULE)


find_package(KPim6MailCommon ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6MessageCore ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6MessageViewer ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6Libkdepim ${KPIM_MIN_VERSION} CONFIG REQUIRED)
find_package(KPim6PimCommonAkonadi ${KPIM_MIN_VERSION} CONFIG REQUIRED)
ki18n_install(po)

set(kmail_unsubscribe_SRCS
    rfc8058validator.cpp
    rfc8058validator.h
    unsubscribemanager.cpp
    unsubscribemanager.h
    unsubscribeplugin.cpp
    unsubscribeplugin.h
    unsubscribeplugininterface.cpp
    unsubscribeplugininterface.h
    oneclickunsubscribejob.cpp
    oneclickunsubscribejob.h
)

ecm_qt_declare_logging_category(
    kmail_unsubscribe_SRCS
    HEADER "unsubscribe_debug.h"
    IDENTIFIER "UnsubscribePlugin"
    CATEGORY_NAME "xyz.datagirl.kpim.unsubscribe"
    DESCRIPTION "Unsubscribe Plugin"
    DEFAULT_SEVERITY Info
    EXPORT 
)

set(BUILD_SHARED_LIBS ON)

kcoreaddons_add_plugin(kmail_unsubscribe
    SOURCES ${kmail_unsubscribe_SRCS}
    INSTALL_NAMESPACE pim6/messageviewer/viewerplugin)
target_link_libraries(kmail_unsubscribe
    KPim6::PimCommon
    KPim6::Libkdepim
    KPim6::PimCommonAkonadi
    KPim6::MessageViewer
    KF6::XmlGui
    KF6::KIOCore
    KF6::KIOGui
    KF6::KIOWidgets
    KF6::I18n)
set_target_properties(kmail_unsubscribe PROPERTIES
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
endif()
