/*
    SPDX-FileCopyrightText: 2014-2015 Daniel Vrátil <dvratil@redhat.com>
    SPDX-FileCopyrightText: 2016 Daniel Vrátil <dvratil@kde.org>
    SPDX-FileCopyrightText: 2016 Christian Mollekopf <mollekopf@kolabsystems.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KASYNC_EXECUTION_P_H_
#define KASYNC_EXECUTION_P_H_

#include "kasync_export.h"

#include "debug.h"

#include <QSharedPointer>
#include <QPointer>
#include <QVector>
#include <QObject>

#include <memory>

namespace KAsync {

class FutureBase;

template<typename T>
class Future;

class Tracer;

//@cond PRIVATE
namespace Private
{

class ExecutorBase;
using ExecutorBasePtr = QSharedPointer<ExecutorBase>;

struct Execution;
using ExecutionPtr = QSharedPointer<Execution>;

class ExecutionContext;

enum ExecutionFlag {
    Always,
    ErrorCase,
    GoodCase
};

struct KASYNC_EXPORT Execution {
    explicit Execution(const ExecutorBasePtr &executor)
        : executor(executor)
    {}

    virtual ~Execution()
    {
        if (resultBase) {
            resultBase->releaseExecution();
            delete resultBase;
        }
        prevExecution.reset();
    }

    void setFinished()
    {
        tracer.reset();
    }

    template<typename T>
    KAsync::Future<T>* result() const
    {
        return static_cast<KAsync::Future<T>*>(resultBase);
    }

    void releaseFuture()
    {
        resultBase = nullptr;
    }

    ExecutorBasePtr executor;
    ExecutionPtr prevExecution;
    std::unique_ptr<Tracer> tracer;
    FutureBase *resultBase = nullptr;
};

class ExecutionContext {
public:
    using Ptr = QSharedPointer<ExecutionContext>;

    QVector<QPointer<const QObject>> guards;
    bool guardIsBroken() const
    {
        for (const auto &g : guards) {
            if (!g) {
                return true;
            }
        }
        return false;
    }
};

} // namespace Private
//@endcond

} // namespace KAsync

#endif
