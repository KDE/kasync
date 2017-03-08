/*
 * Copyright 2014 - 2015 Daniel Vrátil <dvratil@redhat.com>
 * Copyright 2016  Daniel Vrátil <dvratil@kde.org>
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
 * TODO: Possibility to abort a job through future (perhaps optional?)
 * TODO: Support for timeout, specified during exec call, after which the error
 *       handler gets called with a defined errorCode.
 */


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
    ContinuationHelper(HandleContinuation<Out, In...> func)
        : handleContinuation(std::move(func))
    {};
    ContinuationHelper(HandleErrorContinuation<Out, In...> func)
        : handleErrorContinuation(std::move(func))
    {};
    ContinuationHelper(JobContinuation<Out, In...> func)
        : jobContinuation(std::move(func))
    {};
    ContinuationHelper(JobErrorContinuation<Out, In...> func)
        : jobErrorContinuation(std::move(func))
    {};

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

    void prepend(const ExecutorBasePtr &e)
    {
        if (mPrev) {
            mPrev->prepend(e);
        } else {
            mPrev = e;
        }
    }

    void addToContext(const QVariant &entry)
    {
        mContext << entry;
    }

    QString mExecutorName;
    QVector<QVariant> mContext;
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
        : ExecutorBase(parent)
        , executionFlag(executionFlag)
    {}

    virtual ~Executor() {}
    virtual void run(const ExecutionPtr &execution) = 0;

    ExecutionPtr exec(const ExecutorBasePtr &self) override;

    const ExecutionFlag executionFlag;

private:
    void runExecution(const KAsync::Future<PrevOut> &prevFuture, const ExecutionPtr &execution);
};

} // namespace Private
//@endcond


template<typename Out, typename ... In>
Job<Out, In ...> startImpl(Private::ContinuationHelper<Out, In ...>);

template<typename Out, typename ... In>
Job<Out, In ...> syncStartImpl(SyncContinuation<Out, In ...>);



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

///Sync void continuation without job: [] () -> T { ... }
template<typename Out = void, typename ... In, typename F>
KASYNC_EXPORT auto start(F func) -> typename std::enable_if<!std::is_base_of<JobBase, decltype(func())>::value,
                                                       Job<decltype(func()), In...>
                                                      >::type
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return syncStartImpl<Out, In...>(std::move(func));
}

///Sync continuation without job: [] () -> T { ... }
template<typename Out = void, typename ... In, typename F>
KASYNC_EXPORT auto start(F func) -> typename std::enable_if<!std::is_base_of<JobBase, decltype(func(std::declval<In...>()))>::value,
                                                       Job<decltype(func(std::declval<In...>())), In...>
                                                      >::type
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return syncStartImpl<Out, In...>(std::move(func));
}

///Void continuation with job: [] () -> KAsync::Job<...> { ... }
template<typename Out = void, typename ... In, typename F>
KASYNC_EXPORT auto start(F func) -> typename std::enable_if<std::is_base_of<JobBase, decltype(func())>::value,
                                                       Job<typename decltype(func())::OutType, In...>
                                                      >::type
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return startImpl<Out, In...>(std::move(Private::ContinuationHelper<Out, In ...>(func)));
}

///continuation with job: [] () -> KAsync::Job<...> { ... }
template<typename Out = void, typename ... In, typename F>
KASYNC_EXPORT auto start(F func) -> typename std::enable_if<std::is_base_of<JobBase, decltype(func(std::declval<In...>()))>::value,
                                                       Job<typename decltype(func(std::declval<In...>()))::OutType, In...>
                                                      >::type
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return startImpl<Out, In...>(std::move(Private::ContinuationHelper<Out, In ...>(func)));
}

///Handle continuation: [] (KAsync::Future<T>, ...) { ... }
template<typename Out = void, typename ... In>
KASYNC_EXPORT auto start(HandleContinuation<Out, In ...> func) -> Job<Out, In ...>
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return startImpl<Out, In...>(std::move(Private::ContinuationHelper<Out, In ...>(func)));
}

enum ControlFlowFlag {
    Break,
    Continue
};

/**
 * @relates Job
 *
 * Async while loop.
 *
 * Loop continues while body returns ControlFlowFlag::Continue.
 */
KASYNC_EXPORT Job<void> doWhile(Job<ControlFlowFlag> body);

/**
 * @relates Job
 *
 * Async while loop.
 *
 * Shorthand that takes a continuation.
 *
 * @see doWhile
 */
KASYNC_EXPORT Job<void> doWhile(JobContinuation<ControlFlowFlag> body);



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
template<typename List, typename ValueType = typename List::value_type>
KASYNC_EXPORT Job<void, List> forEach(KAsync::Job<void, ValueType> job);

/**
 * @relates Job
 *
 * Async foreach loop.
 *
 * Shorthand that takes a continuation.
 *
 * @see serialForEach
 */
template<typename List>
KASYNC_EXPORT Job<void, List> forEach(JobContinuation<void, typename List::value_type>);


/**
 * @relates Job
 *
 * Serial Async foreach loop.
 *
 * This will execute a job for every value in the list sequentially.
 * Errors while not stop processing of other jobs but set an error on the wrapper job.
 */
template<typename List, typename ValueType = typename List::value_type>
KASYNC_EXPORT Job<void, List> serialForEach(KAsync::Job<void, ValueType> job);

/**
 * @relates Job
 *
 * Serial Async foreach loop.
 *
 * Shorthand that takes a continuation.
 *
 * @see serialForEach
 */
template<typename List>
KASYNC_EXPORT Job<void, List> serialForEach(JobContinuation<void, typename List::value_type>);


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
KASYNC_EXPORT Job<Out> error(const char *);

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
    friend Job<OutOther, InOther ...> startImpl(Private::ContinuationHelper<OutOther, InOther ...>);

    template<typename OutOther, typename ... InOther>
    friend Job<OutOther, InOther ...> syncStartImpl(SyncContinuation<OutOther, InOther ...>);

    template<typename List, typename ValueType>
    friend Job<void, List> forEach(KAsync::Job<void, ValueType> job);

    template<typename List, typename ValueType>
    friend Job<void, List> serialForEach(KAsync::Job<void, ValueType> job);

    // Used to disable implicit conversion of Job<void to Job<void> which triggers
    // comiler warning.
    struct IncompleteType;
    //@endcond

public:
    typedef Out OutType;

    ///A continuation
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(const Job<OutOther, InOther ...> &job) const;

    ///Shorthands for a job that returns another job from it's continuation
    //
    //OutOther and InOther are only there fore backwards compatibility, but are otherwise ignored.
    //It should never be neccessary to specify any template arguments, as they are automatically deduced from the provided argument.
    //
    //We currently have to write a then overload for:
    //* One argument in the continuation
    //* No argument in the continuation
    //* One argument + error in the continuation
    //* No argument + error in the continuation
    //This is due to how we extract the return type with "decltype(func(std::declval<Out>()))".
    //Ideally we could conflate this into at least fewer overloads, but I didn't manage so far and this at least works as expected.

    ///Continuation returning job: [] (Arg) -> KAsync::Job<...> { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F func) const -> typename std::enable_if<std::is_base_of<JobBase, decltype(func(std::declval<Out>()))>::value,
                                                       Job<typename decltype(func(std::declval<Out>()))::OutType, In...>
                                                      >::type
    {
        using ResultJob = decltype(func(std::declval<Out>())); //Job<QString, int>
        return thenImpl<typename ResultJob::OutType, Out>({func}, Private::ExecutionFlag::GoodCase);
    }

    ///Void continuation with job: [] () -> KAsync::Job<...> { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F func) const -> typename std::enable_if<std::is_base_of<JobBase, decltype(func())>::value,
                                                       Job<typename decltype(func())::OutType, In...>
                                                      >::type
    {
        using ResultJob = decltype(func()); //Job<QString, void>
        return thenImpl<typename ResultJob::OutType>({func}, Private::ExecutionFlag::GoodCase);
    }

    ///Error continuation returning job: [] (KAsync::Error, Arg) -> KAsync::Job<...> { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F func) const -> typename std::enable_if<std::is_base_of<JobBase, decltype(func(KAsync::Error{}, std::declval<Out>()))>::value,
                                                       Job<typename decltype(func(KAsync::Error{}, std::declval<Out>()))::OutType, In...>
                                                      >::type
    {
        using ResultJob = decltype(func(KAsync::Error{}, std::declval<Out>())); //Job<QString, int>
        return thenImpl<typename ResultJob::OutType, Out>({func}, Private::ExecutionFlag::Always);
    }

    ///Error void continuation returning job: [] (KAsync::Error) -> KAsync::Job<...> { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F func) const -> typename std::enable_if<std::is_base_of<JobBase, decltype(func(KAsync::Error{}))>::value,
                                                       Job<typename decltype(func(KAsync::Error{}))::OutType, In...>
                                                      >::type
    {
        using ResultJob = decltype(func(KAsync::Error{}));
        return thenImpl<typename ResultJob::OutType>({func}, Private::ExecutionFlag::Always);
    }

    ///Sync continuation: [] (Arg) -> void { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F func) const -> typename std::enable_if<!std::is_base_of<JobBase, decltype(func(std::declval<Out>()))>::value,
                                                       Job<decltype(func(std::declval<Out>())), In...>
                                                      >::type
    {
        using ResultType = decltype(func(std::declval<Out>())); //QString
        return syncThenImpl<ResultType, Out>({func}, Private::ExecutionFlag::GoodCase);
    }

    ///Sync void continuation: [] () -> void { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F func) const -> typename std::enable_if<!std::is_base_of<JobBase, decltype(func())>::value,
                                                       Job<decltype(func()), In...>
                                                      >::type
    {
        using ResultType = decltype(func()); //QString
        return syncThenImpl<ResultType>({func}, Private::ExecutionFlag::GoodCase);
    }

    ///Sync error continuation: [] (KAsync::Error, Arg) -> void { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F func) const -> typename std::enable_if<!std::is_base_of<JobBase, decltype(func(KAsync::Error{}, std::declval<Out>()))>::value,
                                                       Job<decltype(func(KAsync::Error{}, std::declval<Out>())),In...>
                                                      >::type
    {
        using ResultType = decltype(func(KAsync::Error{}, std::declval<Out>())); //QString
        return syncThenImpl<ResultType, Out>({func}, Private::ExecutionFlag::Always);
    }

    ///Sync void error continuation: [] (KAsync::Error) -> void { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F func) const -> typename std::enable_if<!std::is_base_of<JobBase, decltype(func(KAsync::Error{}))>::value,
                                                       Job<decltype(func(KAsync::Error{})), In...>
                                                      >::type
    {
        using ResultType = decltype(func(KAsync::Error{}));
        return syncThenImpl<ResultType>({func}, Private::ExecutionFlag::Always);
    }

    ///Shorthand for a job that receives the error and a handle
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(HandleContinuation<OutOther, InOther ...> func) const
    {
        return thenImpl<OutOther, InOther ...>({func}, Private::ExecutionFlag::GoodCase);
    }

    ///Shorthand for a job that receives the error and a handle
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(HandleErrorContinuation<OutOther, InOther ...> func) const
    {
        return thenImpl<OutOther, InOther ...>({func}, Private::ExecutionFlag::Always);
    }

    ///Shorthand for a job that receives the error only
    Job<Out, In ...> onError(const SyncErrorContinuation<void> &errorFunc) const;

    /**
     * Shorthand for a forEach loop that automatically uses the return type of
     * this job to deduce the type exepected.
     */
    template<typename OutOther = void, typename ListType = Out, typename ValueType = typename ListType::value_type, typename std::enable_if<!std::is_void<ListType>::value, int>::type = 0>
    Job<void, In ...> each(JobContinuation<void, ValueType> func) const
    {
        eachInvariants<OutOther>();
        return then<void, In ...>(forEach<Out, ValueType>(std::move(func)));
    }

    /**
     * Shorthand for a serialForEach loop that automatically uses the return type
     * of this job to deduce the type exepected.
     */
    template<typename OutOther = void, typename ListType = Out, typename ValueType = typename ListType::value_type, typename std::enable_if<!std::is_void<ListType>::value, int>::type = 0>
    Job<void, In ...> serialEach(JobContinuation<void, ValueType> func) const
    {
        eachInvariants<OutOther>();
        return then<void, In ...>(serialForEach<Out, ValueType>(std::move(func)));
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
    operator typename std::conditional<std::is_void<OutType>::value, IncompleteType, Job<void>>::type ();

    /**
     * Adds an unnamed value to the context.
     * The context is guaranteed to persist until the jobs execution has finished.
     *
     * Useful for setting smart pointer to manage lifetime of objects required
     * during the execution of the job.
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

    explicit Job(JobContinuation<Out, In ...> func);
    explicit Job(HandleContinuation<Out, In ...> func);

private:
    //@cond PRIVATE
    explicit Job(Private::ExecutorBasePtr executor);

    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> thenImpl(Private::ContinuationHelper<OutOther, InOther ...> helper,
                                   Private::ExecutionFlag execFlag = Private::ExecutionFlag::GoodCase) const;

    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> syncThenImpl(SyncContinuation<OutOther, InOther ...> func,
                                       Private::ExecutionFlag execFlag = Private::ExecutionFlag::GoodCase) const;
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> syncThenImpl(SyncErrorContinuation<OutOther, InOther ...> func,
                                       Private::ExecutionFlag execFlag = Private::ExecutionFlag::Always) const;

    template<typename InOther, typename ... InOtherTail>
    void thenInvariants() const;

    //Base case for an empty parameter pack
    template<typename ... InOther>
    typename std::enable_if<(sizeof...(InOther) == 0)>::type
    thenInvariants() const;

    template<typename OutOther>
    void eachInvariants() const;
    //@endcond
};

} // namespace KAsync


// out-of-line definitions of Job methods
#include "job_impl.h"

#endif // KASYNC_H
