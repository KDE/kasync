include(ECMAddTests)

ecm_add_test(asynctest.cpp
             TEST_NAME asynctest
             LINK_LIBRARIES KF5Async Qt5::Test
)

if (WITH_KJOB)
    ecm_add_test(kjobtest.cpp testkjob.cpp
                 TEST_NAME kjobtest
                 LINK_LIBRARIES KF5Async Qt5::Test KF5::CoreAddons
    )
endif()
