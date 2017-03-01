include(ECMAddTests)

ecm_add_test(asynctest.cpp
             TEST_NAME asynctest
             LINK_LIBRARIES KAsync Qt5::Test
)
