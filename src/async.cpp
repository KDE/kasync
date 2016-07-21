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

static void asyncWhile(const std::function<void(std::function<void(bool)>)> &body, const std::function<void()> &completionHandler) {
    body([body, completionHandler](bool complete) {
        if (complete) {
            completionHandler();
        } else {
            asyncWhile(body, completionHandler);
        }
    });
}

Job<void> KAsync::dowhile(Job<ControlFlowFlag> body)
{
    return body.then<void, ControlFlowFlag>([body](const KAsync::Error &error, ControlFlowFlag flag) {
        if (error) {
            return KAsync::error(error);
        } else if (flag == ControlFlowFlag::Continue) {
            return dowhile(body);
        }
        return KAsync::null();
    });
}

Job<void> KAsync::dowhile(JobContinuation<ControlFlowFlag> body)
{
    return dowhile(KAsync::start<ControlFlowFlag>([body] {
        qDebug() << "Calling wrapper";
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
