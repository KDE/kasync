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
#include <boost/graph/graph_concepts.hpp>

#include "future.h"
#include "async_impl.h"


namespace Async {

template<typename PrevOut, typename Out, typename ... In>
class Executor;

class JobBase;

template<typename Out, typename ... In>
class Job;

template<typename Out, typename ... In>
using ThenTask = typename detail::identity<std::function<void(In ..., Async::Future<Out>&)>>::type;
template<typename Out, typename In>
using EachTask = typename detail::identity<std::function<void(In, Async::Future<Out>&)>>::type;
template<typename Out, typename In>
using ReduceTask = typename detail::identity<std::function<void(In, Async::Future<Out>&)>>::type;

namespace Private
{

template<typename ... T>
struct PreviousOut {
    using type = typename std::tuple_element<0, std::tuple<T ..., void>>::type;
};

class ExecutorBase
{
    template<typename PrevOut, typename Out, typename ... In>
    friend class Executor;

public:
    virtual ~ExecutorBase();
    virtual void exec() = 0;

    inline FutureBase* result() const
    {
        return mResult;
    }

protected:
    ExecutorBase(ExecutorBase *parent);

    ExecutorBase *mPrev;
    FutureBase *mResult;
};

template<typename PrevOut, typename Out, typename ... In>
class Executor : public ExecutorBase
{
protected:
    Executor(ExecutorBase *parent)
        : ExecutorBase(parent)
    {}
    virtual ~Executor() {}
    inline Async::Future<PrevOut>* chainup();

    std::function<void(const In& ..., Async::Future<Out> &)> mFunc;
};

template<typename Out, typename ... In>
class ThenExecutor: public Executor<typename PreviousOut<In ...>::type, Out, In ...>
{
public:
    ThenExecutor(ThenTask<Out, In ...> then, ExecutorBase *parent = nullptr);
    void exec();
};

template<typename PrevOut, typename Out, typename In>
class EachExecutor : public Executor<PrevOut, Out, In>
{
public:
    EachExecutor(EachTask<Out, In> each, ExecutorBase *parent);
    void exec();
};

template<typename Out, typename In>
class ReduceExecutor : public Executor<In, Out, In>
{
public:
    ReduceExecutor(ReduceTask<Out, In> reduce, ExecutorBase *parent);
    void exec();
};

} // namespace Private


class JobBase
{
    template<typename Out, typename ... In>
    friend class Job;

public:
    JobBase(Private::ExecutorBase *executor);
    ~JobBase();

    void exec();

protected:
    Private::ExecutorBase *mExecutor;
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
        return Job<OutOther, InOther ...>(new Private::ThenExecutor<OutOther, InOther ...>(func, mExecutor));
    }

    template<typename OutOther, typename InOther>
    Job<OutOther, InOther> each(EachTask<OutOther, InOther> func)
    {
        static_assert(detail::isIterable<Out>::value,
                      "The 'Each' task can only be connected to a job that returns a list or an array.");
        static_assert(detail::isIterable<OutOther>::value,
                      "The result type of 'Each' task must be a list or an array.");
        return Job<OutOther, InOther>(new Private::EachExecutor<Out, OutOther, InOther>(func, mExecutor));
    }

    template<typename OutOther, typename InOther>
    Job<OutOther, InOther> reduce(ReduceTask<OutOther, InOther> func)
    {
        static_assert(Async::detail::isIterable<Out>::value,
                      "The 'Result' task can only be connected to a job that returns a list or an array");
        static_assert(std::is_same<typename Out::value_type, typename InOther::value_type>::value,
                      "The return type of previous task must be compatible with input type of this task");
        return Job<OutOther, InOther>(new Private::ReduceExecutor<OutOther, InOther>(func, mExecutor));
    }

    Async::Future<Out> result() const
    {
        return *static_cast<Async::Future<Out>*>(mExecutor->result());
    }

private:
    Job(Private::ExecutorBase *executor)
        : JobBase(executor)
    {
    }
};

} // namespace Async


// ********** Out of line definitions ****************

namespace Async {

template<typename Out, typename ... In>
Job<Out, In ...> start(ThenTask<Out, In ...> func)
{
    return Job<Out, In ...>(new Private::ThenExecutor<Out, In ...>(func));
}

namespace Private {

template<typename PrevOut, typename Out, typename ... In>
Future<PrevOut>* Executor<PrevOut, Out, In ...>::chainup()
{
    if (mPrev) {
        mPrev->exec();
        auto future = static_cast<Async::Future<PrevOut>*>(mPrev->result());
        assert(future->isFinished());
        return future;
    } else {
        return 0;
    }
}

template<typename Out, typename ... In>
ThenExecutor<Out, In ...>::ThenExecutor(ThenTask<Out, In ...> then, ExecutorBase* parent)
    : Executor<typename PreviousOut<In ...>::type, Out, In ...>(parent)
{
    this->mFunc = then;
}

template<typename Out, typename ... In>
void ThenExecutor<Out, In ...>::exec()
{
    auto in = this->chainup();
    (void)in; // supress 'unused variable' warning when In is void

    auto out = new Async::Future<Out>();
    this->mFunc(in ? in->value() : In() ..., *out);
    out->waitForFinished();
    this->mResult = out;
}

template<typename PrevOut, typename Out, typename In>
EachExecutor<PrevOut, Out, In>::EachExecutor(EachTask<Out, In> each, ExecutorBase* parent)
    : Executor<PrevOut, Out, In>(parent)
{
    this->mFunc = each;
}

template<typename PrevOut, typename Out, typename In>
void EachExecutor<PrevOut, Out, In>::exec()
{
    auto in = this->chainup();

    auto *out = new Async::Future<Out>();
    for (auto arg : in->value()) {
        Async::Future<Out> future;
        this->mFunc(arg, future);
        future.waitForFinished();
        out->setValue(out->value() + future.value());
    }
    out->setFinished();

    this->mResult = out;
}

template<typename Out, typename In>
ReduceExecutor<Out, In>::ReduceExecutor(ReduceTask<Out, In> reduce, ExecutorBase* parent)
    : Executor<In, Out, In>(parent)
{
    this->mFunc = reduce;
}

template<typename Out, typename In>
void ReduceExecutor<Out, In>::exec()
{
    auto in = this->chainup();

    auto out = new Async::Future<Out>();
    this->mFunc(in->value(), *out);
    out->waitForFinished();
    this->mResult = out;
}

} // namespace Private

} // namespace Async



#endif // ASYNC_H


