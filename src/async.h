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
#include "debug.h"
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
 * * Job: API wrapper around Executors chain. Can be destroyed while still running,
 *        because the actual execution happens in the background
 * * Executor: Describes task to execute. Executors form a linked list matching the
 *        order in which they will be executed. The Executor chain is destroyed when
 *        the parent Job is destroyed. However if the Job is still running it is
 *        guaranteed that the Executor chain will not be destroyed until the execution
 *        is finished.
 * * Execution: The running execution of the task stored in Executor. Each call to Job::exec()
 *        instantiates new Execution chain, which makes it possible for the Job to be
 *        executed multiple times (even in parallel).
 * * Future: Representation of the result that is being calculated
 *
 *
 * TODO: Composed progress reporting
 * TODO: Possibility to abort a job through future (perhaps optional?)
 * TODO: Support for timeout, specified during exec call, after which the error handler gets called with a defined errorCode.
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

struct Execution {
    Execution(const ExecutorBasePtr &executor);
    ~Execution();
    void setFinished();

    template<typename T>
    Async::Future<T>* result() const
    {
        return static_cast<Async::Future<T>*>(resultBase);
    }

    void releaseFuture();
    bool errorWasHandled() const;

    ExecutorBasePtr executor;
    FutureBase *resultBase;
    bool isRunning;
    bool isFinished;

    ExecutionPtr prevExecution;

#ifndef QT_NO_DEBUG
    Tracer *tracer;
#endif
};


typedef QSharedPointer<Execution> ExecutionPtr;

class ExecutorBase
{
    template<typename PrevOut, typename Out, typename ... In>
    friend class Executor;

    template<typename Out, typename ... In>
    friend class Async::Job;

    friend class Execution;
    friend class Async::Tracer;

public:
    virtual ~ExecutorBase();
    virtual ExecutionPtr exec(const ExecutorBasePtr &self) = 0;

protected:
    ExecutorBase(const ExecutorBasePtr &parent);

    template<typename T>
    Async::Future<T>* createFuture(const ExecutionPtr &execution) const;

    virtual bool hasErrorFunc() const = 0;
    virtual bool handleError(const ExecutionPtr &execution) = 0;

    ExecutorBasePtr mPrev;

#ifndef QT_NO_DEBUG
    QString mExecutorName;
#endif
};

template<typename PrevOut, typename Out, typename ... In>
class Executor : public ExecutorBase
{
protected:
    Executor(ErrorHandler errorFunc, const Private::ExecutorBasePtr &parent)
        : ExecutorBase(parent)
        , mErrorFunc(errorFunc)
    {}

    virtual ~Executor() {}
    virtual void run(const ExecutionPtr &execution) = 0;

    ExecutionPtr exec(const ExecutorBasePtr &self);
    bool hasErrorFunc() const { return (bool) mErrorFunc; }
    bool handleError(const ExecutionPtr &execution);

    std::function<void(int, const QString &)> mErrorFunc;
};

template<typename Out, typename ... In>
class ThenExecutor: public Executor<typename detail::prevOut<In ...>::type, Out, In ...>
{
public:
    ThenExecutor(ThenTask<Out, In ...> then, ErrorHandler errorFunc, const ExecutorBasePtr &parent);
    void run(const ExecutionPtr &execution);
private:
    ThenTask<Out, In ...> mFunc;
};

template<typename PrevOut, typename Out, typename In>
class EachExecutor : public Executor<PrevOut, Out, In>
{
public:
    EachExecutor(EachTask<Out, In> each, ErrorHandler errorFunc, const ExecutorBasePtr &parent);
    void run(const ExecutionPtr &execution);
private:
    EachTask<Out, In> mFunc;
    QVector<Async::FutureWatcher<Out>*> mFutureWatchers;
};

template<typename Out, typename In>
class ReduceExecutor : public ThenExecutor<Out, In>
{
public:
    ReduceExecutor(ReduceTask<Out, In> reduce, ErrorHandler errorFunc, const ExecutorBasePtr &parent);
private:
    ReduceTask<Out, In> mFunc;
};

template<typename Out, typename ... In>
class SyncThenExecutor : public Executor<typename detail::prevOut<In ...>::type, Out, In ...>
{
public:
    SyncThenExecutor(SyncThenTask<Out, In ...> then, ErrorHandler errorFunc, const ExecutorBasePtr &parent);
    void run(const ExecutionPtr &execution);

private:
    void run(const ExecutionPtr &execution, std::false_type); // !std::is_void<Out>
    void run(const ExecutionPtr &execution, std::true_type);  // std::is_void<Out>
    SyncThenTask<Out, In ...> mFunc;
};

template<typename Out, typename In>
class SyncReduceExecutor : public SyncThenExecutor<Out, In>
{
public:
    SyncReduceExecutor(SyncReduceTask<Out, In> reduce, ErrorHandler errorFunc, const ExecutorBasePtr &parent);
private:
    SyncReduceTask<Out, In> mFunc;
};

template<typename PrevOut, typename Out, typename In>
class SyncEachExecutor : public Executor<PrevOut, Out, In>
{
public:
    SyncEachExecutor(SyncEachTask<Out, In> each, ErrorHandler errorFunc, const ExecutorBasePtr &parent);
    void run(const ExecutionPtr &execution);
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
Job<Out, In ...> start(ThenTask<Out, In ...> func, ErrorHandler errorFunc = ErrorHandler());

template<typename Out, typename ... In>
Job<Out, In ...> start(SyncThenTask<Out, In ...> func, ErrorHandler errorFunc = ErrorHandler());

#ifdef WITH_KJOB
template<typename ReturnType, typename KJobType, ReturnType (KJobType::*KJobResultMethod)(), typename ... Args>
Job<ReturnType, Args ...> start();
#endif

/**
 * Async while loop.
 * 
 * The loop continues while @param condition returns true.
 */
Job<void> dowhile(Condition condition, ThenTask<void> func);

/**
 * Async while loop.
 *
 * Loop continues while body returns true.
 */
Job<void> dowhile(ThenTask<bool> body);

/**
 * Iterate over a container.
 *
 * Use in conjunction with .each
 */
template<typename Out>
Job<Out> iterate(const Out &container);

/**
 * Async delay.
 */
Job<void> wait(int delay);

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
    friend Job<OutOther, InOther ...> start(Async::ThenTask<OutOther, InOther ...> func, ErrorHandler errorFunc);

    template<typename OutOther, typename ... InOther>
    friend Job<OutOther, InOther ...> start(Async::SyncThenTask<OutOther, InOther ...> func, ErrorHandler errorFunc);

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
        Private::ExecutionPtr execution = mExecutor->exec(mExecutor);
        Async::Future<Out> result = *execution->result<Out>();

        return result;
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
                             [watcher, future]() {
                                 // FIXME: We pass future by value, because using reference causes the
                                 // future to get deleted before this lambda is invoked, leading to crash
                                 // in copyFutureValue()
                                 // copy by value is const
                                 auto outFuture = future;
                                 Async::detail::copyFutureValue(watcher->future(), outFuture);
                                 if (watcher->future().errorCode()) {
                                     outFuture.setError(watcher->future().errorCode(), watcher->future().errorMessage());
                                 } else {
                                     outFuture.setFinished();
                                 }
                                 delete watcher;
                             });
            watcher->setFuture(job.exec(in ...));
        };
    }
};

} // namespace Async


// ********** Out of line definitions ****************

namespace Async {

template<typename Out, typename ... In>
Job<Out, In ...> start(ThenTask<Out, In ...> func, ErrorHandler error)
{
    return Job<Out, In...>(Private::ExecutorBasePtr(
        new Private::ThenExecutor<Out, In ...>(func, error, Private::ExecutorBasePtr())));
}

template<typename Out, typename ... In>
Job<Out, In ...> start(SyncThenTask<Out, In ...> func, ErrorHandler error)
{
    return Job<Out, In...>(Private::ExecutorBasePtr(
        new Private::SyncThenExecutor<Out, In ...>(func, error, Private::ExecutorBasePtr())));
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

template<typename Out>
Job<Out> iterate(const Out &container)
{
    return Async::start<Out>(
        [container]() {
            return container;
        });
}


namespace Private {

template<typename T>
Async::Future<T>* ExecutorBase::createFuture(const ExecutionPtr &execution) const
{
    return new Async::Future<T>(execution);
}

template<typename PrevOut, typename Out, typename ... In>
ExecutionPtr Executor<PrevOut, Out, In ...>::exec(const ExecutorBasePtr &self)
{
    // Passing 'self' to execution ensures that the Executor chain remains
    // valid until the entire execution is finished
    ExecutionPtr execution = ExecutionPtr::create(self);
#ifndef QT_NO_DEBUG
    execution->tracer = new Tracer(execution.data()); // owned by execution
#endif

    // chainup
    execution->prevExecution = mPrev ? mPrev->exec(mPrev) : ExecutionPtr();

    execution->resultBase = ExecutorBase::createFuture<Out>(execution);
    auto fw = new Async::FutureWatcher<Out>();
    QObject::connect(fw, &Async::FutureWatcher<Out>::futureReady,
                     [fw, execution, this]() {
                         handleError(execution);
                         execution->setFinished();
                         delete fw;
                     });
    fw->setFuture(*execution->result<Out>());

    Async::Future<PrevOut> *prevFuture = execution->prevExecution ? execution->prevExecution->result<PrevOut>() : nullptr;
    if (!prevFuture || prevFuture->isFinished()) {
        if (prevFuture) { // prevFuture implies execution->prevExecution
            if (prevFuture->errorCode()) {
                // Propagate the errorCode and message to the outer Future
                execution->resultBase->setError(prevFuture->errorCode(), prevFuture->errorMessage());
                if (!execution->errorWasHandled()) {
                    if (handleError(execution)) {
                        return execution;
                    }
                } else {
                    return execution;
                }
            } else {
                // Propagate error (if any)
            }
        }

        execution->isRunning = true;
        run(execution);
    } else {
        auto prevFutureWatcher = new Async::FutureWatcher<PrevOut>();
        QObject::connect(prevFutureWatcher, &Async::FutureWatcher<PrevOut>::futureReady,
                         [prevFutureWatcher, execution, this]() {
                             auto prevFuture = prevFutureWatcher->future();
                             assert(prevFuture.isFinished());
                             delete prevFutureWatcher;
                             auto prevExecutor = execution->executor->mPrev;
                             if (prevFuture.errorCode()) {
                                 execution->resultBase->setError(prevFuture.errorCode(), prevFuture.errorMessage());
                                 if (!execution->errorWasHandled()) {
                                    if (handleError(execution)) {
                                        return;
                                    }
                                 } else {
                                     return;
                                 }
                             }


                             // propagate error (if any)
                             execution->isRunning = true;
                             run(execution);
                         });

        prevFutureWatcher->setFuture(*static_cast<Async::Future<PrevOut>*>(prevFuture));
    }

    return execution;
}

template<typename PrevOut, typename Out, typename ... In>
bool Executor<PrevOut, Out, In ...>::handleError(const ExecutionPtr &execution)
{
    assert(execution->resultBase->isFinished());
    if (execution->resultBase->errorCode()) {
        if (mErrorFunc) {
            mErrorFunc(execution->resultBase->errorCode(),
                       execution->resultBase->errorMessage());
            return true;
        }
    }

    return false;
}


template<typename Out, typename ... In>
ThenExecutor<Out, In ...>::ThenExecutor(ThenTask<Out, In ...> then, ErrorHandler error, const ExecutorBasePtr &parent)
    : Executor<typename detail::prevOut<In ...>::type, Out, In ...>(error, parent)
    , mFunc(then)
{
    STORE_EXECUTOR_NAME("ThenExecutor", Out, In ...);
}

template<typename Out, typename ... In>
void ThenExecutor<Out, In ...>::run(const ExecutionPtr &execution)
{
    Async::Future<typename detail::prevOut<In ...>::type> *prevFuture = nullptr;
    if (execution->prevExecution) {
        prevFuture = execution->prevExecution->result<typename detail::prevOut<In ...>::type>();
        assert(prevFuture->isFinished());
    }

    ThenExecutor<Out, In ...>::mFunc(prevFuture ? prevFuture->value() : In() ..., *execution->result<Out>());
}

template<typename PrevOut, typename Out, typename In>
EachExecutor<PrevOut, Out, In>::EachExecutor(EachTask<Out, In> each, ErrorHandler error, const ExecutorBasePtr &parent)
    : Executor<PrevOut, Out, In>(error, parent)
    , mFunc(each)
{
    STORE_EXECUTOR_NAME("EachExecutor", PrevOut, Out, In);
}

template<typename PrevOut, typename Out, typename In>
void EachExecutor<PrevOut, Out, In>::run(const ExecutionPtr &execution)
{
    assert(execution->prevExecution);
    auto prevFuture = execution->prevExecution->result<PrevOut>();
    assert(prevFuture->isFinished());

    auto out = execution->result<Out>();
    if (prevFuture->value().isEmpty()) {
        out->setFinished();
        return;
    }

    for (auto arg : prevFuture->value()) {
        //We have to manually manage the lifetime of these temporary futures
        Async::Future<Out> *future = new Async::Future<Out>();
        EachExecutor<PrevOut, Out, In>::mFunc(arg, *future);
        auto fw = new Async::FutureWatcher<Out>();
        mFutureWatchers.append(fw);
        QObject::connect(fw, &Async::FutureWatcher<Out>::futureReady,
                         [out, fw, this, future]() {
                             assert(fw->future().isFinished());
                             const int index = mFutureWatchers.indexOf(fw);
                             assert(index > -1);
                             mFutureWatchers.removeAt(index);
                             Async::detail::aggregateFutureValue<Out>(fw->future(), *out);
                             if (mFutureWatchers.isEmpty()) {
                                 out->setFinished();
                             }
                             delete fw;
                             delete future;
                         });
        fw->setFuture(*future);
    }
}

template<typename Out, typename In>
ReduceExecutor<Out, In>::ReduceExecutor(ReduceTask<Out, In> reduce, ErrorHandler errorFunc, const ExecutorBasePtr &parent)
    : ThenExecutor<Out, In>(reduce, errorFunc, parent)
{
    STORE_EXECUTOR_NAME("ReduceExecutor", Out, In);
}

template<typename Out, typename ... In>
SyncThenExecutor<Out, In ...>::SyncThenExecutor(SyncThenTask<Out, In ...> then, ErrorHandler errorFunc, const ExecutorBasePtr &parent)
    : Executor<typename detail::prevOut<In ...>::type, Out, In ...>(errorFunc, parent)
    , mFunc(then)
{
    STORE_EXECUTOR_NAME("SyncThenExecutor", Out, In ...);
}

template<typename Out, typename ... In>
void SyncThenExecutor<Out, In ...>::run(const ExecutionPtr &execution)
{
    if (execution->prevExecution) {
        assert(execution->prevExecution->resultBase->isFinished());
    }

    run(execution, std::is_void<Out>());
    execution->resultBase->setFinished();
}

template<typename Out, typename ... In>
void SyncThenExecutor<Out, In ...>::run(const ExecutionPtr &execution, std::false_type)
{
    Async::Future<typename detail::prevOut<In ...>::type> *prevFuture =
        execution->prevExecution
            ? execution->prevExecution->result<typename detail::prevOut<In ...>::type>()
            : nullptr;
    (void) prevFuture; // silence 'set but not used' warning
    Async::Future<Out> *future = execution->result<Out>();
    future->setValue(SyncThenExecutor<Out, In...>::mFunc(prevFuture ? prevFuture->value() : In() ...));
}

template<typename Out, typename ... In>
void SyncThenExecutor<Out, In ...>::run(const ExecutionPtr &execution, std::true_type)
{
    Async::Future<typename detail::prevOut<In ...>::type> *prevFuture =
        execution->prevExecution
            ? execution->prevExecution->result<typename detail::prevOut<In ...>::type>()
            : nullptr;
    (void) prevFuture; // silence 'set but not used' warning
    SyncThenExecutor<Out, In ...>::mFunc(prevFuture ? prevFuture->value() : In() ...);
}

template<typename PrevOut, typename Out, typename In>
SyncEachExecutor<PrevOut, Out, In>::SyncEachExecutor(SyncEachTask<Out, In> each, ErrorHandler errorFunc, const ExecutorBasePtr &parent)
    : Executor<PrevOut, Out, In>(errorFunc, parent)
    , mFunc(each)
{
    STORE_EXECUTOR_NAME("SyncEachExecutor", PrevOut, Out, In);
}

template<typename PrevOut, typename Out, typename In>
void SyncEachExecutor<PrevOut, Out, In>::run(const ExecutionPtr &execution)
{
    assert(execution->prevExecution);
    auto *prevFuture = execution->prevExecution->result<PrevOut>();
    assert(prevFuture->isFinished());

    auto out = execution->result<Out>();
    if (prevFuture->value().isEmpty()) {
        out->setFinished();
        return;
    }

    for (auto arg : prevFuture->value()) {
        run(out, arg, std::is_void<Out>());
    }
    out->setFinished();
}

template<typename PrevOut, typename Out, typename In>
void SyncEachExecutor<PrevOut, Out, In>::run(Async::Future<Out> *out, const typename PrevOut::value_type &arg, std::false_type)
{
    out->setValue(out->value() + SyncEachExecutor<PrevOut, Out, In>::mFunc(arg));
}

template<typename PrevOut, typename Out, typename In>
void SyncEachExecutor<PrevOut, Out, In>::run(Async::Future<Out> * /* unused */, const typename PrevOut::value_type &arg, std::true_type)
{
    SyncEachExecutor<PrevOut, Out, In>::mFunc(arg);
}

template<typename Out, typename In>
SyncReduceExecutor<Out, In>::SyncReduceExecutor(SyncReduceTask<Out, In> reduce, ErrorHandler errorFunc, const ExecutorBasePtr &parent)
    : SyncThenExecutor<Out, In>(reduce, errorFunc, parent)
{
    STORE_EXECUTOR_NAME("SyncReduceExecutor", Out, In);
}


} // namespace Private

} // namespace Async



#endif // ASYNC_H


