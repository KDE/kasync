cmake_minimum_required(VERSION 3.0)

project(KAsync VERSION 0.3.0)

# ECM setup
find_package(ECM 5.10.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
set(CMAKE_CXX_STANDARD 14)

include(GenerateExportHeader)
include(ECMGenerateHeaders)
include(ECMGeneratePriFile)
include(CMakePackageConfigHelpers)
include(ECMSetupVersion)
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)

set(QT_REQUIRED_VERSION "5.2.0")

ecm_setup_version(${KAsync_VERSION}
    VARIABLE_PREFIX KASYNC
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kasync_version.h"
    PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KAsyncConfigVersion.cmake"
    SOVERSION 0
)


########### Find packages ###########
find_package(Qt5 ${QT_REQUIRED_VERSION} REQUIRED COMPONENTS Core Test)

########### Targets ###########
add_subdirectory(src)
add_subdirectory(autotests)


########### CMake Config Files ###########
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KAsync")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/KAsyncConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/KAsyncConfig.cmake"
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/KAsyncConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/KAsyncConfigVersion.cmake"
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    COMPONENT Devel
)

install(EXPORT
    KAsyncTargets
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    FILE KAsyncTargets.cmake
    )

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kasync_version.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
