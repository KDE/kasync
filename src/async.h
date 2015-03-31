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

#ifdef WITH_KJOB
#include <KJob>
#endif


/*
 * API to help write async code.
 *
 * This API is based around jobs that take lambdas to execute asynchronous tasks. Each async operation can take a continuation,
 * that can then be used to execute further async operations. That way it is possible to build async chains of operations,
 * that can be stored and executed later on. Jobs can be composed, similarly to functions.
 *
 * Relations between the components:
 * * Job: description of what should happen
 * * Executor: Running execution of a job, the process that calculates the result.
 * * Future: Representation of the result that is being calculated
 *
 * Lifetime:
 * * Before a job is executed is treated like a normal value on the stack.
 * * As soon as the job is executed, a heap allocated executor keeps the task running until complete. The associated future handle remains
 *   valid until the task is complete. To abort a job it has to be killed through the future handle.
 *   TODO: Can we tie the lifetime of the executor to the last available future handle?
 *
 * TODO: Progress reporting through future
 * TODO: Possibility to abort a job through future (perhaps optional?)
 * TODO: Support for timeout, specified during exec call, after which the error handler gets called with a defined errorCode.
 * TODO: Repeated execution of a job to facilitate i.e. an async while loop of a job?
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
using Condition = std::function<bool()>;

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
    bool mIsRunning;
    bool mIsFinished;
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
    void run(std::false_type); // !std::is_void<Out>
    void run(std::true_type);  // std::is_void<Out>
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
    void run(Async::Future<Out> *future, const typename PrevOut::value_type &arg, std::false_type); // !std::is_void<Out>
    void run(Async::Future<Out> *future, const typename PrevOut::value_type &arg, std::true_type);  // std::is_void<Out>
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

#ifdef WITH_KJOB
template<typename ReturnType, typename KJobType, ReturnType (KJobType::*KJobResultMethod)(), typename ... Args>
Job<ReturnType, Args ...> start();
#endif

template<typename Out>
Job<Out> dowhile(Condition condition, ThenTask<void> func);


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

#ifdef WITH_KJOB
    template<typename ReturnType, typename KJobType, ReturnType (KJobType::*KJobResultMethod)(), typename ... Args>
    friend Job<ReturnType, Args ...> start();
#endif

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

    template<typename OutOther, typename ... InOther>
    Job<OutOther, InOther ...> then(Job<OutOther, InOther ...> otherJob, ErrorHandler errorFunc = ErrorHandler())
    {
        return then<OutOther, InOther ...>(nestedJobWrapper<OutOther, InOther ...>(otherJob), errorFunc);
    }

#ifdef WITH_KJOB
    template<typename ReturnType, typename KJobType, ReturnType (KJobType::*KJobResultMethod)(), typename ... Args>
    Job<ReturnType, Args ...> then()
    {
        return start<ReturnType, KJobType, KJobResultMethod, Args ...>();
    }
#endif

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
    Job<OutOther, InOther> each(Job<OutOther, InOther> otherJob, ErrorHandler errorFunc = ErrorHandler())
    {
        eachInvariants<OutOther>();
        return each<OutOther, InOther>(nestedJobWrapper<OutOther, InOther>(otherJob), errorFunc);
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

    template<typename OutOther, typename InOther>
    Job<OutOther, InOther> reduce(Job<OutOther, InOther> otherJob, ErrorHandler errorFunc = ErrorHandler())
    {
        return reduce<OutOther, InOther>(nestedJobWrapper<OutOther, InOther>(otherJob), errorFunc);
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
        static_assert(std::is_void<OutOther>::value || detail::isIterable<OutOther>::value,
                      "The result type of 'Each' task must be void, a list or an array.");
    }

    template<typename InOther>
    void reduceInvariants()
    {
        static_assert(Async::detail::isIterable<Out>::value,
                      "The 'Result' task can only be connected to a job that returns a list or an array");
        static_assert(std::is_same<typename Out::value_type, typename InOther::value_type>::value,
                      "The return type of previous task must be compatible with input type of this task");
    }

    template<typename OutOther, typename ... InOther>
    inline std::function<void(InOther ..., Async::Future<OutOther>&)> nestedJobWrapper(Job<OutOther, InOther ...> otherJob) {
        return [otherJob](InOther ... in, Async::Future<OutOther> &future) {
            // copy by value is const
            auto job = otherJob;
            FutureWatcher<OutOther> *watcher = new FutureWatcher<OutOther>();
            QObject::connect(watcher, &FutureWatcherBase::futureReady,
                             [watcher, &future]() {
                                 Async::detail::copyFutureValue(watcher->future(), future);
                                 future.setFinished();
                                 watcher->deleteLater();
                             });
            watcher->setFuture(job.exec(in ...));
        };
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

#ifdef WITH_KJOB
template<typename ReturnType, typename KJobType, ReturnType (KJobType::*KJobResultMethod)(), typename ... Args>
Job<ReturnType, Args ...> start()
{
    return Job<ReturnType, Args ...>(Private::ExecutorBasePtr(
        new Private::ThenExecutor<ReturnType, Args ...>([](const Args & ... args, Async::Future<ReturnType> &future)
            {
                KJobType *job = new KJobType(args ...);
                job->connect(job, &KJob::finished,
                             [&future](KJob *job) {
                                 if (job->error()) {
                                     future.setError(job->error(), job->errorString());
                                 } else {
                                    future.setValue((static_cast<KJobType*>(job)->*KJobResultMethod)());
                                    future.setFinished();
                                 }
                             });
                job->start();
            }, ErrorHandler(), Private::ExecutorBasePtr())));
}
#endif

static void asyncWhile(const std::function<void(std::function<void(bool)>)> &body, const std::function<void()> &completionHandler) {
    body([body, completionHandler](bool complete) {
        if (complete) {
            completionHandler();
        } else {
            asyncWhile(body, completionHandler);
        }
    });
}
    }
template<typename Out>
Job<Out> dowhile(Condition condition, ThenTask<void> body)
{
    return Async::start<void>([body, condition](Async::Future<void> &future) {
        asyncWhile([condition, body](std::function<void(bool)> whileCallback) {
            Async::start<void>(body).then<void>([whileCallback, condition]() {
                whileCallback(!condition());
            }).exec();
        },
        [&future]() { //while complete
            future.setFinished();
        });
    });
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
    // Don't chain up to job that already is running (or is finished)
    if (mPrev && !mPrev->mIsRunning & !mPrev->mIsFinished) {
        mPrevFuture = chainup();
    } else if (mPrev && !mPrevFuture) {
        // If previous job is running or finished, just get it's future
        mPrevFuture = static_cast<Async::Future<PrevOut>*>(mPrev->result());
    }

    // Initialize our future
    mResult = new Async::Future<Out>();
    auto fw = new Async::FutureWatcher<Out>();
    QObject::connect(fw, &Async::FutureWatcher<Out>::futureReady,
                     [&]() {
                         mIsFinished = true;
                         fw->deleteLater();
                     });

    if (!mPrevFuture || mPrevFuture->isFinished()) {
        if (mPrevFuture && mPrevFuture->errorCode() != 0) {
            if (mErrorFunc) {
                mErrorFunc(mPrevFuture->errorCode(), mPrevFuture->errorMessage());
                mResult->setFinished();
                mIsFinished = true;
                return;
            } else {
                // Propagate the error to next caller
            }
        }
        mIsRunning = true;
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
                             mIsRunning = true;
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

    run(std::is_void<Out>());
    this->mResult->setFinished();
}

template<typename Out, typename ... In>
void SyncThenExecutor<Out, In ...>::run(std::false_type)
{
    Out result = this->mFunc(this->mPrevFuture ? this->mPrevFuture->value() : In() ...);
    static_cast<Async::Future<Out>*>(this->mResult)->setValue(result);
}

template<typename Out, typename ... In>
void SyncThenExecutor<Out, In ...>::run(std::true_type)
{
    this->mFunc(this->mPrevFuture ? this->mPrevFuture->value() : In() ...);
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
        run(out, arg, std::is_void<Out>());
    }
    out->setFinished();
}

template<typename PrevOut, typename Out, typename In>
void SyncEachExecutor<PrevOut, Out, In>::run(Async::Future<Out> *out, const typename PrevOut::value_type &arg, std::false_type)
{
    out->setValue(out->value() + this->mFunc(arg));
}

template<typename PrevOut, typename Out, typename In>
void SyncEachExecutor<PrevOut, Out, In>::run(Async::Future<Out> * /* unushed */, const typename PrevOut::value_type &arg, std::true_type)
{
    this->mFunc(arg);
}

template<typename Out, typename In>
SyncReduceExecutor<Out, In>::SyncReduceExecutor(SyncReduceTask<Out, In> reduce, ErrorHandler errorHandler, const ExecutorBasePtr &parent)
    : SyncThenExecutor<Out, In>(reduce, errorHandler, parent)
{
}


} // namespace Private

} // namespace Async



#endif // ASYNC_H


