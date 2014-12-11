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

class JobBase;

template<typename Out, typename ... In>
class Job;

template<typename Out, typename ... In>
Job<Out, In ...> start(const std::function<Async::Future<Out>(In ...)> &func);

namespace Private
{
    template<typename Out, typename ... In>
    Async::Future<Out>* doExec(Job<Out, In ...> *job, const In & ... args);
}

class JobBase
{
    template<typename Out, typename ... In>
    friend Async::Future<Out>* Private::doExec(Job<Out, In ...> *job, const In & ... args);

    template<typename Out, typename ... In>
    friend class Job;

protected:
    enum JobType {
        Then,
        Each
    };

public:
    JobBase(JobType jobType, JobBase *prev = nullptr)
    : mPrev(prev)
    , mResult(0)
    , mJobType(jobType)
    {}

    virtual void exec() = 0;

protected:
    JobBase *mPrev;
    void *mResult;

    JobType mJobType;
};



template<typename Out, typename ... In>
class Job : public JobBase
{
    template<typename Out_, typename ... In_>
    friend class Job;

    template<typename Out_, typename ... In_, typename F_>
    friend Job<Out_, In_ ...> start(F_ func);

    typedef Out OutType;
    typedef typename std::tuple_element<0, std::tuple<In ..., void>>::type InType;

public:
    ~Job()
    {
        delete reinterpret_cast<Async::Future<Out>*>(mResult);
    }

    template<typename Out_, typename ... In_, typename F>
    Job<Out_, In_ ...> then(F func)
    {
        return Job<Out_, In_ ...>::create(func, JobBase::Then, this);
    }

    template<typename Out_, typename ... In_, typename F>
    Job<Out_, In_ ...> each(F func)
    {
        return Job<Out_, In_ ...>::create(func, JobBase::Each, this);
    }

    Async::Future<Out> result() const
    {
        return *reinterpret_cast<Async::Future<Out>*>(mResult);
    }

    void exec()
    {
        Async::Future<InType> *in = nullptr;
        if (mPrev) {
            mPrev->exec();
            in = reinterpret_cast<Async::Future<InType>*>(mPrev->mResult);
            assert(in->isFinished());
        }

        Job<Out, In ...> *job = dynamic_cast<Job<Out, In ...>*>(this);
        Async::Future<Out> *out = Private::doExec<Out, In ...>(this, in ? in->value() : In() ...);
        out->waitForFinished();
        job->mResult = reinterpret_cast<void*>(out);
    }

private:
    Job(JobBase::JobType jobType, JobBase *parent = nullptr)
        : JobBase(jobType, parent)
    {
    }

    template<typename F>
    static Job<Out, In ... > create(F func, JobBase::JobType jobType, JobBase *parent = nullptr)
    {
        Job<Out, In ...> job(jobType, parent);
        job.mFunc = func;
        return job;
    }

public:
    std::function<Async::Future<Out>(In ...)> mFunc;
};

template<typename Out, typename ... In, typename F>
Job<Out, In ...> start(F func)
{
    return Job<Out, In ...>::create(func, JobBase::Then);
}

} // namespace Async

template<typename Out, typename ... In>
Async::Future<Out>* Async::Private::doExec(Job<Out, In ...> *job, const In & ... args)
{
    return new Async::Future<Out>(job->mFunc(args ...));
};

#endif // ASYNC_H


