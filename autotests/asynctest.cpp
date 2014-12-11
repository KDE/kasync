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
};

void AsyncTest::testSyncPromises()
{
    auto baseJob = Async::start<int>(
        []() -> Async::Future<int> {
            auto f = Async::Future<int>(42);
            f.setFinished();
            return f;
        })
    .then<QString, int>(
        [](int v) -> Async::Future<QString> {
            auto f = Async::Future<QString>("Result is " + QString::number(v));
            f.setFinished();
            return f;
        });

    auto job = baseJob.then<QString, QString>(
        [](const QString &v) -> Async::Future<QString> {
            auto f = Async::Future<QString>(v.toUpper());
            f.setFinished();
            return f;
        });

    job.exec();
    Async::Future<QString> future = job.result();

    QCOMPARE(future.value(), QString::fromLatin1("RESULT IS 42"));
}

void AsyncTest::testAsyncPromises()
{
    auto job = Async::start<int>(
      []() -> Async::Future<int> {
          Async::Future<int> future(-1);
          QTimer *timer = new QTimer();
          QObject::connect(timer, &QTimer::timeout,
                           [&]() {
                              future.setValue(42);
                              future.setFinished();
                              timer->deleteLater();
                           });
          timer->setSingleShot(true);
          timer->start(200);
          return future;
      });

    job.exec();
    Async::Future<int> future = job.result();
    QCOMPARE(future.value(), 42);
}


QTEST_MAIN(AsyncTest);

#include "asynctest.moc"
