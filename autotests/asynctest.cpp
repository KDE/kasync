/*
 * Copyright 2014  Daniel Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../src/async.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtTest/QTest>
#include <QDebug>

class AsyncTest : public QObject
{
    Q_OBJECT

public:
    AsyncTest()
    {}

    ~AsyncTest()
    {}

private Q_SLOTS:
    void testSyncPromises();
    void testAsyncPromises();
    void testAsyncPromises2();
    void testNestedAsync();
    void testStartValue();

    void testAsyncThen();
    void testSyncThen();
    void testJoinedThen();
    void testVoidThen();

    void testAsyncEach();
    void testSyncEach();
    void testJoinedEach();
    void testVoidEach();

    void testAsyncReduce();
    void testSyncReduce();
    void testJoinedReduce();
    void testVoidReduce();

    void testProgressReporting();
    void testErrorHandler();

    void testChainingRunningJob();
    void testChainingFinishedJob();

    void testLifetimeWithoutHandle();
    void testLifetimeWithHandle();

    void benchmarkSyncThenExecutor();
    void benchmarkAllTests();

private:
    template<typename T>
    class AsyncSimulator {
    public:
        AsyncSimulator(Async::Future<T> &future, const T &result)
            : mFuture(future)
            , mResult(result)
        {
            QObject::connect(&mTimer, &QTimer::timeout,
                             [this]() {
                                 mFuture.setValue(mResult);
                                 mFuture.setFinished();
                             });
            QObject::connect(&mTimer, &QTimer::timeout,
                             [this]() {
                                 delete this;
                             });
            mTimer.setSingleShot(true);
            mTimer.start(200);
        }

    private:
        Async::Future<T> mFuture;
        T mResult;
        QTimer mTimer;
    };
};


template<>
class AsyncTest::AsyncSimulator<void> {
public:
    AsyncSimulator(Async::Future<void> &future)
        : mFuture(future)
    {
        QObject::connect(&mTimer, &QTimer::timeout,
                            [this]() {
                                mFuture.setFinished();
                            });
        QObject::connect(&mTimer, &QTimer::timeout,
                            [this]() {
                                delete this;
                            });
        mTimer.setSingleShot(true);
        mTimer.start(200);
    }

private:
    Async::Future<void> mFuture;
    QTimer mTimer;
};



void AsyncTest::testSyncPromises()
{
    auto baseJob = Async::start<int>(
        [](Async::Future<int> &f) {
            f.setValue(42);
            f.setFinished();
        })
    .then<QString, int>(
        [](int v, Async::Future<QString> &f) {
            f.setValue("Result is " + QString::number(v));
            f.setFinished();
        });

    auto job = baseJob.then<QString, QString>(
        [](const QString &v, Async::Future<QString> &f) {
            f.setValue(v.toUpper());
            f.setFinished();
        });

    Async::Future<QString> future = job.exec();

    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), QString::fromLatin1("RESULT IS 42"));
}

void AsyncTest::testAsyncPromises()
{
    auto job = Async::start<int>(
        [](Async::Future<int> &future) {
            new AsyncSimulator<int>(future, 42);
        });

    Async::Future<int> future = job.exec();

    future.waitForFinished();
    QCOMPARE(future.value(), 42);
}

void AsyncTest::testAsyncPromises2()
{
    bool done = false;

    auto job = Async::start<int>(
        [](Async::Future<int> &future) {
            new AsyncSimulator<int>(future, 42);
        }
    ).then<int, int>([&done](int result, Async::Future<int> &future) {
        done = true;
        future.setValue(result);
        future.setFinished();
    });
    auto future = job.exec();

    QTRY_VERIFY(done);
    QCOMPARE(future.value(), 42);
}

void AsyncTest::testNestedAsync()
{
    bool done = false;

    auto job = Async::start<int>(
        [](Async::Future<int> &future) {
            auto innerJob = Async::start<int>([](Async::Future<int> &innerFuture) {
                new AsyncSimulator<int>(innerFuture, 42);
            }).then<void>([&future](Async::Future<void> &innerThenFuture) {
                future.setFinished();
                innerThenFuture.setFinished();
            });
            innerJob.exec().waitForFinished();
        }
    ).then<int, int>([&done](int result, Async::Future<int> &future) {
        done = true;
        future.setValue(result);
        future.setFinished();
    });
    job.exec();

    QTRY_VERIFY(done);
}

void AsyncTest::testStartValue()
{
    auto job = Async::start<int, int>(
        [](int in, Async::Future<int> &future) {
            future.setValue(in);
            future.setFinished();
        });

    auto future = job.exec(42);
    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), 42);
}





void AsyncTest::testAsyncThen()
{
    auto job = Async::start<int>(
        [](Async::Future<int> &future) {
            new AsyncSimulator<int>(future, 42);
        });

    auto future = job.exec();
    future.waitForFinished();

    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), 42);
}


void AsyncTest::testSyncThen()
{
    auto job = Async::start<int>(
        []() -> int {
            return 42;
        })
    .then<int, int>(
        [](int in) -> int {
            return in * 2;
        });

    auto future = job.exec();
    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), 84);
}

void AsyncTest::testJoinedThen()
{
    auto job1 = Async::start<int, int>(
        [](int in, Async::Future<int> &future) {
            new AsyncSimulator<int>(future, in * 2);
        });

    auto job2 = Async::start<int>(
        [](Async::Future<int> &future) {
            new AsyncSimulator<int>(future, 42);
        })
    .then<int>(job1);

    auto future = job2.exec();
    future.waitForFinished();

    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), 84);
}

void AsyncTest::testVoidThen()
{
    int check = 0;

    auto job = Async::start<void>(
        [&check](Async::Future<void> &future) {
            new AsyncSimulator<void>(future);
            ++check;
        })
    .then<void>(
        [&check](Async::Future<void> &future) {
            new AsyncSimulator<void>(future);
            ++check;
        })
    .then<void>(
        [&check]() {
            ++check;
        });

    auto future = job.exec();
    future.waitForFinished();

    QVERIFY(future.isFinished());
    QCOMPARE(check, 3);
}



void AsyncTest::testAsyncEach()
{
    auto job = Async::start<QList<int>>(
        [](Async::Future<QList<int>> &future) {
            new AsyncSimulator<QList<int>>(future, { 1, 2, 3, 4 });
        })
    .each<QList<int>, int>(
        [](const int &v, Async::Future<QList<int>> &future) {
            new AsyncSimulator<QList<int>>(future, { v + 1 });
        });

    auto future = job.exec();
    future.waitForFinished();

    const QList<int> expected({ 2, 3, 4, 5 });
    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), expected);
}

void AsyncTest::testSyncEach()
{
    auto job = Async::start<QList<int>>(
        []() -> QList<int> {
            return { 1, 2, 3, 4 };
        })
    .each<QList<int>, int>(
        [](const int &v) -> QList<int> {
            return { v + 1 };
        });

    Async::Future<QList<int>> future = job.exec();

    const QList<int> expected({ 2, 3, 4, 5 });
    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), expected);
}

void AsyncTest::testJoinedEach()
{
    auto job1 = Async::start<QList<int>, int>(
        [](int v, Async::Future<QList<int>> &future) {
            new AsyncSimulator<QList<int>>(future, { v * 2 });
        });

    auto job = Async::start<QList<int>>(
        []() -> QList<int> {
            return { 1, 2, 3, 4 };
        })
    .each(job1);

    auto future = job.exec();
    future.waitForFinished();

    const QList<int> expected({ 2, 4, 6, 8 });
    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), expected);
}

void AsyncTest::testVoidEach()
{
    QList<int> check;
    auto job = Async::start<QList<int>>(
        []() -> QList<int> {
            return { 1, 2, 3, 4 };
        }).each<void, int>(
        [&check](const int &v) {
            check << v;
        });

    auto future = job.exec();

    const QList<int> expected({ 1, 2, 3, 4 });
    QVERIFY(future.isFinished());
    QCOMPARE(check, expected);
}





void AsyncTest::testAsyncReduce()
{
    auto job = Async::start<QList<int>>(
        [](Async::Future<QList<int>> &future) {
            new AsyncSimulator<QList<int>>(future, { 1, 2, 3, 4 });
        })
    .reduce<int, QList<int>>(
        [](const QList<int> &list, Async::Future<int> &future) {
            QTimer *timer = new QTimer();
            QObject::connect(timer, &QTimer::timeout,
                             [list, &future]() {
                                 int sum = 0;
                                 for (int i : list) sum += i;
                                 future.setValue(sum);
                                 future.setFinished();
                             });
            QObject::connect(timer, &QTimer::timeout,
                             timer, &QObject::deleteLater);
            timer->setSingleShot(true);
            timer->start(0);
        });

    Async::Future<int> future = job.exec();
    future.waitForFinished();

    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), 10);
}

void AsyncTest::testSyncReduce()
{
    auto job = Async::start<QList<int>>(
        []() -> QList<int> {
            return { 1, 2, 3, 4 };
        })
    .reduce<int, QList<int>>(
        [](const QList<int> &list) -> int {
            int sum = 0;
            for (int i : list) sum += i;
            return sum;
        });

    Async::Future<int> future = job.exec();

    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), 10);
}


void AsyncTest::testJoinedReduce()
{
    auto job1 = Async::start<int, QList<int>>(
        [](const QList<int> &list, Async::Future<int> &future) {
            int sum = 0;
            for (int i : list) sum += i;
            new AsyncSimulator<int>(future, sum);
        });

    auto job = Async::start<QList<int>>(
        []() -> QList<int> {
            return { 1, 2, 3, 4 };
        })
    .reduce(job1);

    auto future = job.exec();
    future.waitForFinished();

    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), 10);
}

void AsyncTest::testVoidReduce()
{
// This must not compile (reduce with void result makes no sense)
#ifdef TEST_BUILD_FAIL
    auto job = Async::start<QList<int>>(
        []() -> QList<int> {
            return { 1, 2, 3, 4 };
        })
    .reduce<void, QList<int>>(
        [](const QList<int> &list) -> int {
            return;
        });

    auto future = job.exec();
    QVERIFY(future.isFinished());
#endif
}


void AsyncTest::testProgressReporting()
{
    static int progress;
    progress = 0;

    auto job = Async::start<void>(
        [](Async::Future<void> &f) {
            QTimer *timer = new QTimer();
            connect(timer, &QTimer::timeout,
                    [&f, timer]() {
                        f.setProgress(++progress);
                        if (progress == 100) {
                            timer->stop();
                            timer->deleteLater();
                            f.setFinished();
                        }
                    });
            timer->start(1);
        });

    int progressCheck = 0;
    Async::FutureWatcher<void> watcher;
    connect(&watcher, &Async::FutureWatcher<void>::futureProgress,
            [&progressCheck](qreal progress) {
                progressCheck++;
                // FIXME: Don't use Q_ASSERT in unit tests
                Q_ASSERT((int) progress == progressCheck);
            });
    watcher.setFuture(job.exec());
    watcher.future().waitForFinished();

    QVERIFY(watcher.future().isFinished());
    QCOMPARE(progressCheck, 100);
}

void AsyncTest::testErrorHandler()
{
    int error = 0;
    auto job = Async::start<int>(
        [](Async::Future<int> &f) {
            f.setError(1, "error");
        })
    .then<int, int>(
        [](int v, Async::Future<int> &f) {
            f.setFinished();
        },
        [&error](int errorCode, const QString &errorMessage) {
            error = errorCode; 
        }
    );
    auto future = job.exec();
    future.waitForFinished();
    QCOMPARE(error, 1);
    QVERIFY(future.isFinished());
}



void AsyncTest::testChainingRunningJob()
{
    int check = 0;

    auto job = Async::start<int>(
        [&check](Async::Future<int> &future) {
            QTimer *timer = new QTimer();
            QObject::connect(timer, &QTimer::timeout,
                             [&future, &check]() {
                                 ++check;
                                 future.setValue(42);
                                 future.setFinished();
                             });
            QObject::connect(timer, &QTimer::timeout,
                             timer, &QObject::deleteLater);
            timer->setSingleShot(true);
            timer->start(500);
        });

    auto future1 = job.exec();
    QTest::qWait(200);

    auto job2 = job.then<int, int>(
        [&check](int in) -> int {
            ++check;
            return in * 2;
        });

    auto future2 = job2.exec();
    QVERIFY(!future1.isFinished());
    future2.waitForFinished();

    QEXPECT_FAIL("", "Chaining new job to a running job no longer executes the new job. "
                     "This is a trade-off for being able to re-execute single job multiple times.",
                 Abort);

    QCOMPARE(check, 2);

    QVERIFY(future1.isFinished());
    QVERIFY(future2.isFinished());
    QCOMPARE(future1.value(), 42);
    QCOMPARE(future2.value(), 84);
}

void AsyncTest::testChainingFinishedJob()
{
    int check = 0;

    auto job = Async::start<int>(
        [&check]() -> int {
            ++check;
            return 42;
        });

    auto future1 = job.exec();
    QVERIFY(future1.isFinished());

    auto job2 = job.then<int, int>(
        [&check](int in) -> int {
            ++check;
            return in * 2;
        });

    auto future2 = job2.exec();
    QVERIFY(future2.isFinished());

    QEXPECT_FAIL("", "Resuming finished job by chaining a new job and calling exec() is no longer suppported. "
                     "This is a trade-off for being able to re-execute single job multiple times.",
                 Abort);

    QCOMPARE(check, 2);

    QCOMPARE(future1.value(), 42);
    QCOMPARE(future2.value(), 84);
}

/*
 * We want to be able to execute jobs without keeping a handle explicitly alive.
 * If the future handle inside the continuation would keep the executor alive, that would probably already work.
 */
void AsyncTest::testLifetimeWithoutHandle()
{
    bool done = false;
    {
        auto job = Async::start<void>([&done](Async::Future<void> &future) {
            QTimer *timer = new QTimer();
            QObject::connect(timer, &QTimer::timeout,
                             [&future, &done]() {
                                 done = true;
                                 future.setFinished();
                             });
            QObject::connect(timer, &QTimer::timeout,
                             timer, &QObject::deleteLater);
            timer->setSingleShot(true);
            timer->start(500);
        });
        job.exec();
    }

    QTRY_VERIFY(done);
}

/*
 * The future handle should keep the executor alive, and the future reference should probably not become invalid inside the continuation,
 * until the job is done (alternatively a copy of the future inside the continuation should work as well).
 */
void AsyncTest::testLifetimeWithHandle()
{
    Async::Future<void> future;
    {
        auto job = Async::start<void>([](Async::Future<void> &future) {
            QTimer *timer = new QTimer();
            QObject::connect(timer, &QTimer::timeout,
                             [&future]() {
                                 future.setFinished();
                             });
            QObject::connect(timer, &QTimer::timeout,
                             timer, &QObject::deleteLater);
            timer->setSingleShot(true);
            timer->start(500);
        });
        future = job.exec();
    }

    QTRY_VERIFY(future.isFinished());
}

void AsyncTest::benchmarkSyncThenExecutor()
{
    auto job = Async::start<int>(
        []() -> int {
            return 0;
        });

    QBENCHMARK {
       job.exec();
    }
}

void AsyncTest::benchmarkAllTests()
{
    QBENCHMARK {
        testSyncPromises();
        testAsyncPromises();
        testAsyncPromises2();
        testNestedAsync();
        testStartValue();

        testAsyncThen();
        testSyncThen();
        testJoinedThen();
        testVoidThen();

        testAsyncEach();
        testSyncEach();
        testJoinedEach();
        testVoidEach();

        testAsyncReduce();
        testSyncReduce();
        testJoinedReduce();
        testVoidReduce();
    }
}



QTEST_MAIN(AsyncTest);

#include "asynctest.moc"
