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

private Q_SLOTS:
    void testSyncPromises();
    void testAsyncPromises();
    void testSyncEach();
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

    job.exec();
    Async::Future<QString> future = job.result();

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

    job.exec();
    Async::Future<int> future = job.result();
    QCOMPARE(future.value(), 42);
}

void AsyncTest::testSyncEach()
{
  /*
    auto job = Async::start<QList<int>>(
        []() -> Async::Future<QList<int>> {
            Async::Future<QList<int>> future(QList<int>{ 1, 2, 3, 4 });
            future.setFinished();
            return future;
        })
    .each<QList<int>, int>(
        [](const int &v, Async::Future<QList<int>> &future) {
            setFinished();
        });
        */
}


QTEST_MAIN(AsyncTest);

#include "asynctest.moc"
