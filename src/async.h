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

#ifndef ASYNC_H
#define ASYNC_H

#include <functional>
#include <list>
#include <type_traits>
#include <cassert>
#include <iterator>

#include "future.h"
#include "async_impl.h"

#include <QVector>
#include <QObject>


/*
 * TODO: on .then and potentially others: support for ThenTask without future argument and return value which makes it implicitly a sync continuation. Useful for typical value consumer continuations.
 * TODO: error continuation on .then and others.
 * TODO: instead of passing the future objects callbacks could be provided for result reporting (we can still use the future object internally
 */
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
using ErrorHandler = std::function<void(int, const QString &)>;

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
        , mPrevFuture(0)
    {}
    virtual ~Executor() {}
    inline Async::Future<PrevOut>* chainup();
    virtual void previousFutureReady() = 0;

    void exec();

    std::function<void(const In& ..., Async::Future<Out> &)> mFunc;
    std::function<void(int, const QString &)> mErrorFunc;
    Async::Future<PrevOut> *mPrevFuture;
};

template<typename Out, typename ... In>
class ThenExecutor: public Executor<typename PreviousOut<In ...>::type, Out, In ...>
{
public:
    ThenExecutor(ThenTask<Out, In ...> then, ErrorHandler errorHandler = ErrorHandler(), ExecutorBase *parent = nullptr);
    void previousFutureReady();
};

template<typename PrevOut, typename Out, typename In>
class EachExecutor : public Executor<PrevOut, Out, In>
{
public:
    EachExecutor(EachTask<Out, In> each, ExecutorBase *parent);
    void previousFutureReady();

private:
    QVector<Async::FutureWatcher<PrevOut>*> mFutureWatchers;
};

template<typename Out, typename In>
class ReduceExecutor : public Executor<In, Out, In>
{
public:
    ReduceExecutor(ReduceTask<Out, In> reduce, ExecutorBase *parent);
    void previousFutureReady();
};

} // namespace Private

/**
 * Start an asynchronous job sequence.
 *
 * Async::start() is your starting point to build a chain of jobs to be executed
 * asynchronously.
 *
 * @param func An asynchronous function to be executed. The function must have
 *             void return type, and accept exactly one argument of type @p Async::Future<In>,
 *             where @p In is type of the result.
 */
template<typename Out>
Job<Out> start(ThenTask<Out> func);


/**
 * A null job.
 * 
 * An async noop.
 *
 */
template<typename Out>
Job<Out> null()
{
    return Async::start<Out>([](Async::Future<Out> &future) {future.setFinished();});
}

/**
 * An error job.
 * 
 * An async error.
 *
 */
template<typename Out>
Job<Out> error(int errorCode = 1, const QString &errorMessage = QString())
{
    return Async::start<Out>([errorCode, errorMessage](Async::Future<Out> &future) {future.setError(errorCode, errorMessage);});
}

class JobBase
{
    template<typename Out, typename ... In>
    friend class Job;

public:
    JobBase(Private::ExecutorBase *executor);
    ~JobBase();

protected:
    Private::ExecutorBase *mExecutor;
};

/**
 * An Asynchronous job
 *
 * A single instance of Job represents a single method that will be executed
 * asynchrously. The Job is started by @p Job::exec(), which returns @p Async::Future
 * immediatelly. The Future will be set to finished state once the asynchronous
 * task has finished. You can use @p Async::Future::waitForFinished() to wait for
 * for the Future in blocking manner.
 *
 * It is possible to chain multiple Jobs one after another in different fashion
 * (sequential, parallel, etc.). Calling Job::exec() will then return a pending
 * @p Async::Future, and will execute the entire chain of jobs.
 *
 * @code
 * auto job = Job::start<QList<int>>(
 *     [](Async::Future<QList<int>> &future) {
 *         MyREST::PendingUsers *pu = MyREST::requestListOfUsers();
 *         QObject::connect(pu, &PendingOperation::finished,
 *                          [&](PendingOperation *pu) {
 *                              future->setValue(dynamic_cast<MyREST::PendingUsers*>(pu)->userIds());
 *                              future->setFinished();
 *                          });
 *      })
 * .each<QList<MyREST::User>, int>(
 *      [](const int &userId, Async::Future<QList<MyREST::User>> &future) {
 *          MyREST::PendingUser *pu = MyREST::requestUserDetails(userId);
 *          QObject::connect(pu, &PendingOperation::finished,
 *                           [&](PendingOperation *pu) {
 *                              future->setValue(Qlist<MyREST::User>() << dynamic_cast<MyREST::PendingUser*>(pu)->user());
 *                              future->setFinished();
 *                           });
 *      });
 *
 * Async::Future<QList<MyREST::User>> usersFuture = job.exec();
 * usersFuture.waitForFinished();
 * QList<MyRest::User> users = usersFuture.value();
 * @endcode
 *
 * In the example above, calling @p job.exec() will first invoke the first job,
 * which will retrieve a list of IDs, and then will invoke the second function
 * for each single entry in the list returned by the first function.
 */
template<typename Out, typename ... In>
class Job : public JobBase
{
    template<typename OutOther, typename ... InOther>
    friend class Job;

    template<typename OutOther>
    friend Job<OutOther> start(Async::ThenTask<OutOther> func);

public:
    template<typename OutOther, typename ... InOther>
    Job<OutOther, InOther ...> then(ThenTask<OutOther, InOther ...> func, ErrorHandler errorFunc = ErrorHandler())
    {
        //FIXME are we leaking the executor?
        //The executor is copied with ever job instance, so use a sharedpointer?
        return Job<OutOther, InOther ...>(new Private::ThenExecutor<OutOther, InOther ...>(func, errorFunc, mExecutor));
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

    Async::Future<Out> exec()
    {
        mExecutor->exec();
        return result();
    }

    Async::Future<Out> result() const
    {
        return *static_cast<Async::Future<Out>*>(mExecutor->result());
    }

private:
    Job(Private::ExecutorBase *executor)
        : JobBase(executor)
    {}
};

} // namespace Async


// ********** Out of line definitions ****************

namespace Async {

template<typename Out>
Job<Out> start(ThenTask<Out> func)
{
    //FIXME we're leaking the exucutor, use a shared pointer
    return Job<Out>(new Private::ThenExecutor<Out>(func));
}

namespace Private {

template<typename PrevOut, typename Out, typename ... In>
Future<PrevOut>* Executor<PrevOut, Out, In ...>::chainup()
{
    if (mPrev) {
        mPrev->exec();
        return static_cast<Async::Future<PrevOut>*>(mPrev->result());
    } else {
        return nullptr;
    }
}

template<typename PrevOut, typename Out, typename ... In>
void Executor<PrevOut, Out, In ...>::exec()
{
    mPrevFuture = chainup();
    mResult = new Async::Future<Out>();
    if (!mPrevFuture || mPrevFuture->isFinished()) {
        previousFutureReady();
    } else {
        auto futureWatcher = new Async::FutureWatcher<PrevOut>();
        QObject::connect(futureWatcher, &Async::FutureWatcher<PrevOut>::futureReady,
                         [futureWatcher, this]() {
                             assert(futureWatcher->future().isFinished());
                             futureWatcher->deleteLater();
                             previousFutureReady();
                         });
        futureWatcher->setFuture(*mPrevFuture);
    }
}

template<typename Out, typename ... In>
ThenExecutor<Out, In ...>::ThenExecutor(ThenTask<Out, In ...> then, ErrorHandler error, ExecutorBase* parent)
    : Executor<typename PreviousOut<In ...>::type, Out, In ...>(parent)
{
    this->mFunc = then;
    this->mErrorFunc = error;
}

template<typename Out, typename ... In>
void ThenExecutor<Out, In ...>::previousFutureReady()
{
    if (this->mPrevFuture) {
        assert(this->mPrevFuture->isFinished());
    }
    if (this->mPrevFuture && this->mPrevFuture->errorCode()) {
        if (this->mErrorFunc) {
            this->mErrorFunc(this->mPrevFuture->errorCode(), this->mPrevFuture->errorMessage());
        } else {
            static_cast<Async::Future<Out>*>(this->mResult)->setError(this->mPrevFuture->errorCode(), this->mPrevFuture->errorMessage());
            //propagate error if no error handler is available
            this->mFunc(this->mPrevFuture ? this->mPrevFuture->value() : In() ...,
                        *static_cast<Async::Future<Out>*>(this->mResult));
        }
    } else {
        this->mFunc(this->mPrevFuture ? this->mPrevFuture->value() : In() ...,
                    *static_cast<Async::Future<Out>*>(this->mResult));
    }
}

template<typename PrevOut, typename Out, typename In>
EachExecutor<PrevOut, Out, In>::EachExecutor(EachTask<Out, In> each, ExecutorBase* parent)
    : Executor<PrevOut, Out, In>(parent)
{
    this->mFunc = each;
}

template<typename PrevOut, typename Out, typename In>
void EachExecutor<PrevOut, Out, In>::previousFutureReady()
{
    assert(this->mPrevFuture->isFinished());
    auto out = static_cast<Async::Future<Out>*>(this->mResult);
    if (this->mPrevFuture->value().isEmpty()) {
        out->setFinished();
        return;
    }

    for (auto arg : this->mPrevFuture->value()) {
        Async::Future<Out> future;
        this->mFunc(arg, future);
        auto fw = new Async::FutureWatcher<Out>();
        mFutureWatchers.append(fw);
        QObject::connect(fw, &Async::FutureWatcher<Out>::futureReady,
                         [out, future, fw, this]() {
                             assert(future.isFinished());
                             const int index = mFutureWatchers.indexOf(fw);
                             assert(index > -1);
                             mFutureWatchers.removeAt(index);
                             out->setValue(out->value() + future.value());
                             if (mFutureWatchers.isEmpty()) {
                                 out->setFinished();
                             }
                         });
        fw->setFuture(future);
    }
}

template<typename Out, typename In>
ReduceExecutor<Out, In>::ReduceExecutor(ReduceTask<Out, In> reduce, ExecutorBase* parent)
    : Executor<In, Out, In>(parent)
{
    this->mFunc = reduce;
}

template<typename Out, typename In>
void ReduceExecutor<Out, In>::previousFutureReady()
{
    assert(this->mPrevFuture->isFinished());
    this->mFunc(this->mPrevFuture->value(), *static_cast<Async::Future<Out>*>(this->mResult));
}

} // namespace Private

} // namespace Async



#endif // ASYNC_H


