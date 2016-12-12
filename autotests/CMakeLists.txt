include(ECMAddTests)

ecm_add_test(asynctest.cpp
             TEST_NAME asynctest
             LINK_LIBRARIES KF5Async Qt5::Test
)
