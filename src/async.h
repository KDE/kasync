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
#include <QSharedPointer>

#include <QDebug>

/*
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
template<typename Out, typename ... In>
using SyncThenTask = typename detail::identity<std::function<Out(In ...)>>::type;
template<typename Out, typename In>
using EachTask = typename detail::identity<std::function<void(In, Async::Future<Out>&)>>::type;
template<typename Out, typename In>
using SyncEachTask = typename detail::identity<std::function<Out(In)>>::type;
template<typename Out, typename In>
using ReduceTask = typename detail::identity<std::function<void(In, Async::Future<Out>&)>>::type;
template<typename Out, typename In>
using SyncReduceTask = typename detail::identity<std::function<Out(In)>>::type;

using ErrorHandler = std::function<void(int, const QString &)>;

namespace Private
{

class ExecutorBase;
typedef QSharedPointer<ExecutorBase> ExecutorBasePtr;


class ExecutorBase
{
    template<typename PrevOut, typename Out, typename ... In>
    friend class Executor;

    template<typename Out, typename ... In>
    friend class Async::Job;

public:
    virtual ~ExecutorBase();
    virtual void exec() = 0;

    inline FutureBase* result() const
    {
        return mResult;
    }

protected:
    ExecutorBase(const ExecutorBasePtr &parent);

    ExecutorBasePtr mPrev;
    FutureBase *mResult;
};

template<typename PrevOut, typename Out, typename ... In>
class Executor : public ExecutorBase
{
protected:
    Executor(ErrorHandler errorHandler, const Private::ExecutorBasePtr &parent)
        : ExecutorBase(parent)
        , mErrorFunc(errorHandler)
        , mPrevFuture(0)
    {}
    virtual ~Executor() {}
    inline Async::Future<PrevOut>* chainup();
    virtual void previousFutureReady() = 0;

    void exec();

    //std::function<void(const In& ..., Async::Future<Out> &)> mFunc;
    std::function<void(int, const QString &)> mErrorFunc;
    Async::Future<PrevOut> *mPrevFuture;
};

template<typename Out, typename ... In>
class ThenExecutor: public Executor<typename detail::prevOut<In ...>::type, Out, In ...>
{
public:
    ThenExecutor(ThenTask<Out, In ...> then, ErrorHandler errorHandler, const ExecutorBasePtr &parent);
    void previousFutureReady();
private:
    ThenTask<Out, In ...> mFunc;
};

template<typename PrevOut, typename Out, typename In>
class EachExecutor : public Executor<PrevOut, Out, In>
{
public:
    EachExecutor(EachTask<Out, In> each, ErrorHandler errorHandler, const ExecutorBasePtr &parent);
    void previousFutureReady();
private:
    EachTask<Out, In> mFunc;
    QVector<Async::FutureWatcher<PrevOut>*> mFutureWatchers;
};

template<typename Out, typename In>
class ReduceExecutor : public ThenExecutor<Out, In>
{
public:
    ReduceExecutor(ReduceTask<Out, In> reduce, ErrorHandler errorHandler, const ExecutorBasePtr &parent);
private:
    ReduceTask<Out, In> mFunc;
};

template<typename Out, typename ... In>
class SyncThenExecutor : public Executor<typename detail::prevOut<In ...>::type, Out, In ...>
{
public:
    SyncThenExecutor(SyncThenTask<Out, In ...> then, ErrorHandler errorHandler, const ExecutorBasePtr &parent);
    void previousFutureReady();
private:
    SyncThenTask<Out, In ...> mFunc;
};

template<typename Out, typename In>
class SyncReduceExecutor : public SyncThenExecutor<Out, In>
{
public:
    SyncReduceExecutor(SyncReduceTask<Out, In> reduce, ErrorHandler errorHandler, const ExecutorBasePtr &parent);
private:
    SyncReduceTask<Out, In> mFunc;
};

template<typename PrevOut, typename Out, typename In>
class SyncEachExecutor : public Executor<PrevOut, Out, In>
{
public:
    SyncEachExecutor(SyncEachTask<Out, In> each, ErrorHandler errorHandler, const ExecutorBasePtr &parent);
    void previousFutureReady();
private:
    SyncEachTask<Out, In> mFunc;
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
template<typename Out, typename ... In>
Job<Out, In ...> start(ThenTask<Out, In ...> func);

template<typename Out, typename ... In>
Job<Out, In ...> start(SyncThenTask<Out, In ...> func);


/**
 * A null job.
 * 
 * An async noop.
 *
 */
template<typename Out>
Job<Out> null();

/**
 * An error job.
 *
 * An async error.
 *
 */
template<typename Out>
Job<Out> error(int errorCode = 1, const QString &errorMessage = QString());


class JobBase
{
    template<typename Out, typename ... In>
    friend class Job;

public:
    JobBase(const Private::ExecutorBasePtr &executor);
    ~JobBase();

protected:
    Private::ExecutorBasePtr mExecutor;
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

    template<typename OutOther, typename ... InOther>
    friend Job<OutOther, InOther ...> start(Async::ThenTask<OutOther, InOther ...> func);

    template<typename OutOther, typename ... InOther>
    friend Job<OutOther, InOther ...> start(Async::SyncThenTask<OutOther, InOther ...> func);

public:
    template<typename OutOther, typename ... InOther>
    Job<OutOther, InOther ...> then(ThenTask<OutOther, InOther ...> func, ErrorHandler errorFunc = ErrorHandler())
    {
        return Job<OutOther, InOther ...>(Private::ExecutorBasePtr(
            new Private::ThenExecutor<OutOther, InOther ...>(func, errorFunc, mExecutor)));
    }

    template<typename OutOther, typename ... InOther>
    Job<OutOther, InOther ...> then(SyncThenTask<OutOther, InOther ...> func, ErrorHandler errorFunc = ErrorHandler())
    {
        return Job<OutOther, InOther ...>(Private::ExecutorBasePtr(
            new Private::SyncThenExecutor<OutOther, InOther ...>(func, errorFunc, mExecutor)));
    }

    template<typename OutOther, typename InOther>
    Job<OutOther, InOther> each(EachTask<OutOther, InOther> func, ErrorHandler errorFunc = ErrorHandler())
    {
        eachInvariants<OutOther>();
        return Job<OutOther, InOther>(Private::ExecutorBasePtr(
            new Private::EachExecutor<Out, OutOther, InOther>(func, errorFunc, mExecutor)));
    }

    template<typename OutOther, typename InOther>
    Job<OutOther, InOther> each(SyncEachTask<OutOther, InOther> func, ErrorHandler errorFunc = ErrorHandler())
    {
        eachInvariants<OutOther>();
        return Job<OutOther, InOther>(Private::ExecutorBasePtr(
            new Private::SyncEachExecutor<Out, OutOther, InOther>(func, errorFunc, mExecutor)));
    }

    template<typename OutOther, typename InOther>
    Job<OutOther, InOther> reduce(ReduceTask<OutOther, InOther> func, ErrorHandler errorFunc = ErrorHandler())
    {
        reduceInvariants<InOther>();
        return Job<OutOther, InOther>(Private::ExecutorBasePtr(
            new Private::ReduceExecutor<OutOther, InOther>(func, errorFunc, mExecutor)));
    }

    template<typename OutOther, typename InOther>
    Job<OutOther, InOther> reduce(SyncReduceTask<OutOther, InOther> func, ErrorHandler errorFunc = ErrorHandler())
    {
        reduceInvariants<InOther>();
        return Job<OutOther, InOther>(Private::ExecutorBasePtr(
            new Private::SyncReduceExecutor<OutOther, InOther>(func, errorFunc, mExecutor)));
    }

    template<typename FirstIn>
    Async::Future<Out> exec(FirstIn in)
    {
        // Inject a fake sync executor that will return the initial value
        Private::ExecutorBasePtr first = mExecutor;
        while (first->mPrev) {
            first = first->mPrev;
        }
        auto init = new Private::SyncThenExecutor<FirstIn>(
            [in]() -> FirstIn {
                return in;
            },
            ErrorHandler(), Private::ExecutorBasePtr());
        first->mPrev = Private::ExecutorBasePtr(init);

        auto result = exec();
        // Remove the injected executor
        first->mPrev.reset();
        return result;
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
    Job(Private::ExecutorBasePtr executor)
        : JobBase(executor)
    {}

    template<typename OutOther>
    void eachInvariants()
    {
        static_assert(detail::isIterable<Out>::value,
                      "The 'Each' task can only be connected to a job that returns a list or an array.");
        static_assert(detail::isIterable<OutOther>::value,
                      "The result type of 'Each' task must be a list or an array.");
    }

    template<typename InOther>
    void reduceInvariants()
    {
        static_assert(Async::detail::isIterable<Out>::value,
                      "The 'Result' task can only be connected to a job that returns a list or an array");
        static_assert(std::is_same<typename Out::value_type, typename InOther::value_type>::value,
                      "The return type of previous task must be compatible with input type of this task");
    }
};

} // namespace Async


// ********** Out of line definitions ****************

namespace Async {

template<typename Out, typename ... In>
Job<Out, In ...> start(ThenTask<Out, In ...> func)
{
    return Job<Out, In...>(Private::ExecutorBasePtr(
        new Private::ThenExecutor<Out, In ...>(func, ErrorHandler(), Private::ExecutorBasePtr())));
}

template<typename Out, typename ... In>
Job<Out, In ...> start(SyncThenTask<Out, In ...> func)
{
    return Job<Out, In...>(Private::ExecutorBasePtr(
        new Private::SyncThenExecutor<Out, In ...>(func, ErrorHandler(), Private::ExecutorBasePtr())));
}

template<typename Out>
Job<Out> null()
{
    return Async::start<Out>(
        [](Async::Future<Out> &future) {
            future.setFinished();
        });
}

template<typename Out>
Job<Out> error(int errorCode, const QString &errorMessage)
{
    return Async::start<Out>(
        [errorCode, errorMessage](Async::Future<Out> &future) {
            future.setError(errorCode, errorMessage);
        });
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
    // Initialize our future
    mResult = new Async::Future<Out>();
    if (!mPrevFuture || mPrevFuture->isFinished()) {
        if (mPrevFuture && mPrevFuture->errorCode() != 0) {
            if (mErrorFunc) {
                mErrorFunc(mPrevFuture->errorCode(), mPrevFuture->errorMessage());
                mResult->setFinished();
                return;
            } else {
                // Propagate the error to next caller
            }
        }
        previousFutureReady();
    } else {
        auto futureWatcher = new Async::FutureWatcher<PrevOut>();
        QObject::connect(futureWatcher, &Async::FutureWatcher<PrevOut>::futureReady,
                         [futureWatcher, this]() {
                             auto prevFuture = futureWatcher->future();
                             assert(prevFuture.isFinished());
                             futureWatcher->deleteLater();
                             if (prevFuture.errorCode() != 0) {
                                 if (mErrorFunc) {
                                     mErrorFunc(prevFuture.errorCode(), prevFuture.errorMessage());
                                     mResult->setFinished();
                                     return;
                                 } else {
                                     // Propagate the error to next caller
                                 }
                             }
                             previousFutureReady();
                         });

        futureWatcher->setFuture(*mPrevFuture);
    }
}


template<typename Out, typename ... In>
ThenExecutor<Out, In ...>::ThenExecutor(ThenTask<Out, In ...> then, ErrorHandler error, const ExecutorBasePtr &parent)
    : Executor<typename detail::prevOut<In ...>::type, Out, In ...>(error, parent)
    , mFunc(then)
{
}

template<typename Out, typename ... In>
void ThenExecutor<Out, In ...>::previousFutureReady()
{
    if (this->mPrevFuture) {
        assert(this->mPrevFuture->isFinished());
    }

    this->mFunc(this->mPrevFuture ? this->mPrevFuture->value() : In() ...,
                *static_cast<Async::Future<Out>*>(this->mResult));
}

template<typename PrevOut, typename Out, typename In>
EachExecutor<PrevOut, Out, In>::EachExecutor(EachTask<Out, In> each, ErrorHandler error, const ExecutorBasePtr &parent)
    : Executor<PrevOut, Out, In>(error, parent)
    , mFunc(each)
{
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
        auto future = new Async::Future<Out>;
        this->mFunc(arg, *future);
        auto fw = new Async::FutureWatcher<Out>();
        mFutureWatchers.append(fw);
        QObject::connect(fw, &Async::FutureWatcher<Out>::futureReady,
                         [out, future, fw, this]() {
                             assert(future->isFinished());
                             const int index = mFutureWatchers.indexOf(fw);
                             assert(index > -1);
                             mFutureWatchers.removeAt(index);
                             out->setValue(out->value() + future->value());
                             delete future;
                             if (mFutureWatchers.isEmpty()) {
                                 out->setFinished();
                             }
                         });
        fw->setFuture(*future);
    }
}

template<typename Out, typename In>
ReduceExecutor<Out, In>::ReduceExecutor(ReduceTask<Out, In> reduce, ErrorHandler error, const ExecutorBasePtr &parent)
    : ThenExecutor<Out, In>(reduce, error, parent)
{
}

template<typename Out, typename ... In>
SyncThenExecutor<Out, In ...>::SyncThenExecutor(SyncThenTask<Out, In ...> then, ErrorHandler errorHandler, const ExecutorBasePtr &parent)
    : Executor<typename detail::prevOut<In ...>::type, Out, In ...>(errorHandler, parent)
    , mFunc(then)
{
}

template<typename Out, typename ... In>
void SyncThenExecutor<Out, In ...>::previousFutureReady()
{
    if (this->mPrevFuture) {
        assert(this->mPrevFuture->isFinished());
    }

    Out result = this->mFunc(this->mPrevFuture ? this->mPrevFuture->value() : In() ...);
    static_cast<Async::Future<Out>*>(this->mResult)->setValue(result);
    this->mResult->setFinished();
}

template<typename PrevOut, typename Out, typename In>
SyncEachExecutor<PrevOut, Out, In>::SyncEachExecutor(SyncEachTask<Out, In> each, ErrorHandler errorHandler, const ExecutorBasePtr &parent)
    : Executor<PrevOut, Out, In>(errorHandler, parent)
    , mFunc(each)
{
}

template<typename PrevOut, typename Out, typename In>
void SyncEachExecutor<PrevOut, Out, In>::previousFutureReady()
{
    assert(this->mPrevFuture->isFinished());
    auto out = static_cast<Async::Future<Out>*>(this->mResult);
    if (this->mPrevFuture->value().isEmpty()) {
        out->setFinished();
        return;
    }

    for (auto arg : this->mPrevFuture->value()) {
        out->setValue(out->value() + this->mFunc(arg));
    }
    out->setFinished();
}

template<typename Out, typename In>
SyncReduceExecutor<Out, In>::SyncReduceExecutor(SyncReduceTask<Out, In> reduce, ErrorHandler errorHandler, const ExecutorBasePtr &parent)
    : SyncThenExecutor<Out, In>(reduce, errorHandler, parent)
{
}


} // namespace Private

} // namespace Async



#endif // ASYNC_H


