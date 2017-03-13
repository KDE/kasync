/*
 * Copyright 2014  Daniel Vrátil <dvratil@redhat.com>
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

#include "async.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>

using namespace KAsync;

Private::Execution::Execution(const Private::ExecutorBasePtr &executor)
    : executor(executor)
    , resultBase(nullptr)
    , tracer(nullptr)
{
}

Private::Execution::~Execution()
{
    if (resultBase) {
        resultBase->releaseExecution();
        delete resultBase;
    }
    prevExecution.reset();
}

void Private::Execution::setFinished()
{
    delete tracer;
}

void Private::Execution::releaseFuture()
{
    resultBase = 0;
}




Private::ExecutorBase::ExecutorBase(const ExecutorBasePtr &parent)
    : mPrev(parent)
{
}

Private::ExecutorBase::~ExecutorBase()
{
}




JobBase::JobBase(const Private::ExecutorBasePtr &executor)
    : mExecutor(executor)
{
}

JobBase::~JobBase()
{
}

Job<void> KAsync::doWhile(const Job<ControlFlowFlag> &body)
{
    return KAsync::start<void>([body] (KAsync::Future<void> &future) {
        auto job = body.then<void, ControlFlowFlag>([&future, body](const KAsync::Error &error, ControlFlowFlag flag) {
            if (error) {
                future.setError(error);
                future.setFinished();
            } else if (flag == ControlFlowFlag::Continue) {
                doWhile(body).then<void>([&future](const KAsync::Error &error) {
                    if (error) {
                        future.setError(error);
                    }
                    future.setFinished();
                }).exec();
            } else {
                future.setFinished();
            }
        }).exec();
    });
}

Job<void> KAsync::doWhile(const JobContinuation<ControlFlowFlag> &body)
{
    return doWhile(KAsync::start<ControlFlowFlag>([body] {
        return body();
    }));
}

Job<void> KAsync::wait(int delay)
{
    return KAsync::start<void>([delay](KAsync::Future<void> &future) {
        QTimer::singleShot(delay, [&future]() {
            future.setFinished();
        });
    });
}
