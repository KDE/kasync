/*
    SPDX-FileCopyrightText: 2014-2015 Daniel Vrátil <dvratil@redhat.com>
    SPDX-FileCopyrightText: 2016 Daniel Vrátil <dvratil@kde.org>
    SPDX-FileCopyrightText: 2016 Christian Mollekopf <mollekopf@kolabsystems.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KASYNC_H
#define KASYNC_H

#include "kasync_export.h"

#include <functional>
#include <type_traits>
#include <cassert>

#include <QVariant>

#include "future.h"
#include "debug.h"

#include "continuations_p.h"
#include "executor_p.h"

class QObject;

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

//@cond PRIVATE
namespace Private {

template<typename Out, typename ... In>
Job<Out, In ...> startImpl(Private::ContinuationHolder<Out, In ...> &&helper)
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return Job<Out, In...>(QSharedPointer<Private::Executor<Out, In ...>>::create(
                std::forward<Private::ContinuationHolder<Out, In...>>(helper), nullptr, Private::ExecutionFlag::GoodCase));
}

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

///Sync continuation without job: [] () -> T { ... }
template<typename Out = void, typename ... In, typename F>
auto start(F &&func) -> std::enable_if_t<!std::is_base_of<JobBase, decltype(func(std::declval<In>() ...))>::value,
                                         Job<decltype(func(std::declval<In>() ...)), In...>>
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return Private::startImpl<Out, In...>(Private::ContinuationHolder<Out, In ...>(SyncContinuation<Out, In ...>(std::forward<F>(func))));
}

///continuation with job: [] () -> KAsync::Job<...> { ... }
template<typename Out = void, typename ... In, typename F>
auto start(F &&func) -> std::enable_if_t<std::is_base_of<JobBase, decltype(func(std::declval<In>() ...))>::value,
                                         Job<typename decltype(func(std::declval<In>() ...))::OutType, In...>>
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return Private::startImpl<Out, In...>(Private::ContinuationHolder<Out, In ...>(JobContinuation<Out, In...>(std::forward<F>(func))));
}

///Handle continuation: [] (KAsync::Future<T>, ...) { ... }
template<typename Out = void, typename ... In>
auto start(AsyncContinuation<Out, In ...> &&func) -> Job<Out, In ...>
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return Private::startImpl<Out, In...>(Private::ContinuationHolder<Out, In ...>(std::forward<AsyncContinuation<Out, In ...>>(func)));
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
KASYNC_EXPORT Job<void> doWhile(const Job<ControlFlowFlag> &body);

/**
 * @relates Job
 *
 * Async while loop.
 *
 * Shorthand that takes a continuation.
 *
 * @see doWhile
 */
KASYNC_EXPORT Job<void> doWhile(const JobContinuation<ControlFlowFlag> &body);



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
Job<Out> null();

/**
 * @relates Job
 *
 * Async value.
 */
template<typename Out>
Job<Out> value(Out);

/**
 * @relates Job
 *
 * Async foreach loop.
 *
 * This will execute a job for every value in the list.
 * Errors while not stop processing of other jobs but set an error on the wrapper job.
 */
template<typename List, typename ValueType = typename List::value_type>
Job<void, List> forEach(KAsync::Job<void, ValueType> job);

/**
 * @relates Job
 *
 * Async foreach loop.
 *
 * Shorthand that takes a continuation.
 *
 * @see serialForEach
 */
template<typename List, typename ValueType = typename List::value_type>
 Job<void, List> forEach(JobContinuation<void, ValueType> &&);


/**
 * @relates Job
 *
 * Serial Async foreach loop.
 *
 * This will execute a job for every value in the list sequentially.
 * Errors while not stop processing of other jobs but set an error on the wrapper job.
 */
template<typename List, typename ValueType = typename List::value_type>
Job<void, List> serialForEach(KAsync::Job<void, ValueType> job);

/**
 * @relates Job
 *
 * Serial Async foreach loop.
 *
 * Shorthand that takes a continuation.
 *
 * @see serialForEach
 */
template<typename List, typename ValueType = typename List::value_type>
Job<void, List> serialForEach(JobContinuation<void, ValueType> &&);

/**
 * @brief Wait until all given futures are completed.
 */
template<template<typename> class Container>
Job<void> waitForCompletion(Container<KAsync::Future<void>> &futures);

/**
 * @relates Job
 *
 * An error job.
 *
 * An async error.
 *
 */
template<typename Out = void>
Job<Out> error(int errorCode = 1, const QString &errorMessage = QString());

/**
 * @relates Job
 *
 * An error job.
 *
 * An async error.
 *
 */
template<typename Out = void>
Job<Out> error(const char *);

/**
 * @relates Job
 *
 * An error job.
 *
 * An async error.
 *
 */
template<typename Out = void>
Job<Out> error(const Error &);

//@cond PRIVATE
class KASYNC_EXPORT JobBase
{
    template<typename Out, typename ... In>
    friend class Job;

public:
    explicit JobBase(const Private::ExecutorBasePtr &executor)
        : mExecutor(executor)
    {}

    virtual ~JobBase() = default;

protected:
    Private::ExecutorBasePtr mExecutor;
};
//@endcond

/**
 * @brief An Asynchronous job
 *
 * A single instance of Job represents a single method that will be executed
 * asynchronously. The Job is started by exec(), which returns Future
 * immediately. The Future will be set to finished state once the asynchronous
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
class [[nodiscard]] Job : public JobBase
{
    //@cond PRIVATE
    template<typename OutOther, typename ... InOther>
    friend class Job;

    template<typename OutOther, typename ... InOther>
    friend Job<OutOther, InOther ...> Private::startImpl(Private::ContinuationHolder<OutOther, InOther ...> &&);

    template<typename List, typename ValueType>
    friend  Job<void, List> forEach(KAsync::Job<void, ValueType> job);

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
    //It should never be necessary to specify any template arguments, as they are automatically deduced from the provided argument.
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
    auto then(F &&func) const -> std::enable_if_t<std::is_base_of<JobBase, decltype(func(std::declval<Out>()))>::value,
                                                  Job<typename decltype(func(std::declval<Out>()))::OutType, In...>>
    {
        using ResultJob = decltype(func(std::declval<Out>())); //Job<QString, int>
        return thenImpl<typename ResultJob::OutType, Out>(
                {JobContinuation<typename ResultJob::OutType, Out>(std::forward<F>(func))}, Private::ExecutionFlag::GoodCase);
    }

    ///Void continuation with job: [] () -> KAsync::Job<...> { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F &&func) const -> std::enable_if_t<std::is_base_of<JobBase, decltype(func())>::value,
                                                  Job<typename decltype(func())::OutType, In...>>
    {
        using ResultJob = decltype(func()); //Job<QString, void>
        return thenImpl<typename ResultJob::OutType>(
                {JobContinuation<typename ResultJob::OutType>(std::forward<F>(func))}, Private::ExecutionFlag::GoodCase);
    }

    ///Error continuation returning job: [] (KAsync::Error, Arg) -> KAsync::Job<...> { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F &&func) const -> std::enable_if_t<std::is_base_of<JobBase, decltype(func(KAsync::Error{}, std::declval<Out>()))>::value,
                                                  Job<typename decltype(func(KAsync::Error{}, std::declval<Out>()))::OutType, In...>>
    {
        using ResultJob = decltype(func(KAsync::Error{}, std::declval<Out>())); //Job<QString, int>
        return thenImpl<typename ResultJob::OutType, Out>(
                {JobErrorContinuation<typename ResultJob::OutType, Out>(std::forward<F>(func))}, Private::ExecutionFlag::Always);
    }

    ///Error void continuation returning job: [] (KAsync::Error) -> KAsync::Job<...> { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F &&func) const -> std::enable_if_t<std::is_base_of<JobBase, decltype(func(KAsync::Error{}))>::value,
                                                  Job<typename decltype(func(KAsync::Error{}))::OutType, In...>>
    {
        using ResultJob = decltype(func(KAsync::Error{}));
        return thenImpl<typename ResultJob::OutType>(
                {JobErrorContinuation<typename ResultJob::OutType>(std::forward<F>(func))}, Private::ExecutionFlag::Always);
    }

    ///Sync continuation: [] (Arg) -> void { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F &&func) const -> std::enable_if_t<!std::is_base_of<JobBase, decltype(func(std::declval<Out>()))>::value,
                                                  Job<decltype(func(std::declval<Out>())), In...>>
    {
        using ResultType = decltype(func(std::declval<Out>())); //QString
        return thenImpl<ResultType, Out>(
                {SyncContinuation<ResultType, Out>(std::forward<F>(func))}, Private::ExecutionFlag::GoodCase);
    }

    ///Sync void continuation: [] () -> void { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F &&func) const -> std::enable_if_t<!std::is_base_of<JobBase, decltype(func())>::value,
                                                  Job<decltype(func()), In...>>
    {
        using ResultType = decltype(func()); //QString
        return thenImpl<ResultType>(
                {SyncContinuation<ResultType>(std::forward<F>(func))}, Private::ExecutionFlag::GoodCase);
    }

    ///Sync error continuation: [] (KAsync::Error, Arg) -> void { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F &&func) const -> std::enable_if_t<!std::is_base_of<JobBase, decltype(func(KAsync::Error{}, std::declval<Out>()))>::value,
                                                  Job<decltype(func(KAsync::Error{}, std::declval<Out>())),In...>>
    {
        using ResultType = decltype(func(KAsync::Error{}, std::declval<Out>())); //QString
        return thenImpl<ResultType, Out>(
                {SyncErrorContinuation<ResultType, Out>(std::forward<F>(func))}, Private::ExecutionFlag::Always);
    }

    ///Sync void error continuation: [] (KAsync::Error) -> void { ... }
    template<typename OutOther = void, typename ... InOther, typename F>
    auto then(F &&func) const -> std::enable_if_t<!std::is_base_of<JobBase, decltype(func(KAsync::Error{}))>::value,
                                                  Job<decltype(func(KAsync::Error{})), In...>>
    {
        using ResultType = decltype(func(KAsync::Error{}));
        return thenImpl<ResultType>(
                {SyncErrorContinuation<ResultType>(std::forward<F>(func))}, Private::ExecutionFlag::Always);
    }

    ///Shorthand for a job that receives the error and a handle
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(AsyncContinuation<OutOther, InOther ...> &&func) const
    {
        return thenImpl<OutOther, InOther ...>({std::forward<AsyncContinuation<OutOther, InOther ...>>(func)},
                                               Private::ExecutionFlag::GoodCase);
    }

    ///Shorthand for a job that receives the error and a handle
    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> then(AsyncErrorContinuation<OutOther, InOther ...> &&func) const
    {
        return thenImpl<OutOther, InOther ...>({std::forward<AsyncErrorContinuation<OutOther, InOther ...>>(func)}, Private::ExecutionFlag::Always);
    }

    ///Shorthand for a job that receives the error only
    Job<Out, In ...> onError(SyncErrorContinuation<void> &&errorFunc) const;

    /**
     * Shorthand for a forEach loop that automatically uses the return type of
     * this job to deduce the type expected.
     */
    template<typename OutOther = void, typename ListType = Out, typename ValueType = typename ListType::value_type, std::enable_if_t<!std::is_void<ListType>::value, int> = 0>
    Job<void, In ...> each(JobContinuation<void, ValueType> &&func) const
    {
        eachInvariants<OutOther>();
        return then<void, In ...>(forEach<Out, ValueType>(std::forward<JobContinuation<void, ValueType>>(func)));
    }

    /**
     * Shorthand for a serialForEach loop that automatically uses the return type
     * of this job to deduce the type expected.
     */
    template<typename OutOther = void, typename ListType = Out, typename ValueType = typename ListType::value_type, std::enable_if_t<!std::is_void<ListType>::value, int> = 0>
    Job<void, In ...> serialEach(JobContinuation<void, ValueType> &&func) const
    {
        eachInvariants<OutOther>();
        return then<void, In ...>(serialForEach<Out, ValueType>(std::forward<JobContinuation<void, ValueType>>(func)));
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
    operator std::conditional_t<std::is_void<OutType>::value, IncompleteType, Job<void>>();

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
     * Adds a guard.
     * It is guaranteed that no callback is executed after the guard vanishes.
     *
     * Use this i.e. ensure you don't call-back into an already destroyed object.
     */
    Job<Out, In ...> &guard(const QObject *o)
    {
        assert(mExecutor);
        mExecutor->guard(o);
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

    explicit Job(JobContinuation<Out, In ...> &&func);
    explicit Job(AsyncContinuation<Out, In ...> &&func);

private:
    //@cond PRIVATE
    explicit Job(Private::ExecutorBasePtr executor);

    template<typename OutOther, typename ... InOther>
    Job<OutOther, In ...> thenImpl(Private::ContinuationHolder<OutOther, InOther ...> helper,
                                   Private::ExecutionFlag execFlag = Private::ExecutionFlag::GoodCase) const;

    template<typename InOther, typename ... InOtherTail>
    void thenInvariants() const;

    //Base case for an empty parameter pack
    template<typename ... InOther>
    auto thenInvariants() const -> std::enable_if_t<(sizeof...(InOther) == 0)>;

    template<typename OutOther>
    void eachInvariants() const;
    //@endcond
};

} // namespace KAsync


// out-of-line definitions of Job methods
#include "job_impl.h"

#endif // KASYNC_H
