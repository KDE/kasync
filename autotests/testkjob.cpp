#include "testkjob.h"

TestKJob::TestKJob(int result)
    : mResult(result)
{
    connect(&mTimer, &QTimer::timeout,
            this, &TestKJob::onTimeout);
    mTimer.setSingleShot(true);
    mTimer.setInterval(200);
}

TestKJob::~TestKJob()
{}

void TestKJob::start()
{
    mTimer.start();
}

int TestKJob::result()
{
    return mResult;
}

void TestKJob::onTimeout()
{
    emitResult();
}