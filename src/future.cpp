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

#include "future.h"
#include "async.h"

using namespace Async;

FutureBase::PrivateBase::PrivateBase(const Private::ExecutionPtr &execution)
    : finished(false)
    , errorCode(0)
    , mExecution(execution)
{
}

FutureBase::PrivateBase::~PrivateBase()
{
    Private::ExecutionPtr executionPtr = mExecution.toStrongRef();
    if (executionPtr) {
        executionPtr->releaseFuture();
        releaseExecution();
    }
}

void FutureBase::PrivateBase::releaseExecution()
{
    mExecution.clear();
}



FutureBase::FutureBase()
    : d(nullptr)
{
}

FutureBase::FutureBase(FutureBase::PrivateBase *dd)
    : d(dd)
{
}

FutureBase::FutureBase(const Async::FutureBase &other)
    : d(other.d)
{
}

FutureBase::~FutureBase()
{
}

void FutureBase::releaseExecution()
{
    d->releaseExecution();
}

void FutureBase::setFinished()
{
    if (isFinished()) {
        return;
    }
    d->finished = true;
    for (auto watcher : d->watchers) {
        if (watcher) {
            watcher->futureReadyCallback();
        }
    }
}

bool FutureBase::isFinished() const
{
    return d->finished;
}

void FutureBase::setError(int code, const QString &message)
{
    d->errorCode = code;
    d->errorMessage = message;
    setFinished();
}

int FutureBase::errorCode() const
{
    return d->errorCode;
}

QString FutureBase::errorMessage() const
{
    return d->errorMessage;
}


void FutureBase::addWatcher(FutureWatcherBase* watcher)
{
    d->watchers.append(QPointer<FutureWatcherBase>(watcher));
}





FutureWatcherBase::FutureWatcherBase(QObject *parent)
    : QObject(parent)
    , d(new FutureWatcherBase::Private)
{
}

FutureWatcherBase::~FutureWatcherBase()
{
    delete d;
}

void FutureWatcherBase::futureReadyCallback()
{
    Q_EMIT futureReady();
}

void FutureWatcherBase::setFutureImpl(const FutureBase &future)
{
    d->future = future;
    d->future.addWatcher(this);
    if (future.isFinished()) {
        futureReadyCallback();
    }
}
