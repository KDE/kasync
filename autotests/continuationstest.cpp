/*
    SPDX-FileCopyrightText: 2019 Daniel Vrátil <dvratil@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QObject>
#include <QTest>

#define KASYNC_TEST

#include "../src/continuations_p.h"

namespace KAsync
{
// Simplified definition of KAsync::Job so we can return it from
// the test continuations
template<typename T, typename ... U>
class Job {};
}

Q_DECLARE_METATYPE(std::function<bool()>)

#define STATIC_COMPARE(actual, expected) \
    static_assert((actual) == (expected), "Check failed: " #actual " == " #expected)

class ContinuationHolderTest : public QObject
{
    Q_OBJECT

    using TestedHolder = KAsync::Private::ContinuationHolder<void>;
    template<typename Continuation, typename T>
    void testContinuation(T &&func, int index)
    {

        #define CHECK(Cont) \
            QCOMPARE(KAsync::Private::continuationIs<KAsync::Cont<void>>(holder), (std::is_same<Continuation, KAsync::Cont<void>>::value))

        TestedHolder holder(Continuation(std::move(func)));
        QCOMPARE(holder.mIndex, index);
        CHECK(SyncContinuation);
        CHECK(SyncErrorContinuation);
        CHECK(AsyncContinuation);
        CHECK(AsyncErrorContinuation);
        CHECK(JobContinuation);
        CHECK(JobErrorContinuation);

        //KAsync::Private::continuation_get<Continuation>(holder)();
    }

private Q_SLOTS:
    void testTupleMax()
    {
        using TestHolder = KAsync::Private::ContinuationHolder<int>;
        using Tuple = std::tuple<uint8_t, uint32_t, uint16_t>;

        STATIC_COMPARE(TestHolder::tuple_max<Tuple>::size, sizeof(uint32_t));
        STATIC_COMPARE(TestHolder::tuple_max<Tuple>::alignment, alignof(uint32_t));
    }

    void testTupleIndex()
    {
        using TestHolder = KAsync::Private::ContinuationHolder<int>;
        using Tuple = std::tuple<uint8_t, uint32_t, uint16_t>;

        STATIC_COMPARE((TestHolder::tuple_index<uint8_t, Tuple>::value), 0);
        STATIC_COMPARE((TestHolder::tuple_index<uint32_t, Tuple>::value), 1);
        STATIC_COMPARE((TestHolder::tuple_index<uint16_t, Tuple>::value), 2);
    }

    void testContinuationHolder_data()
    {
        QTest::addColumn<std::function<bool()>>("func");

#define ADD_ROW(name, lambda, index) \
        QTest::newRow(#name) << std::function<bool()>([this]() { \
            bool called = true; \
            testContinuation<KAsync::name<void>>(lambda, index); \
            return called; \
        });

        ADD_ROW(AsyncContinuation, [&called](KAsync::Future<void> &) mutable { called = true; }, 0);
        ADD_ROW(AsyncErrorContinuation, [&called](const KAsync::Error &, KAsync::Future<void> &) mutable { called = true; }, 1);
ADD_ROW(SyncContinuation, [&called]() mutable { called = true; }, 2)
        ADD_ROW(SyncErrorContinuation, [&called](const KAsync::Error &) mutable { called = true; }, 3);
        ADD_ROW(JobContinuation, [&called]() mutable { called = true; return KAsync::Job<void>(); }, 4);
        ADD_ROW(JobErrorContinuation, [&called](const KAsync::Error &) mutable { called = true; return KAsync::Job<void>(); }, 5);
    }

    void testContinuationHolder()
    {
        QFETCH(std::function<bool()>, func);

        QVERIFY(func());
    }
};

QTEST_GUILESS_MAIN(ContinuationHolderTest)

#include "continuationstest.moc"

