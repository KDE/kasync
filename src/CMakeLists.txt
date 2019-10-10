set(kasync_SRCS
    async.cpp
    future.cpp
    debug.cpp
)

set(kasync_priv_HEADERS
    continuations_p.h
    execution_p.h
    executor_p.h
    async_impl.h
    job_impl.h
    debug.h
)

ecm_generate_headers(kasync_HEADERS
    HEADER_NAMES
    Async
    Future
    REQUIRED_HEADERS kasync_HEADERS
)


add_library(KAsync ${kasync_SRCS})

generate_export_header(KAsync BASE_NAME kasync)

target_include_directories(KAsync INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}/KAsync>")
target_include_directories(KAsync PUBLIC "$<BUILD_INTERFACE:${KAsync_SOURCE_DIR}/src>")

target_link_libraries(KAsync
    PUBLIC
        Qt5::Core
)


set_target_properties(KAsync PROPERTIES
    VERSION ${KASYNC_VERSION_STRING}
    SOVERSION ${KASYNC_SOVERSION}
    EXPORT_NAME KAsync
)

ecm_generate_pri_file(BASE_NAME KAsync
    LIB_NAME KAsync
    FILENAME_VAR PRI_FILENAME
)

install(TARGETS
    KAsync
    EXPORT KAsyncTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kasync_export.h
    ${kasync_HEADERS}
    ${kasync_priv_HEADERS}
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/KAsync
    COMPONENT Devel
)

install(FILES
    ${PRI_FILENAME}
    DESTINATION ${ECM_MKSPECS_INSTALL_DIR}
)
