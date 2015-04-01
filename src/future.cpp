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

FutureBase::FutureBase()
{
}

FutureBase::FutureBase(const Async::FutureBase &other)
{
}

FutureBase::~FutureBase()
{
}

FutureBase::PrivateBase::PrivateBase(const Private::ExecutionPtr &execution)
    : mExecution(execution)
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



FutureWatcherBase::FutureWatcherBase(QObject *parent)
    : QObject(parent)
{
}

FutureWatcherBase::~FutureWatcherBase()
{
}


#include "future.moc"
