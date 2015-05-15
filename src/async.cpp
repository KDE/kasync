/*
 * Copyright 2014  Daniel Vrátil <dvratil@redhat.com>
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
    , isRunning(false)
    , isFinished(false)
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
    isFinished = true;
    //executor.clear();
#ifndef QT_NO_DEBUG
    if (tracer) {
        delete tracer;
    }
#endif
}

void Private::Execution::releaseFuture()
{
    resultBase = 0;
}

bool Private::Execution::errorWasHandled() const
{
    Execution *exec = const_cast<Execution*>(this);
    while (exec) {
        if (exec->executor->hasErrorFunc()) {
            return true;
        }
        exec = exec->prevExecution.data();
    }
    return false;
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

Job<void> KAsync::dowhile(Condition condition, ThenTask<void> body)
{
    return KAsync::start<void>([body, condition](KAsync::Future<void> &future) {
        asyncWhile([condition, body](std::function<void(bool)> whileCallback) {
            KAsync::start<void>(body).then<void>([whileCallback, condition]() {
                whileCallback(!condition());
            }).exec();
        },
        [&future]() { //while complete
            future.setFinished();
        });
    });
}

Job<void> KAsync::dowhile(ThenTask<bool> body)
{
    return KAsync::start<void>([body](KAsync::Future<void> &future) {
        asyncWhile([body](std::function<void(bool)> whileCallback) {
            KAsync::start<bool>(body).then<bool, bool>([whileCallback](bool result) {
                whileCallback(!result);
                //FIXME this return value is only required because .then<bool, void> doesn't work
                return true;
            }).exec();
        },
        [&future]() { //while complete
            future.setFinished();
        });
    });
}

Job<void> KAsync::wait(int delay)
{
    auto timer = QSharedPointer<QTimer>::create();
    return KAsync::start<void>([timer, delay](KAsync::Future<void> &future) {
        timer->setSingleShot(true);
        QObject::connect(timer.data(), &QTimer::timeout, [&future]() {
            future.setFinished();
        });
        timer->start(delay);
    });
}

