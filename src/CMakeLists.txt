project(akonadi2async)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(async_SRCS
    async.cpp
    future.cpp
)

add_library(${PROJECT_NAME} SHARED ${async_SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Core)
if (WITH_KJOB)
    target_link_libraries(${PROJECT_NAME} PUBLIC KF5::CoreAddons)
endif ()

install(TARGETS ${PROJECT_NAME} ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
