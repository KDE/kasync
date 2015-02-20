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

    void testAsyncEach();
    void testSyncEach();
    void testJoinedEach();

    void testAsyncReduce();
    void testSyncReduce();
    void testJoinedReduce();

    void testErrorHandler();

    void benchmarkSyncThenExecutor();

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
        }).then<int, int>(
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



QTEST_MAIN(AsyncTest);

#include "asynctest.moc"
