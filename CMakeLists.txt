project(KAsync)

cmake_minimum_required(VERSION 2.8.12)

# ECM setup
find_package(ECM 5.10.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(GenerateExportHeader)
include(ECMGenerateHeaders)
include(ECMGeneratePriFile)
include(ECMPackageConfigHelpers)
include(ECMSetupVersion)
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings)

set(KASYNC_VERSION "4.99.0")

set(KF5_VERSION "5.10.0")
set(QT_REQUIRED_VERSION "5.2.0")

ecm_setup_version(${KASYNC_VERSION}
    VARIABLE_PREFIX KASYNC
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kasync_version.h"
    PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5AsyncConfigVersion.cmake"
    SOVERSION 5
)


########### Find packages ###########
find_package(Qt5 ${QT_REQUIRED_VERSION} REQUIRED COMPONENTS Core Test)

option(WITH_KJOB "Enable native support for KJob in libasync API (enabled by default)" ON)
if (WITH_KJOB)
    find_package(KF5CoreAddons REQUIRED ${KF5_VERSION})
    add_definitions(-DWITH_KJOB)
endif()

########### Targets ###########
add_subdirectory(src)
add_subdirectory(autotests)


########### CMake Config Files ###########
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KF5Async")

ecm_configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/KF5AsyncConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/KF5AsyncConfig.cmake"
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/KF5AsyncConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/KF5AsyncConfigVersion.cmake"
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    COMPONENT Devel
)

install(EXPORT
    KF5AsyncTargets
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    FILE KF5AsyncTargets.cmake
    NAMESPACE KF5::)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kasync_version.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5} COMPONENT Devel
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
