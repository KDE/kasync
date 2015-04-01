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


using namespace Async;

Private::ExecutorBase::ExecutorBase(const ExecutorBasePtr &parent)
    : mPrev(parent)
    , mResult(0)
    , mIsRunning(false)
    , mIsFinished(false)
{
}

Private::ExecutorBase::~ExecutorBase()
{
    delete mResult;
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

Job<void> Async::dowhile(Condition condition, ThenTask<void> body)
{
    return Async::start<void>([body, condition](Async::Future<void> &future) {
        asyncWhile([condition, body](std::function<void(bool)> whileCallback) {
            Async::start<void>(body).then<void>([whileCallback, condition]() {
                whileCallback(!condition());
            }).exec();
        },
        [&future]() { //while complete
            future.setFinished();
        });
    });
}

Job<void> Async::dowhile(ThenTask<bool> body)
{
    return Async::start<void>([body](Async::Future<void> &future) {
        asyncWhile([body](std::function<void(bool)> whileCallback) {
            Async::start<bool>(body).then<bool, bool>([whileCallback](bool result) {
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

