/*
 * Copyright 2014 - 2015 Daniel Vrátil <dvratil@redhat.com>
 * Copyright 2016  Daniel Vrátil <dvratil@kde.org>
 * Copyright 2016  Christian Mollekopf <mollekopf@kolabsystems.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
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
