set(kasync_SRCS
    async.cpp
    future.cpp
    debug.cpp
)

set(kasync_priv_HEADERS
    async_impl.h
    debug.h
)

ecm_generate_headers(kasync_HEADERS
    HEADER_NAMES
    Async
    Future
)


add_library(KF5Async ${kasync_SRCS})
add_library(KF5::Async ALIAS KF5Async)

generate_export_header(KF5Async BASE_NAME kasync)

target_include_directories(KF5Async INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF5}/Async>")
target_include_directories(KF5Async PUBLIC "$<BUILD_INTERFACE:${KAsync_SOURCE_DIR}/src>")

target_link_libraries(KF5Async
    PUBLIC
        Qt5::Core
)
if (WITH_KJOB)
    target_link_libraries(KF5Async PUBLIC KF5::CoreAddons)
endif ()


set_target_properties(KF5Async PROPERTIES
    VERSION ${KASYNC_VERSION_STRING}
    SOVERSION ${KASYNC_SOVERSION}
    EXPORT_NAME KAsync
)

ecm_generate_pri_file(BASE_NAME KAsync
    LIB_NAME KF5Async
    FILENAME_VAR PRI_FILENAME
)

install(TARGETS
    KF5Async
    EXPORT KF5AsyncTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kasync_export.h
    ${kasync_HEADERS}
    ${kasync_priv_HEADERS}
    DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/Async
    COMPONENT Devel
)

install(FILES
    ${PRI_FILENAME}
    DESTINATION ${ECM_MKSPECS_INSTALL_DIR}
)
