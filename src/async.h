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
using ThenTask = typename detail::identity<std::function<void(In ..., Async::Future<Out>&)>>::type;
template<typename Out, typename In>
using EachTask = typename detail::identity<std::function<void(In, Async::Future<Out>&)>>::type;
template<typename Out, typename In>
using ReduceTask = typename detail::identity<std::function<void(In, Async::Future<Out>&)>>::type;

template<typename Out, typename ... In>
Job<Out, In ...> start(ThenTask<Out, In ...> func);

namespace Private
{
    template<typename Out, typename ... In>
    void doExec(Job<Out, In ...> *job, Async::Future<Out> &out, const In & ... args);
}

class JobBase
{
    template<typename Out, typename ... In>
    friend class Job;

protected:
    enum JobType {
        Then,
        Each,
        Reduce
    };

public:
    JobBase(JobType jobType, JobBase *prev = nullptr);
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

    template<typename Out_, typename ... In_>
    friend Job<Out_, In_ ...> start(Async::ThenTask<Out_, In_ ...> func);

    typedef Out OutType;
    typedef typename std::tuple_element<0, std::tuple<In ..., void>>::type InType;

public:
    ~Job()
    {
        delete reinterpret_cast<Async::Future<Out>*>(mResult);
    }

    template<typename Out_, typename ... In_>
    Job<Out_, In_ ...> then(ThenTask<Out_, In_ ...> func)
    {
        return Job<Out_, In_ ...>::create(func, JobBase::Then, this);
    }

    template<typename Out_, typename In_>
    Job<Out_, In_> each(EachTask<Out_, In_> func)
    {
        static_assert(detail::isIterable<OutType>::value,
                      "The 'Each' task can only be connected to a job that returns a list or array.");
        static_assert(detail::isIterable<Out_>::value,
                      "The result type of 'Each' task must be a list or an array.");
        return Job<Out_, In_>::create(func, JobBase::Each, this);
    }

    template<typename Out_, typename In_>
    Job<Out_, In_> reduce(ReduceTask<Out_, In_> func)
    {
        static_assert(Async::detail::isIterable<OutType>::value,
                      "The result type of 'Reduce' task must be a list or an array.");
        return Job<Out_, In_>::create(func, JobBase::Reduce, this);
    }

    Async::Future<Out> result() const
    {
        return *reinterpret_cast<Async::Future<Out>*>(mResult);
    }

    void exec();

private:
    Job(JobBase::JobType jobType, JobBase *parent = nullptr)
        : JobBase(jobType, parent)
    {
    }

    template<typename F>
    static Job<Out, In ... > create(F func, JobBase::JobType jobType, JobBase *parent = nullptr);

public:
    std::function<void(In ..., Async::Future<Out>&)> mFunc;
};


} // namespace Async



// ********** Out of line definitions ****************

template<typename Out, typename ... In>
Async::Job<Out, In ...> Async::start(ThenTask<Out, In ...> func)
{
    return Job<Out, In ...>::create(func, JobBase::Then);
}

template<typename Out, typename ... In>
void Async::Private::doExec(Job<Out, In ...> *job, Async::Future<Out> &out, const In & ... args)
{
    job->mFunc(args ..., out);
};

template<typename Out, typename ... In>
void Async::Job<Out, In ...>::exec()
{
    Async::Future<InType> *in = nullptr;
    if (mPrev) {
        mPrev->exec();
        in = reinterpret_cast<Async::Future<InType>*>(mPrev->mResult);
        assert(in->isFinished());
    }

    auto out = new Async::Future<Out>();
    Private::doExec<Out, In ...>(this, *out, in ? in->value() : In() ...);
    out->waitForFinished();
    mResult = reinterpret_cast<void*>(out);
}

template<typename Out, typename ... In>
template<typename F>
Async::Job<Out, In ...> Async::Job<Out, In ...>::create(F func, Async::JobBase::JobType jobType, Async::JobBase* parent)
{
    Job<Out, In ...> job(jobType, parent);
    job.mFunc = func;
    return job;
}


#endif // ASYNC_H


