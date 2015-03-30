project(libasync)

option(WITH_KJOB "Enable native support for KJob in libasync API (enabled by default)" ON)

if (WITH_KJOB)
    set(MINUMUM_FRAMEWORKS_VERSION "5.8.0")
    find_package(KF5CoreAddons REQUIRED ${MINUMUM_FRAMEWORKS_VERSION})
    add_definitions(-DWITH_KJOB)
endif()

add_subdirectory(src)
add_subdirectory(autotests)