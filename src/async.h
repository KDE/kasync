/*
 * Copyright 2014 - 2015 Daniel Vrátil <dvratil@redhat.com>
 * Copyright 2016  Christian Mollekopf <mollekopf@kolabsystems.com>
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

#ifndef KASYNC_H
#define KASYNC_H

#include "kasync_export.h"

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


/**
 * @mainpage KAsync
 *
 * @brief API to help write async code.
 *
 * This API is based around jobs that take lambdas to execute asynchronous tasks.
 * Each async operation can take a continuation that can then be used to execute
 * further async operations. That way it is possible to build async chains of
 * operations that can be stored and executed later on. Jobs can be composed,
 * similarly to functions.
 *
 * Relations between the components:
 * * Job: API wrapper around Executors chain. Can be destroyed while still running,
 *        because the actual execution happens in the background
 * * Executor: Describes task to execute. Executors form a linked list matching the
 *        order in which they will be executed. The Executor chain is destroyed when
 *        the parent Job is destroyed. However if the Job is still running it is
 *        guaranteed that the Executor chain will not be destroyed until the execution
 *        is finished.
 * * Execution: The running execution of the task stored in Executor. Each call to
 *        Job::exec() instantiates new Execution chain, which makes it possible for
 *        the Job to be executed multiple times (even in parallel).
 * * Future: Representation of the result that is being calculated
 *
 *
 * TODO: Composed progress reporting
 * TODO: Possibility to abort a job through future (perhaps optional?)
 * TODO: Support for timeout, specified during exec call, after which the error
 *       handler gets called with a defined errorCode.
 */


class KJob;

namespace KAsync {

template<typename PrevOut, typename Out, typename ... In>
class Executor;

class JobBase;

template<typename Out, typename ... In>
class Job;

template<typename Out, typename ... In>
using HandleContinuation = typename detail::identity<std::function<void(In ..., KAsync::Future<Out>&)>>::type;

template<typename Out, typename ... In>
using HandleErrorContinuation = typename detail::identity<std::function<void(const KAsync::Error &, In ..., KAsync::Future<Out>&)>>::type;

template<typename Out, typename ... In>
using SyncContinuation = typename detail::identity<std::function<Out(In ...)>>::type;

template<typename Out, typename ... In>
using SyncErrorContinuation = typename detail::identity<std::function<Out(const KAsync::Error &, In ...)>>::type;

template<typename Out, typename ... In>
using JobContinuation = typename detail::identity<std::function<KAsync::Job<Out>(In ...)>>::type;

template<typename Out, typename ... In>
using JobErrorContinuation = typename detail::identity<std::function<KAsync::Job<Out>(const KAsync::Error &, In ...)>>::type;


//@cond PRIVATE
namespace Private
{

class ExecutorBase;
typedef QSharedPointer<ExecutorBase> ExecutorBasePtr;

struct KASYNC_EXPORT Execution {
    explicit Execution(const ExecutorBasePtr &executor);
    virtual ~Execution();
    void setFinished();

    template<typename T>
    KAsync::Future<T>* result() const
    {
        return static_cast<KAsync::Future<T>*>(resultBase);
    }

    void releaseFuture();

    ExecutorBasePtr executor;
    FutureBase *resultBase;

    ExecutionPtr prevExecution;

    Tracer *tracer;
};


template<typename Out, typename ... In>
struct ContinuationHelper {
    ContinuationHelper(const HandleContinuation<Out, In...> &func) : handleContinuation(func) {};
    ContinuationHelper(const HandleErrorContinuation<Out, In...> &func) : handleErrorContinuation(func) {};
    ContinuationHelper(const JobContinuation<Out, In...> &func) : jobContinuation(func) {};
    ContinuationHelper(const JobErrorContinuation<Out, In...> &func) : jobErrorContinuation(func) {};

    HandleContinuation<Out, In...> handleContinuation;
    HandleErrorContinuation<Out, In...> handleErrorContinuation;
    JobContinuation<Out, In...> jobContinuation;
    JobErrorContinuation<Out, In...> jobErrorContinuation;
};

typedef QSharedPointer<Execution> ExecutionPtr;

class KASYNC_EXPORT ExecutorBase
{
    template<typename PrevOut, typename Out, typename ... In>
    friend class Executor;

    template<typename Out, typename ... In>
    friend class KAsync::Job;

    friend struct Execution;
    friend class KAsync::Tracer;

public:
    virtual ~ExecutorBase();
    virtual ExecutionPtr exec(const ExecutorBasePtr &self) = 0;

protected:
    ExecutorBase(const ExecutorBasePtr &parent);

    template<typename T>
    KAsync::Future<T>* createFuture(const ExecutionPtr &execution) const;

    ExecutorBasePtr mPrev;

    void prefix(ExecutorBasePtr e)
    {
        if (mPrev) {
            mPrev->prefix(e);
        } else {
            mPrev = e;
        }
    }

    void addToContext(const QVariant &entry)
    {
        mContext << entry;
    }

    QString mExecutorName;
    QList<QVariant> mContext;
};

enum ExecutionFlag {
    Always,
    ErrorCase,
    GoodCase
};

template<typename PrevOut, typename Out, typename ... In>
class Executor : public ExecutorBase
{
protected:

    Executor(const Private::ExecutorBasePtr &parent, ExecutionFlag executionFlag)
        : ExecutorBase(parent),
        executionFlag(executionFlag)
    {}

    virtual ~Executor() {}
    virtual void run(const ExecutionPtr &execution) = 0;

    ExecutionPtr exec(const ExecutorBasePtr &self);

    const ExecutionFlag executionFlag;

private:
    void runExecution(const KAsync::Future<PrevOut> &prevFuture, ExecutionPtr execution);
};

} // namespace Private
//@endcond

/**
 * @relates Job
 *
 * Start an asynchronous job sequence.
 *
 * start() is your starting point to build a chain of jobs to be executed
 * asynchronously.
 *
 * @param func A continuation to be executed.
 */
template<typename Out, typename ... In>
KASYNC_EXPORT Job<Out, In ...> start(const HandleContinuation<Out, In ...> &func);
template<typename Out, typename ... In>
KASYNC_EXPORT Job<Out, In ...> start(const JobContinuation<Out, In ...> &func);
template<typename Out, typename ... In>
KASYNC_EXPORT Job<Out, In ...> syncStart(const SyncContinuation<Out, In ...> &func);

/**
 * @relates Job
 *
 * Async while loop.
 *
 * Loop continues while body returns ControlFlowFlag::Continue.
 */
enum ControlFlowFlag {
    Break,
    Continue
};
KASYNC_EXPORT Job<void> dowhile(JobContinuation<ControlFlowFlag> body);
KASYNC_EXPORT Job<void> dowhile(Job<ControlFlowFlag> body);


/**
 * @relates Job
 *
 * Async delay.
 */
KASYNC_EXPORT Job<void> wait(int delay);

/**
 * @relates Job
 *
 * A null job.
 *
 * An async noop.
 *
 */
template<typename Out = void>
KASYNC_EXPORT Job<Out> null();

/**
 * @relates Job
 *
 * Async value.
 */
template<typename Out>
KASYNC_EXPORT Job<Out> value(Out);

/**
 * @relates Job
 *
 * Async foreach loop.
 *
 * This will execute a job for every value in the list.
 * Errors while not stop processing of other jobs but set an error on the wrapper job.
 */
template<typename List>
KASYNC_EXPORT Job<void, List> forEach(JobContinuation<void, typename List::value_type>);

template<typename List, typename ValueType = typename List::value_type>
KASYNC_EXPORT Job<void, List> forEach(KAsync::Job<void, ValueType> job);

/**
 * @relates Job
 *
 * An error job.
 *
 * An async error.
 *
 */
template<typename Out = void>
KASYNC_EXPORT Job<Out> error(int errorCode = 1, const QString &errorMessage = QString());

/**
 * @relates Job
 *
 * An error job.
 *
 * An async error.
 *
 */
template<typename Out = void>
KASYNC_EXPORT Job<Out> error(const Error &);

//@cond PRIVATE
class KASYNC_EXPORT JobBase
{
    template<typename Out, typename ... In>
    friend class Job;

public:
    explicit JobBase(const Private::ExecutorBasePtr &executor);
    virtual ~JobBase();

protected:
    Private::ExecutorBasePtr mExecutor;
};
//@endcond

/**
 * @brief An Asynchronous job
 *
 * A single instance of Job represents a single method that will be executed
 * asynchronously. The Job is started by exec(), which returns Future
 * immediatelly. The Future will be set to finished state once the asynchronous
 * task has finished. You can use Future::waitForFinished() to wait for
 * for the Future in blocking manner.
 *
 * It is possible to chain multiple Jobs one after another in different fashion
 * (sequential, parallel, etc.). Calling exec() will then return a pending
 * Future, and will execute the entire chain of jobs.
 *
 * @code
 * auto job = Job::start<QList<int>>(
 *     [](KAsync::Future<QList<int>> &future) {
 *         MyREST::PendingUsers *pu = MyREST::requestListOfUsers();
 *         QObject::connect(pu, &PendingOperation::finished,
 *                          [&](PendingOperation *pu) {
 *                              future->setValue(dynamic_cast<MyREST::PendingUsers*>(pu)->userIds());
 *                              future->setFinished();
 *                          });
 *      })
 * .each<QList<MyREST::User>, int>(
 *      [](const int &userId, KAsync::Future<QList<MyREST::User>> &future) {
 *          MyREST::PendingUser *pu = MyREST::requestUserDetails(userId);
 *          QObject::connect(pu, &PendingOperation::finished,
 *                           [&](PendingOperation *pu) {
 *                              future->setValue(Qlist<MyREST::User>() << dynamic_cast<MyREST::PendingUser*>(pu)->user());
 *                              future->setFinished();
 *                           });
 *      });
 *
 * KAsync::Future<QList<MyREST::User>> usersFuture = job.exec();
 * usersFuture.waitForFinished();
 * QList<MyRest::User> users = usersFuture.value();
 * @endcode
 *
 * In the example above, calling @p job.exec() will first invoke the first job,
 * which will retrieve a list of IDs and then will invoke the second function
 * for each single entry in the list returned by the first function.
 */
template<typename Out, typename ... In>
class Job : public JobBase
{
    //@cond PRIVATE
    template<typename OutOther, typename ... InOther>
    friend class Job;

    template<typename OutOther, typename ... InOther>
    friend Job<OutOther, InOther ...> startImpl(const Private::ContinuationHelper<OutOther, InOther ...> &);

    template<typename List, typename ValueType>
    friend Job<void, List> forEach(KAsync::Job<void, ValueType> job);

    template<typename OutOther, typename ... InOther>
    friend Job<OutOther, InOther ...> syncStart(const SyncContinuation<OutOther, InOther ...> &func);

    //@endcond

public:
    /// A continuation
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(const Job<OutOther, InOther ...> &job);

    ///Shorthand for a job that returns another job from it's continuation
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(JobContinuation<OutOther, InOther ...> func)
    {
        return thenImpl<OutOther, InOther ...>({func}, Private::ExecutionFlag::GoodCase);
    }

    ///Shorthand for a job that receives the error and a handle
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(HandleContinuation<OutOther, InOther ...> func)
    {
        return thenImpl<OutOther, InOther ...>({func}, Private::ExecutionFlag::GoodCase);
    }

    ///Shorthand for a job that receives the error and a handle
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(HandleErrorContinuation<OutOther, InOther ...> func)
    {
        return thenImpl<OutOther, InOther ...>({func}, Private::ExecutionFlag::Always);
    }

    ///Shorthand for a job that that returns another job from its contiuation and that receives the error
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(JobErrorContinuation<OutOther, InOther ...> func)
    {
        return thenImpl<OutOther, InOther ...>({func}, Private::ExecutionFlag::Always);
    }

    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> syncThen(const SyncContinuation<OutOther, InOther ...> &func)
    {
        return syncThenImpl<OutOther, InOther ...>(func, Private::ExecutionFlag::GoodCase);
    }

    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> syncThen(const SyncErrorContinuation<OutOther, InOther ...> &func)
    {
        return syncThenImpl<OutOther, InOther ...>(func, Private::ExecutionFlag::Always);
    }

    ///Shorthand for a job that receives the error only
    Job<Out, In ...> onError(const SyncErrorContinuation<void> &errorFunc);

    ///Shorthand that automatically uses the return type of this job to deduce the type exepected
    template<typename OutOther = void, typename ListType = Out, typename ValueType = typename ListType::value_type, typename std::enable_if<!std::is_void<ListType>::value, int>::type = 0>
    Job<void, In ...> each(JobContinuation<void, ValueType> func)
    {
        eachInvariants<OutOther>();
        return then<void, In ...>(forEach<Out, ValueType>(func));
    }

    /**
     * Enable implicit conversion to Job<void>.
     *
     * This is necessary in assignments that only use the return value (which is the normal case).
     * This avoids constructs like:
     * auto job = KAsync::start<int>( ... )
     *  .then<void, int>( ... )
     *  .then<void>([](){}); //Necessary for the assignment without the implicit conversion
     */
    template<typename ... InOther>
    operator Job<void>();

    /**
     * Adds an unnamed value to the context.
     * The context is guaranteed to persist until the jobs execution has finished.
     *
     * Useful for setting smart pointer to manage lifetime of objects required during the execution of the job.
     */
    template<typename T>
    Job<Out, In ...> &addToContext(const T &value)
    {
        assert(mExecutor);
        mExecutor->addToContext(QVariant::fromValue<T>(value));
        return *this;
    }

    /**
     * @brief Starts execution of the job chain.
     *
     * This will start the execution of the task chain, starting from the
     * first one. It is possible to call this function multiple times, each
     * invocation will start a new processing and provide a new Future to
     * watch its status.
     *
     * @param in Argument to be passed to the very first task
     * @return Future&lt;Out&gt; object which will contain result of the last
     * task once if finishes executing. See Future documentation for more details.
     *
     * @see exec(), Future
     */
    template<typename FirstIn>
    KAsync::Future<Out> exec(FirstIn in);

    /**
     * @brief Starts execution of the job chain.
     *
     * This will start the execution of the task chain, starting from the
     * first one. It is possible to call this function multiple times, each
     * invocation will start a new processing and provide a new Future to
     * watch its status.
     *
     * @return Future&lt;Out&gt; object which will contain result of the last
     * task once if finishes executing. See Future documentation for more details.
     *
     * @see exec(FirstIn in), Future
     */
    KAsync::Future<Out> exec();

    explicit Job(const JobContinuation<Out, In ...> &func);
    explicit Job(const HandleContinuation<Out, In ...> &func);

private:
    //@cond PRIVATE
    explicit Job(Private::ExecutorBasePtr executor);

    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> thenImpl(const Private::ContinuationHelper<OutOther, InOther ...> &helper, Private::ExecutionFlag execFlag = Private::ExecutionFlag::GoodCase);

    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> syncThenImpl(const SyncContinuation<OutOther, InOther ...> &func, Private::ExecutionFlag execFlag = Private::ExecutionFlag::GoodCase);
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> syncThenImpl(const SyncErrorContinuation<OutOther, InOther ...> &func, Private::ExecutionFlag execFlag = Private::ExecutionFlag::Always);

    template<typename InOther, typename ... InOtherTail>
    void thenInvariants();

    //Base case for an empty parameter pack
    template<typename ... InOther>
    typename std::enable_if<(sizeof...(InOther) == 0)>::type
    thenInvariants();

    template<typename OutOther>
    void eachInvariants();

    template<typename InOther>
    void reduceInvariants();
    //@endcond
};

} // namespace KAsync


// out-of-line definitions of Job methods
#include "job_impl.h"

#endif // KASYNC_H
