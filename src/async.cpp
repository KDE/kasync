/*
 * Copyright 2014  Daniel Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "async.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>


using namespace Async;

JobBase::JobBase(Executor *executor)
    : mExecutor(executor)
{
}

JobBase::~JobBase()
{
}

void JobBase::exec()
{
    mExecutor->exec();
}


FutureBase::FutureBase()
    : mFinished(false)
    , mWaitLoop(nullptr)
{
}

FutureBase::FutureBase(const Async::FutureBase &other)
    : mFinished(other.mFinished)
    , mWaitLoop(other.mWaitLoop)
{
}

FutureBase::~FutureBase()
{
}

void FutureBase::setFinished()
{
    mFinished = true;
    if (mWaitLoop && mWaitLoop->isRunning()) {
        mWaitLoop->quit();
    }
}

bool FutureBase::isFinished() const
{
    return mFinished;
}

void FutureBase::waitForFinished()
{
    if (mFinished) {
        return;
    }

    mWaitLoop = new QEventLoop;
    mWaitLoop->exec(QEventLoop::ExcludeUserInputEvents);
    delete mWaitLoop;
    mWaitLoop = 0;
}

