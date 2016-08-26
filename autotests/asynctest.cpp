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

// Krazy mistakes job.exec() for QDialog::exec() and urges us to use QPointer
//krazy:excludeall=crashy

#include "../src/async.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtTest/QTest>
#include <QDebug>

#include <functional>

#define COMPARERET(actual, expected, retval) \
do {\
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        return retval;\
} while (0)

#define VERIFYRET(statement, retval) \
do {\
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
        return retval;\
} while (0)

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
    void testErrorHandling();
    void testContext();
    void testDoWhile();
    void testAsyncPromises();
    void testNestedAsync();
    // void testNestedJob_data();
    // void testNestedJob();
    void testVoidNestedJob();
    // void testImplicitConversion();
    // void testStartValue();

    // void testAsyncThen();
    // void testAsyncThenClassArgument();
    // void testSyncThen();
    // void testJoinedThen();
    // void testVoidThen();
    // void testMemberThen();
    // void testSyncMemberThen();
    // void testSyncVoidMemberThen();
    //
    void testAsyncEach();
    void testAsyncSerialEach();
    // void testSyncEach();
    // void testNestedEach();
    // void testJoinedEach();
    // void testVoidEachThen();
    // void testAsyncVoidEachThen();
    //
    // void testAsyncReduce();
    // void testSyncReduce();
    // void testJoinedReduce();
    // void testVoidReduce();
    //
    // void testProgressReporting();
    // void testErrorHandler();
    // void testErrorPropagation();
    // void testErrorHandlerAsync();
    // void testErrorPropagationAsync();
    // void testNestedErrorPropagation();
    // void testEachErrorHandler();
    //
    // void testChainingRunningJob();
    // void testChainingFinishedJob();
    //
    // void testLifetimeWithoutHandle();
    // void testLifetimeWithHandle();
    //
    // void testErrorTask_data();
    // void testErrorTask();
    //
    void benchmarkSyncThenExecutor();
    void benchmarkFutureThenExecutor();
    void benchmarkThenExecutor();

private:
    template<typename T>
    class AsyncSimulator {
    public:
        AsyncSimulator(KAsync::Future<T> &future, const T &result)
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

        AsyncSimulator(KAsync::Future<T> &future, std::function<void(KAsync::Future<T>&)> callback)
            : mFuture(future)
            , mCallback(callback)
        {
            QObject::connect(&mTimer, &QTimer::timeout,
                             [this]() {
                                 mCallback(mFuture);
                             });
            QObject::connect(&mTimer, &QTimer::timeout,
                             [this]() {
                                 delete this;
                             });
            mTimer.setSingleShot(true);
            mTimer.start(200);
        }

    private:
        KAsync::Future<T> mFuture;
        std::function<void(KAsync::Future<T>&)> mCallback;
        T mResult;
        QTimer mTimer;
    };

    class MemberTest
    {
    public:
        MemberTest()
            : mFoo(-1)
        {
        }

        void syncFoo(int foo)
        {
            mFoo = foo;
        }

        int syncFooRet(int foo)
        {
            return ++foo;
        }

        void asyncFoo(int foo, KAsync::Future<int> &future)
        {
            new AsyncSimulator<int>(future, ++foo);
        }

        int mFoo;
    };
};

template<>
class AsyncTest::AsyncSimulator<void> {
public:
    AsyncSimulator(KAsync::Future<void> &future)
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
    KAsync::Future<void> mFuture;
    QTimer mTimer;
};


void AsyncTest::testSyncPromises()
{
    {
        auto future = KAsync::syncStart<int>(
            []() {
                return 42;
            }).exec();
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), 42);
    }

    {
        auto future = KAsync::start<int>(
            [](KAsync::Future<int> &f) {
                f.setResult(42);
            }).exec();
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), 42);
    }

    //Sync start
    {
        auto future = KAsync::start<int>([] {
                return KAsync::value<int>(42);
            }).exec();
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), 42);
    }

    //Sync start
    {
        bool called = false;
        auto future = KAsync::start<void>(
            [&called] {
                called = true;
                return KAsync::null<void>();
            }).exec();
        QVERIFY(future.isFinished());
        QVERIFY(called);
    }
    //void
    {
        auto future = KAsync::start<void>(
            []() {
                return KAsync::null<void>();
            }).exec();
        QVERIFY(future.isFinished());
    }

    //value
    {
        auto future = KAsync::value<int>(42).exec();
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), 42);
    }

    //Sync then
    {
        auto job = KAsync::value<int>(42);
        auto future = job.then<int, int>([](int value) {
            return KAsync::value<int>(value);
        }).exec();
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), 42);
    }

    //Job then
    {
        auto job = KAsync::value<int>(42);
        auto future = job.then<QString, int>([](int value) {
            return KAsync::value<QString>(QString::number(value));
        }).exec();
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), QString::number(42));
    }

    //void Job then
    {
        bool continuationCalled = false;
        auto job = KAsync::null<void>();
        auto future = job.then<void>([&continuationCalled] {
            return KAsync::start<void>([&continuationCalled] {
                continuationCalled = true;
                return KAsync::null<void>();
            });
        }).exec();
        QVERIFY(future.isFinished());
        QVERIFY(continuationCalled);
    }

    //Nested job then
    {
        auto job = KAsync::value<int>(42);
        auto future = job.then<QString, int>(
            KAsync::start<QString, int>([](int i) {
                return KAsync::value<QString>(QString::number(i));
            })
        ).exec();
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), QString::number(42));
    }

    //Convert to void
    {
        KAsync::Job<void> job = KAsync::start<int>(
            [] {
                return KAsync::value<int>(42);
            }).then<int, int>([](int i) {
                return KAsync::value<int>(i);
            });
        KAsync::Future<void> future = job.exec();
        QVERIFY(future.isFinished());
    }

    //Job then types
    {
        KAsync::Job<int, double> job1 = KAsync::start<int, double>(
            [](double i)  {
                return KAsync::value<int>(i);
            });

        KAsync::Job<QString, double> job2 = job1.then<QString, int>([](int value) {
            return KAsync::start<QString>([value]() {
                return KAsync::value<QString>(QString::number(value));
            });
        });
        double input = 42;
        KAsync::Future<QString> future = job2.exec(input);
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), QString::number(42));
    }

    //This is useful to be able to spawn different subjobs depending on the initial input value that the continuation gets.
    {
        auto future = KAsync::start<int, bool>(
            [] (bool i) {
                if (i) {
                    return KAsync::value(42);
                } else {
                    return KAsync::error<int>(KAsync::Error("foo"));
                }
            }).exec(true);
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), 42);
    }

    {
        auto baseJob = KAsync::value<int>(42)
        .then<QString, int>(
            [](int v, KAsync::Future<QString> &f) {
                f.setValue(QLatin1String("Result is ") + QString::number(v));
                f.setFinished();
            });

        auto job = baseJob.then<QString, QString>(
            [](const QString &v, KAsync::Future<QString> &f) {
                f.setValue(v.toUpper());
                f.setFinished();
            });
        KAsync::Future<QString> future = job.exec();
        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), QString::fromLatin1("RESULT IS 42"));
    }
}

void AsyncTest::testErrorHandling()
{
    //Failing job
    {
        auto future = KAsync::start<int>(
            [](KAsync::Future<int> &f) {
                f.setError({1, "error"});
            }).exec();
        QVERIFY(future.isFinished());
        QCOMPARE(future.errorCode(), 1);
        QCOMPARE(future.errorMessage().toUtf8(), QByteArray("error"));
    }

    //Call error handler
    {
        bool errorHandlerCalled = false;
        auto future = KAsync::error<int>({1, "error"})
            .then<int, int>([&errorHandlerCalled](const KAsync::Error &error, int) {
                qWarning() << "Handler called";
                errorHandlerCalled = true;
                COMPARERET(error.errorCode, 1, KAsync::error<int>(error));
                return KAsync::error<int>(error);
            }).exec();
        QVERIFY(future.isFinished());
        QVERIFY(errorHandlerCalled);
        QCOMPARE(future.errors().first(), KAsync::Error(1, "error"));
    } 

    //Propagate error
    {
        bool errorHandlerCalled = false;
        auto future = KAsync::error<int>({1, "error"})
        .then<int, int>(
            [](int) {
                VERIFYRET(false, KAsync::null<int>());
                return KAsync::null<int>();
            })
        .then<void, int>([&errorHandlerCalled](const KAsync::Error &error, int) {
                errorHandlerCalled = true;
                COMPARERET(error.errorCode, 1, KAsync::error<void>(error));
                return KAsync::error<void>(error);
            })
        .exec();

        QVERIFY(future.isFinished());
        QVERIFY(errorHandlerCalled);
        QCOMPARE(future.errors().first(), KAsync::Error(1, "error"));
    } 

    //Propagate error
    {
        bool errorHandlerCalled1 = false;
        bool errorHandlerCalled2 = false;
        auto future = KAsync::error<int>({1, "error"})
        .then<int, int>(
            [&errorHandlerCalled1](const KAsync::Error &error, int) {
                errorHandlerCalled1 = true;
                COMPARERET(error.errorCode, 1, KAsync::error<int>(error));
                return KAsync::error<int>(error);
            })
        .then<void, int>([&errorHandlerCalled2](const KAsync::Error &error, int) {
                errorHandlerCalled2 = true;
                COMPARERET(error.errorCode, 1, KAsync::error<void>(error));
                return KAsync::error<void>(error);
            })
        .exec();

        QVERIFY(future.isFinished());
        QVERIFY(errorHandlerCalled1);
        QVERIFY(errorHandlerCalled2);
        QCOMPARE(future.errors().first(), KAsync::Error(1, "error"));
    } 

    //Reconcile error
    {
        bool errorHandlerCalled1 = false;
        bool errorHandlerCalled2 = false;
        auto future = KAsync::error<int>({1, "error"})
        .then<int, int>(
            [&errorHandlerCalled1](const KAsync::Error &error, int) {
                errorHandlerCalled1 = true;
                COMPARERET(error, KAsync::Error(1, "error"), KAsync::null<int>());
                return KAsync::null<int>();
            })
        .then<void, int>([&errorHandlerCalled2](const KAsync::Error &error, int) {
                VERIFYRET(!error, KAsync::null<void>());
                errorHandlerCalled2 = true;
                return KAsync::null<void>();
            })
        .exec();

        QVERIFY(errorHandlerCalled1);
        QVERIFY(errorHandlerCalled2);
        QVERIFY(future.isFinished());
        QVERIFY(!future.hasError());
    } 

    //Propagate value on error
    {
        KAsync::Future<int> future = KAsync::value<int>(1)
        .onError([](const KAsync::Error &error) {
                Q_UNUSED(error);
                QVERIFY(false);
            })
        .exec();

        QVERIFY(future.isFinished());
        QCOMPARE(future.value(), 1);
    } 
}

void AsyncTest::testContext()
{

    QWeakPointer<QObject> refToObj;
    {
        KAsync::Job<int> job = KAsync::null<int>();
        {
            auto contextObject = QSharedPointer<QObject>::create();
            refToObj = contextObject.toWeakRef();
            QVERIFY(refToObj);
            job = KAsync::start<int>(
                [](KAsync::Future<int> &future) {
                    new AsyncSimulator<int>(future, 42);
                });
            job.addToContext(contextObject);

            //Ensure the context survives for the whole duration of the job
            job = job.then<int>([](KAsync::Future<int> &future) {
                    new AsyncSimulator<int>(future, 42);
                });
        }

        QVERIFY(refToObj);

        {
            //Ensure the context survives copies
            auto job2 = job;
            job = KAsync::null<int>();
            KAsync::Future<int> future = job2.exec();
            QVERIFY(refToObj);
            future.waitForFinished();
        }
    }
    QVERIFY(!refToObj);
}

void AsyncTest::testDoWhile()
{
    int i = 0;
    auto future = KAsync::dowhile([&i]() {
        i++;
        if (i < 5) {
            return KAsync::value(KAsync::Continue);
        }
        return KAsync::value(KAsync::Break);
    })
    .exec();
    future.waitForFinished();
    QVERIFY(future.isFinished());
    QCOMPARE(i, 5);
}

void AsyncTest::testAsyncPromises()
{
    auto job = KAsync::start<int>(
        [](KAsync::Future<int> &future) {
            new AsyncSimulator<int>(future, 42);
        });

    KAsync::Future<int> future = job.exec();

    future.waitForFinished();
    QCOMPARE(future.value(), 42);
}

void AsyncTest::testNestedAsync()
{
    bool done = false;

    auto job = KAsync::start<int>(
        [](KAsync::Future<int> &future) {
            auto innerJob = KAsync::start<int>([](KAsync::Future<int> &innerFuture) {
                new AsyncSimulator<int>(innerFuture, 42);
            }).then<void>([&future](KAsync::Future<void> &innerThenFuture) {
                future.setFinished();
                innerThenFuture.setFinished();
            });
            innerJob.exec().waitForFinished();
        }
    ).then<int, int>([&done](int result, KAsync::Future<int> &future) {
        done = true;
        future.setValue(result);
        future.setFinished();
    });
    job.exec();

    QTRY_VERIFY(done);
}
//
// void AsyncTest::testNestedJob_data()
// {
//     QTest::addColumn<bool>("pass");
//     QTest::newRow("pass") << true;
//     QTest::newRow("fail") << false;
// }
//
// void AsyncTest::testNestedJob()
// {
//     QFETCH(bool, pass);
//     bool innerDone = false;
//
//     auto job = KAsync::start<int>(
//         [&innerDone]() {
//             return KAsync::start<int>([&innerDone]() {
//                 innerDone = true;
//                 return 42;
//             });
//         }
//     ).then<int, int>([&innerDone, pass](int in) -> KAsync::Job<int> {
//         if (pass) {
//             return KAsync::start<int>([&innerDone, in]() {
//                 innerDone = true;
//                 return in;
//             });
//         } else {
//             return KAsync::error<int>(3, QLatin1String("foobar"));
//         }
//     });
//     auto future = job.exec();
//
//     if (pass) {
//         QVERIFY(innerDone);
//         QCOMPARE(future.value(), 42);
//         QCOMPARE(future.errorCode(), 0);
//     } else {
//         QCOMPARE(future.errorCode(), 3);
//     }
// }
//
void AsyncTest::testVoidNestedJob()
{
    bool innerDone1 = false;
    bool innerDone2 = false;
    bool innerDone3 = false;
    auto job = KAsync::start<void>(
        [&innerDone1]() -> KAsync::Job<void> {
            return KAsync::start<void>([&innerDone1]() {
                innerDone1 = true;
                return KAsync::null<void>();
            });
        }
    )
    .then<void>([&innerDone2, &innerDone3]() -> KAsync::Job<void> {
        return KAsync::start<void>([&innerDone2]() {
            innerDone2 = true;
            return KAsync::null<void>();
        })
        .then<void>([&innerDone3]() {
            innerDone3 = true;
            return KAsync::null<void>();
        });
    });
    auto future = job.exec();
    future.waitForFinished();
    QCOMPARE(future.errorCode(), 0);
    QVERIFY(innerDone1);
    QVERIFY(innerDone2);
    QVERIFY(innerDone3);
}
//
// void AsyncTest::testImplicitConversion()
// {
//     auto job = KAsync::start<int>(
//         []() -> int {
//             return 42;
//         })
//     .then<void, int>(
//         [](int in, KAsync::Future<void> &future){
//             future.setFinished();
//         });
//     KAsync::Job<void> finalJob = job;
// }
//
// void AsyncTest::testStartValue()
// {
//     auto job = KAsync::start<int, int>(
//         [](int in, KAsync::Future<int> &future) {
//             future.setValue(in);
//             future.setFinished();
//         });
//
//     auto future = job.exec(42);
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.value(), 42);
// }
//
//
//
//
//
// void AsyncTest::testAsyncThen()
// {
//     auto job = KAsync::start<int>(
//         [](KAsync::Future<int> &future) {
//             new AsyncSimulator<int>(future, 42);
//         });
//
//     auto future = job.exec();
//     future.waitForFinished();
//
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.value(), 42);
// }
//
// void AsyncTest::testAsyncThenClassArgument()
// {
//     struct Test {};
//
//     auto job = KAsync::start<QSharedPointer<Test>>(
//         [](KAsync::Future<QSharedPointer<Test> > &future) {
//             new AsyncSimulator<QSharedPointer<Test> >(future, QSharedPointer<Test>::create());
//         }).then<void, QSharedPointer<Test> >(
//         [](QSharedPointer<Test> i, KAsync::Future<void> &future) {
//             Q_UNUSED(i);
//             future.setFinished();
//         }
//     );
//
//     auto future = job.exec();
//     future.waitForFinished();
//
//     QVERIFY(future.isFinished());
// }
//
// void AsyncTest::testSyncThen()
// {
//     auto job = KAsync::start<int>(
//         []() -> int {
//             return 42;
//         })
//     .then<int, int>(
//         [](int in) -> int {
//             return in * 2;
//         });
//
//     auto future = job.exec();
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.value(), 84);
// }
//
// void AsyncTest::testJoinedThen()
// {
//     auto job1 = KAsync::start<int, int>(
//         [](int in, KAsync::Future<int> &future) {
//             new AsyncSimulator<int>(future, in * 2);
//         });
//
//     auto job2 = KAsync::start<int>(
//         [](KAsync::Future<int> &future) {
//             new AsyncSimulator<int>(future, 42);
//         })
//     .then<int>(job1);
//
//     auto future = job2.exec();
//     future.waitForFinished();
//
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.value(), 84);
// }
//
// void AsyncTest::testVoidThen()
// {
//     int check = 0;
//
//     auto job = KAsync::start<void>(
//         [&check](KAsync::Future<void> &future) {
//             new AsyncSimulator<void>(future);
//             ++check;
//         })
//     .then<void>(
//         [&check](KAsync::Future<void> &future) {
//             new AsyncSimulator<void>(future);
//             ++check;
//         })
//     .then<void>(
//         [&check]() {
//             ++check;
//         });
//
//     auto future = job.exec();
//     future.waitForFinished();
//
//     QVERIFY(future.isFinished());
//     QCOMPARE(check, 3);
// }
//
//
// void AsyncTest::testMemberThen()
// {
//     MemberTest memberTest;
//
//     auto job = KAsync::start<int>(
//         []() -> int {
//             return 42;
//         })
//     .then<MemberTest, int, int>(&memberTest, &MemberTest::asyncFoo);
//
//     auto future = job.exec();
//     future.waitForFinished();
//
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.value(), 43);
// }
//
// void AsyncTest::testSyncMemberThen()
// {
//     MemberTest memberTest;
//
//     auto job = KAsync::start<int>(
//         []() -> int {
//             return 42;
//         })
//     .then<MemberTest, int, int>(&memberTest, &MemberTest::syncFooRet);
//
//     auto future = job.exec();
//     future.waitForFinished();
//
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.value(), 43);
// }
//
// void AsyncTest::testSyncVoidMemberThen()
// {
//     MemberTest memberTest;
//
//     auto job = KAsync::start<int>(
//         []() -> int {
//             return 42;
//         })
//     .then<MemberTest, void, int>(&memberTest, &MemberTest::syncFoo);
//
//     auto future = job.exec();
//     future.waitForFinished();
//
//     QVERIFY(future.isFinished());
//     QCOMPARE(memberTest.mFoo, 42);
// }
//
//
//
void AsyncTest::testAsyncEach()
{
    {
        auto job = KAsync::value<std::vector<int>>({1});
        auto future = job.each<void>([](int i) {
                Q_UNUSED(i);
                return KAsync::null<void>();
            }).exec();
        QVERIFY(future.isFinished());
    }

    const QList<int> expected({1, 2, 3});

    auto job = KAsync::value<QList<int>>({1, 2, 3});
    {
        QList<int> result;
        //This is the all manual version
        auto subjob = KAsync::forEach<QList<int>>(
                KAsync::start<void, int>([&result](int i) {
                    result << i;
                    return KAsync::null<void>();
                })
            );
        auto future = job.then<void, QList<int>>(
                subjob
            ).exec();
        future.waitForFinished();
        QVERIFY(future.isFinished());
        QCOMPARE(result, expected);

    }
    {
        QList<int> result;
        //An this is the convenience wrapper
        auto future = job.each([&result](int i) {
                result << i;
                return KAsync::null<void>();
            }).exec();
        future.waitForFinished();
        QVERIFY(future.isFinished());
        QCOMPARE(result, expected);
    }
}

void AsyncTest::testAsyncSerialEach()
{
    {
        auto job = KAsync::value<std::vector<int>>({1});
        auto future = job.serialEach<void>([](int i) {
                Q_UNUSED(i);
                return KAsync::null<void>();
            }).exec();

    }

    const QList<int> expected({1, 2, 3});

    auto job = KAsync::value<QList<int>>({1, 2, 3});
    {
        QList<int> result;
        auto subjob = KAsync::serialForEach<QList<int>>(
                KAsync::start<void, int>([&](int i) {
                    result << i;
                    return KAsync::null<void>();
                })
            );
        auto future = job.then<void, QList<int>>(subjob).exec();
        future.waitForFinished();
        QVERIFY(future.isFinished());
        QCOMPARE(result, expected);
    }
    {
        QList<int> result;
        //An this is the convenience wrapper
        auto future = job.serialEach([&result](int i) {
                result << i;
                return KAsync::null<void>();
            }).exec();
        future.waitForFinished();
        QVERIFY(future.isFinished());
        QCOMPARE(result, expected);
    }
}
//
// void AsyncTest::testSyncEach()
// {
//     auto job = KAsync::start<QList<int>>(
//         []() -> QList<int> {
//             return { 1, 2, 3, 4 };
//         })
//     .each<QList<int>, int>(
//         [](const int &v) -> QList<int> {
//             return { v + 1 };
//         });
//
//     KAsync::Future<QList<int>> future = job.exec();
//
//     const QList<int> expected({ 2, 3, 4, 5 });
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.value(), expected);
// }
//
// void AsyncTest::testNestedEach()
// {
//     auto job = KAsync::start<QList<int>>(
//         []() -> QList<int> {
//             return { 1, 2, 3, 4 };
//         })
//     .each<QList<int>, int>(
//         [](const int &v) {
//             return KAsync::start<QList<int> >([v]() -> QList<int> {
//                 return { v + 1 };
//             });
//         });
//
//     KAsync::Future<QList<int>> future = job.exec();
//
//     const QList<int> expected({ 2, 3, 4, 5 });
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.value(), expected);
// }
//
// void AsyncTest::testJoinedEach()
// {
//     auto job1 = KAsync::start<QList<int>, int>(
//         [](int v, KAsync::Future<QList<int>> &future) {
//             new AsyncSimulator<QList<int>>(future, { v * 2 });
//         });
//
//     auto job = KAsync::start<QList<int>>(
//         []() -> QList<int> {
//             return { 1, 2, 3, 4 };
//         })
//     .each(job1);
//
//     auto future = job.exec();
//     future.waitForFinished();
//
//     const QList<int> expected({ 2, 4, 6, 8 });
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.value(), expected);
// }
//
// void AsyncTest::testVoidEachThen()
// {
//     QList<int> check;
//     auto job = KAsync::start<QList<int>>(
//         []() -> QList<int> {
//             return { 1, 2, 3, 4 };
//         }).each<void, int>(
//         [&check](const int &v) {
//             check << v;
//         }).then<void>([](){});
//
//     auto future = job.exec();
//
//     const QList<int> expected({ 1, 2, 3, 4 });
//     QVERIFY(future.isFinished());
//     QCOMPARE(check, expected);
// }
//
// void AsyncTest::testAsyncVoidEachThen()
// {
//     bool completedJob = false;
//     QList<int> check;
//     auto job = KAsync::start<QList<int>>(
//         [](KAsync::Future<QList<int> > &future) {
//             new AsyncSimulator<QList<int>>(future, { 1, 2, 3, 4 });
//         }).each<void, int>(
//         [&check](const int &v, KAsync::Future<void> &future) {
//             check << v;
//             new AsyncSimulator<void>(future);
//         }).then<void>([&completedJob](KAsync::Future<void> &future) {
//             completedJob = true;
//             future.setFinished();
//         });
//
//     auto future = job.exec();
//     future.waitForFinished();
//
//     const QList<int> expected({ 1, 2, 3, 4 });
//     QVERIFY(future.isFinished());
//     QVERIFY(completedJob);
//     QCOMPARE(check, expected);
// }
//
// void AsyncTest::testProgressReporting()
// {
//     static int progress;
//     progress = 0;
//
//     auto job = KAsync::start<void>(
//         [](KAsync::Future<void> &f) {
//             QTimer *timer = new QTimer();
//             connect(timer, &QTimer::timeout,
//                     [&f, timer]() {
//                         f.setProgress(++progress);
//                         if (progress == 100) {
//                             timer->stop();
//                             timer->deleteLater();
//                             f.setFinished();
//                         }
//                     });
//             timer->start(1);
//         });
//
//     int progressCheck = 0;
//     KAsync::FutureWatcher<void> watcher;
//     connect(&watcher, &KAsync::FutureWatcher<void>::futureProgress,
//             [&progressCheck](qreal progress) {
//                 progressCheck++;
//                 // FIXME: Don't use Q_ASSERT in unit tests
//                 Q_ASSERT((int) progress == progressCheck);
//             });
//     watcher.setFuture(job.exec());
//     watcher.future().waitForFinished();
//
//     QVERIFY(watcher.future().isFinished());
//     QCOMPARE(progressCheck, 100);
// }
//
// void AsyncTest::testErrorHandler()
// {
//
//     {
//         auto job = KAsync::start<int>(
//             [](KAsync::Future<int> &f) {
//                 f.setError(1, QLatin1String("error"));
//             });
//
//         auto future = job.exec();
//         QVERIFY(future.isFinished());
//         QCOMPARE(future.errorCode(), 1);
//         QCOMPARE(future.errorMessage(), QString::fromLatin1("error"));
//     }
//
//     {
//         int error = 0;
//         auto job = KAsync::start<int>(
//             [](KAsync::Future<int> &f) {
//                 f.setError(1, QLatin1String("error"));
//             },
//             [&error](int errorCode, const QString &errorMessage) {
//                 Q_UNUSED(errorMessage);
//                 error += errorCode;
//             }
//         );
//
//         auto future = job.exec();
//         QVERIFY(future.isFinished());
//         QCOMPARE(error, 1);
//         QCOMPARE(future.errorCode(), 1);
//         QCOMPARE(future.errorMessage(), QString::fromLatin1("error"));
//     }
// }
//
// void AsyncTest::testErrorPropagation()
// {
//     int error = 0;
//     bool called = false;
//     auto job = KAsync::start<int>(
//         [](KAsync::Future<int> &f) {
//             f.setError(1, QLatin1String("error"));
//         })
//     .then<int, int>(
//         [&called](int v, KAsync::Future<int> &f) {
//             Q_UNUSED(v);
//             called = true;
//             f.setFinished();
//         },
//         [&error](int errorCode, const QString &errorMessage) {
//             Q_UNUSED(errorMessage);
//             error += errorCode;
//         }
//     );
//     auto future = job.exec();
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.errorCode(), 1);
//     QCOMPARE(future.errorMessage(), QString::fromLatin1("error"));
//     QCOMPARE(called, false);
//     QCOMPARE(error, 1);
// }
//
// void AsyncTest::testErrorHandlerAsync()
// {
//     {
//         auto job = KAsync::start<int>(
//             [](KAsync::Future<int> &f) {
//                 new AsyncSimulator<int>(f,
//                     [](KAsync::Future<int> &f) {
//                         f.setError(1, QLatin1String("error"));
//                     }
//                 );
//             }
//         );
//
//         auto future = job.exec();
//         future.waitForFinished();
//
//         QVERIFY(future.isFinished());
//         QCOMPARE(future.errorCode(), 1);
//         QCOMPARE(future.errorMessage(), QString::fromLatin1("error"));
//     }
//
//     {
//         int error = 0;
//         auto job = KAsync::start<int>(
//             [](KAsync::Future<int> &f) {
//                 new AsyncSimulator<int>(f,
//                     [](KAsync::Future<int> &f) {
//                         f.setError(1, QLatin1String("error"));
//                     }
//                 );
//             },
//             [&error](int errorCode, const QString &errorMessage) {
//                 Q_UNUSED(errorMessage);
//                 error += errorCode;
//             }
//         );
//
//         auto future = job.exec();
//         future.waitForFinished();
//
//         QVERIFY(future.isFinished());
//         QCOMPARE(error, 1);
//         QCOMPARE(future.errorCode(), 1);
//         QCOMPARE(future.errorMessage(), QString::fromLatin1("error"));
//     }
// }
//
// void AsyncTest::testErrorPropagationAsync()
// {
//     int error = 0;
//     bool called = false;
//     auto job = KAsync::start<int>(
//         [](KAsync::Future<int> &f) {
//             new AsyncSimulator<int>(f,
//                 [](KAsync::Future<int> &f) {
//                     f.setError(1, QLatin1String("error"));
//                 }
//             );
//         })
//     .then<int, int>(
//         [&called](int v, KAsync::Future<int> &f) {
//             Q_UNUSED(v);
//             called = true;
//             f.setFinished();
//         },
//         [&error](int errorCode, const QString &errorMessage) {
//             Q_UNUSED(errorMessage);
//             error += errorCode;
//         }
//     );
//
//     auto future = job.exec();
//     future.waitForFinished();
//
//     QVERIFY(future.isFinished());
//     QCOMPARE(future.errorCode(), 1);
//     QCOMPARE(future.errorMessage(), QString::fromLatin1("error"));
//     QCOMPARE(called, false);
//     QCOMPARE(error, 1);
// }
//
// void AsyncTest::testEachErrorHandler()
// {
//     auto job = KAsync::start<QList<int>>(
//         []() -> QList<int> {
//             return { 1, 2, 3, 4 };
//         })
//     .each<QList<int>, int>(
//         [](const int &v, KAsync::Future<QList<int>> &f) {
//             if ((v % 2) == 0) {
//                 f.setError(1, QString::fromLatin1("error"));
//             } else {
//                 f.setValue({ v + 1 });
//                 f.setFinished();
//             }
//         });
//
//     KAsync::Future<QList<int>> future = job.exec();
//     future.waitForFinished();
//
//     QVERIFY(future.isFinished());
//     const QList<int> expected({ 2, 4 });
//     QCOMPARE(future.value(), expected);
//
//     QCOMPARE(future.errorCode(), 1);
//     QCOMPARE(future.errorMessage(), QString::fromLatin1("error"));
//     QCOMPARE(future.errors().size(), 2);
// }
//
// void AsyncTest::testChainingRunningJob()
// {
//     int check = 0;
//
//     auto job = KAsync::start<int>(
//         [&check](KAsync::Future<int> &future) {
//             QTimer *timer = new QTimer();
//             QObject::connect(timer, &QTimer::timeout,
//                              [&future, &check]() {
//                                  ++check;
//                                  future.setValue(42);
//                                  future.setFinished();
//                              });
//             QObject::connect(timer, &QTimer::timeout,
//                              timer, &QObject::deleteLater);
//             timer->setSingleShot(true);
//             timer->start(500);
//         });
//
//     auto future1 = job.exec();
//     QTest::qWait(200);
//
//     auto job2 = job.then<int, int>(
//         [&check](int in) -> int {
//             ++check;
//             return in * 2;
//         });
//
//     auto future2 = job2.exec();
//     QVERIFY(!future1.isFinished());
//     future2.waitForFinished();
//
//     QEXPECT_FAIL("", "Chaining new job to a running job no longer executes the new job. "
//                      "This is a trade-off for being able to re-execute single job multiple times.",
//                  Abort);
//
//     QCOMPARE(check, 2);
//
//     QVERIFY(future1.isFinished());
//     QVERIFY(future2.isFinished());
//     QCOMPARE(future1.value(), 42);
//     QCOMPARE(future2.value(), 84);
// }
//
// void AsyncTest::testChainingFinishedJob()
// {
//     int check = 0;
//
//     auto job = KAsync::start<int>(
//         [&check]() -> int {
//             ++check;
//             return 42;
//         });
//
//     auto future1 = job.exec();
//     QVERIFY(future1.isFinished());
//
//     auto job2 = job.then<int, int>(
//         [&check](int in) -> int {
//             ++check;
//             return in * 2;
//         });
//
//     auto future2 = job2.exec();
//     QVERIFY(future2.isFinished());
//
//     QEXPECT_FAIL("", "Resuming finished job by chaining a new job and calling exec() is no longer suppported. "
//                      "This is a trade-off for being able to re-execute single job multiple times.",
//                  Abort);
//
//     QCOMPARE(check, 2);
//
//     QCOMPARE(future1.value(), 42);
//     QCOMPARE(future2.value(), 84);
// }
//
// #<{(|
//  * We want to be able to execute jobs without keeping a handle explicitly alive.
//  * If the future handle inside the continuation would keep the executor alive, that would probably already work.
//  |)}>#
// void AsyncTest::testLifetimeWithoutHandle()
// {
//     bool done = false;
//     {
//         auto job = KAsync::start<void>([&done](KAsync::Future<void> &future) {
//             QTimer *timer = new QTimer();
//             QObject::connect(timer, &QTimer::timeout,
//                              [&future, &done]() {
//                                  done = true;
//                                  future.setFinished();
//                              });
//             QObject::connect(timer, &QTimer::timeout,
//                              timer, &QObject::deleteLater);
//             timer->setSingleShot(true);
//             timer->start(500);
//         });
//         job.exec();
//     }
//
//     QTRY_VERIFY(done);
// }
//
// #<{(|
//  * The future handle should keep the executor alive, and the future reference should probably not become invalid inside the continuation,
//  * until the job is done (alternatively a copy of the future inside the continuation should work as well).
//  |)}>#
// void AsyncTest::testLifetimeWithHandle()
// {
//     KAsync::Future<void> future;
//     {
//         auto job = KAsync::start<void>([](KAsync::Future<void> &future) {
//             QTimer *timer = new QTimer();
//             QObject::connect(timer, &QTimer::timeout,
//                              [&future]() {
//                                  future.setFinished();
//                              });
//             QObject::connect(timer, &QTimer::timeout,
//                              timer, &QObject::deleteLater);
//             timer->setSingleShot(true);
//             timer->start(500);
//         });
//         future = job.exec();
//     }
//
//     QTRY_VERIFY(future.isFinished());
// }
//
// void AsyncTest::testErrorTask_data()
// {
//     QTest::addColumn<bool>("pass");
//     QTest::newRow("pass") << true;
//     QTest::newRow("fail") << false;
// }
//
// void AsyncTest::testErrorTask()
// {
//     QFETCH(bool, pass);
//
//     int errorCode = 0;
//
//     auto job = KAsync::start<int>(
//         []() -> int {
//             return 42;
//         })
//     .then<int, int>(
//         [pass](int in, KAsync::Future<int> &future) {
//             if (pass) {
//                 future.setValue(in);
//                 future.setFinished();
//             } else {
//                 future.setError(in);
//             }
//         })
//     .error(
//         [&errorCode](int error, const QString &) {
//             errorCode = error;
//         });
//
//     KAsync::Future<int> future = job.exec();
//     QTRY_VERIFY(future.isFinished());
//     if (pass) {
//         QCOMPARE(future.value(), 42);
//         QCOMPARE(errorCode, 0);
//         QCOMPARE(future.errorCode(), 0);
//     } else {
//         QCOMPARE(errorCode, 42);
//         QCOMPARE(future.errorCode(), errorCode);
//     }
// }
//
//
void AsyncTest::benchmarkSyncThenExecutor()
{
    auto job = KAsync::syncStart<int>(
        []() {
            return 1;
        });

    QBENCHMARK {
       job.exec();
    }
}

void AsyncTest::benchmarkFutureThenExecutor()
{
    auto job = KAsync::start<int>(
        [](KAsync::Future<int> &f) {
            f.setResult(1);
        });

    QBENCHMARK {
       job.exec();
    }
}

void AsyncTest::benchmarkThenExecutor()
{
    // auto job = KAsync::start<int>(
    //     []() {
    //         return KAsync::value(1);
    //     });
    
    //This is exactly the same as the future version (due to it's implementation).
    auto job = KAsync::value(1);

    QBENCHMARK {
       job.exec();
    }
}

QTEST_MAIN(AsyncTest)

#include "asynctest.moc"
