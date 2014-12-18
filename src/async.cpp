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

Private::ExecutorBase::ExecutorBase(ExecutorBase* parent)
    : mPrev(parent)
    , mResult(0)
{
}

Private::ExecutorBase::~ExecutorBase()
{
    delete mResult;
}


JobBase::JobBase(Private::ExecutorBase *executor)
    : mExecutor(executor)
{
}

JobBase::~JobBase()
{
}

