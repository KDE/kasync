set(async_SRCS
    async.cpp
)

add_library(akonadi2async SHARED ${async_SRCS})
target_link_libraries(akonadi2async Qt5::Core)
