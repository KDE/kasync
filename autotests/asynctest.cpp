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

class AsyncTest : public QObject
{
    Q_OBJECT

public:
    AsyncTest()
    {}

    ~AsyncTest()
    {}

private:
    template<typename T>
    bool waitForFuture(const Async::Future<T> &f)
    {
        QEventLoop eventLoop;
        Async::FutureWatcher<T> watcher;
        connect(&watcher, &Async::FutureWatcher<T>::futureReady,
                &eventLoop, &QEventLoop::quit);
        watcher.setFuture(f);
        eventLoop.exec();
        return true;
    }

private Q_SLOTS:
    void testSyncPromises();
    void testAsyncPromises();
    void testSyncEach();
    void testSyncReduce();
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
            QTimer *timer = new QTimer();
            QObject::connect(timer, &QTimer::timeout,
                             [&]() {
                                future.setValue(42);
                                future.setFinished();
                             });
            QObject::connect(timer, &QTimer::timeout,
                             timer, &QObject::deleteLater);
            timer->setSingleShot(true);
            timer->start(200);
        });

    Async::Future<int> future = job.exec();

    QVERIFY(waitForFuture<int>(future));
    QCOMPARE(future.value(), 42);
}

void AsyncTest::testSyncEach()
{
    auto job = Async::start<QList<int>>(
        [](Async::Future<QList<int>> &future) {
            future.setValue(QList<int>{ 1, 2, 3, 4 });
            future.setFinished();
        })
    .each<QList<int>, int>(
        [](const int &v, Async::Future<QList<int>> &future) {
            future.setValue(QList<int>{ v + 1 });
            future.setFinished();
        });

    Async::Future<QList<int>> future = job.exec();

    const QList<int> expected({ 2, 3, 4, 5 });
    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), expected);
}

void AsyncTest::testSyncReduce()
{
    auto job = Async::start<QList<int>>(
        [](Async::Future<QList<int>> &future) {
            future.setValue(QList<int>{ 1, 2, 3, 4 });
            future.setFinished();
        })
    .reduce<int, QList<int>>(
        [](const QList<int> &list, Async::Future<int> &future) {
            int sum = 0;
            for (int i : list) sum += i;
            future.setValue(sum);
            future.setFinished();
        });

    Async::Future<int> future = job.exec();

    QVERIFY(future.isFinished());
    QCOMPARE(future.value(), 10);
}



QTEST_MAIN(AsyncTest);

#include "asynctest.moc"
