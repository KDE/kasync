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
#include <iterator>

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

class Executor
{

public:
    Executor(Executor *parent)
      : mPrev(parent)
      , mResult(0)
    {
    }

    virtual ~Executor()
    {
        delete mResult;
    }

    virtual void exec() = 0;

    FutureBase* result() const
    {
        return mResult;
    }

    Executor *mPrev;
    FutureBase *mResult;
};

template<typename Out, typename ... In>
class ThenExecutor:  public Executor
{

    typedef Out OutType;
    typedef typename std::tuple_element<0, std::tuple<In ..., void>>::type InType;


public:
    ThenExecutor(ThenTask<Out, In ...> then, Executor *parent = nullptr)
        : Executor(parent)
        , mFunc(then)
    {
    }

    void exec()
    {
        Async::Future<InType> *in = 0;
        if (mPrev) {
            mPrev->exec();
            in = static_cast<Async::Future<InType>*>(mPrev->result());
            assert(in->isFinished());
        }

        auto out = new Async::Future<Out>();
        mFunc(in ? in->value() : In() ..., *out);
        out->waitForFinished();
        mResult = out;
    }

private:
    std::function<void(const In& ..., Async::Future<Out>&)> mFunc;
};

template<typename PrevOut, typename Out, typename In>
class EachExecutor : public Executor
{
public:
    EachExecutor(EachTask<Out, In> each, Executor *parent = nullptr)
        : Executor(parent)
        , mFunc(each)
    {
    }

    void exec()
    {
        assert(mPrev);
        mPrev->exec();
        Async::Future<PrevOut> *in = static_cast<Async::Future<PrevOut>*>(mPrev->result());

        auto *out = new Async::Future<Out>();
        for (auto arg : in->value()) {
            Async::Future<Out> future;
            mFunc(arg, future);
            future.waitForFinished();
            out->setValue(out->value() + future.value());
        }

        mResult = out;
    }

private:
    std::function<void(const In&, Async::Future<Out>&)> mFunc;
};

class JobBase
{
    template<typename Out, typename ... In>
    friend class Job;

public:
    JobBase(Executor *executor);
    ~JobBase();

    void exec();

protected:
    Executor *mExecutor;
};

template<typename Out, typename ... In>
class Job : public JobBase
{
    template<typename OutOther, typename ... InOther>
    friend class Job;

    template<typename OutOther, typename ... InOther>
    friend Job<OutOther, InOther ...> start(Async::ThenTask<OutOther, InOther ...> func);

public:
    template<typename OutOther, typename ... InOther>
    Job<OutOther, InOther ...> then(ThenTask<OutOther, InOther ...> func)
    {
        Executor *exec = new ThenExecutor<OutOther, InOther ...>(func, mExecutor);
        return Job<OutOther, InOther ...>(exec);
    }

    template<typename OutOther, typename InOther>
    Job<OutOther, InOther> each(EachTask<OutOther, InOther> func)
    {
        static_assert(detail::isIterable<Out>::value,
                      "The 'Each' task can only be connected to a job that returns a list or array.");
        static_assert(detail::isIterable<OutOther>::value,
                      "The result type of 'Each' task must be a list or an array.");
        return Job<OutOther, InOther>(new EachExecutor<Out, OutOther, InOther>(func, mExecutor));
    }

    template<typename OutOther, typename InOther>
    Job<OutOther, InOther> reduce(ReduceTask<OutOther, InOther> func)
    {
        static_assert(Async::detail::isIterable<Out>::value,
                      "The result type of 'Reduce' task must be a list or an array.");
        //return Job<Out_, In_>::create(func, new ReduceEx, this);
    }

    Async::Future<Out> result() const
    {
        return *static_cast<Async::Future<Out>*>(mExecutor->result());
    }

private:
    Job(Executor *executor)
        : JobBase(executor)
    {
    }
};

} // namespace Async



// ********** Out of line definitions ****************

template<typename Out, typename ... In>
Async::Job<Out, In ...> Async::start(ThenTask<Out, In ...> func)
{
    Executor *exec = new ThenExecutor<Out, In ...>(func);
    return Job<Out, In ...>(exec);
}

#endif // ASYNC_H


