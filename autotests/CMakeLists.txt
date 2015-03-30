include_directories(../src ${CMAKE_CURRENT_BINARY_DIR})

add_executable(asynctest asynctest.cpp)
qt5_use_modules(asynctest Test)
target_link_libraries(asynctest akonadi2async Qt5::Core Qt5::Test)

if (WITH_KJOB)
    add_executable(kjobtest kjobtest.cpp testkjob.cpp)
    qt5_use_modules(kjobtest Test)
    target_link_libraries(kjobtest akonadi2async Qt5::Core Qt5::Test KF5::CoreAddons)
endif ()