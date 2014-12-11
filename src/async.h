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

#ifndef ASYNC_H
#define ASYNC_H

#include <functional>
#include <list>
#include <type_traits>
#include <iostream>
#include <cassert>

#include "future.h"
#include "async_impl.h"


namespace Async {

template<typename Out, typename ... In>
class Job;

template<typename Out, typename ... In>
Job<Out, In ...> start(const std::function<Async::Future<Out>(In ...)> &func);


class JobBase
{
public:
    JobBase(JobBase *prev = nullptr)
    : mPrev(prev)
    , mResult(0)
    {}

    virtual void exec() = 0;

public:
    JobBase *mPrev;
    void *mResult;
};

namespace Private {

    template<typename Out, typename In>
    typename std::enable_if<!std::is_void<In>::value, void>::type
    doExec(JobBase *prev, JobBase *jobBase);

    template<typename Out, typename ... In>
    typename std::enable_if<sizeof...(In) == 0, void>::type
    doExec(JobBase *prev, JobBase *jobBase, int * /* disambiguate */ = 0);
}

template<typename Out, typename ... In>
class Job : public JobBase
{
    template<typename Out_, typename ... In_>
    friend class Job;

    template<typename Out_, typename ... In_, typename F_>
    friend Job<Out_, In_ ...> start(F_ func);

public:
    ~Job()
    {
        // Can't delete in JobBase, since we don't know the type
        // and deleting void* is undefined
        delete reinterpret_cast<Async::Future<Out>*>(mResult);
    }

    template<typename Out_, typename ... In_, typename F>
    Job<Out_, In_ ...> then(F func)
    {
        Job<Out_, In_ ...> job(this);
        job.mFunc = func;
        return job;
    }

    Async::Future<Out> result() const
    {
        return *reinterpret_cast<Async::Future<Out>*>(mResult);
    }


    void exec()
    {
        Async::Private::doExec<Out, In ...>(mPrev, this);
    }

private:
    Job(JobBase *parent = nullptr)
        : JobBase(parent)
    {}

public:
    std::function<Async::Future<Out>(In ...)> mFunc;
};

template<typename Out, typename ... In, typename F>
Job<Out, In ...> start(F func)
{
    Job<Out, In ...> job;
    job.mFunc = std::function<Async::Future<Out>(In ...)>(func);
    return job;
}

} // namespace Async

template<typename Out, typename In>
typename std::enable_if<!std::is_void<In>::value, void>::type
Async::Private::doExec(JobBase *prev, JobBase *jobBase)
{
    prev->exec();
    Async::Future<In> *in = reinterpret_cast<Async::Future<In>*>(prev->mResult);
    assert(in->isFinished());

    Job<Out, In> *job = dynamic_cast<Job<Out, In>*>(jobBase);
    Async::Future<Out> *out = new Async::Future<Out>(job->mFunc(in->value()));
    out->waitForFinished();
    job->mResult = reinterpret_cast<void*>(out);
};

template<typename Out, typename ... In>
typename std::enable_if<sizeof...(In) == 0, void>::type
Async::Private::doExec(JobBase *prev, JobBase *jobBase, int * /* disambiguation */ = 0)
{
    if (prev) {
        prev->exec();
        Async::Future<void> *in = reinterpret_cast<Async::Future<void>*>(prev->mResult);
        assert(in->isFinished());
    }

    Job<Out> *job = dynamic_cast<Job<Out>*>(jobBase);
    Async::Future<Out> *out = new Async::Future<Out>(job->mFunc());
    out->waitForFinished();
    job->mResult = reinterpret_cast<void*>(out);
};


#endif // ASYNC_H


