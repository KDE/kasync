project(akonadi2async)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(async_SRCS
    async.cpp
    future.cpp
)

add_library(${PROJECT_NAME} SHARED ${async_SRCS})
target_link_libraries(${PROJECT_NAME} Qt5::Core)
install(TARGETS ${PROJECT_NAME} DESTINATION lib)
