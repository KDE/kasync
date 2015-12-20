/*
 * Copyright 2014 - 2015 Daniel Vrátil <dvratil@redhat.com>
 * Copyright 2015        Daniel Vrátil <dvratil@kde.org>
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

#ifndef KASYNC_JOB_IMPL_H
#define KASYNC_JOB_IMPL_H

#include "async.h"

//@cond PRIVATE

namespace KAsync
{

template<typename Out, typename ... In>
template<typename OutOther, typename ... InOther>
Job<OutOther, InOther ...> Job<Out, In ...>::then(ThenTask<OutOther, InOther ...> func,
                                                  ErrorHandler errorFunc)
{
    return Job<OutOther, InOther ...>(Private::ExecutorBasePtr(
        new Private::ThenExecutor<OutOther, InOther ...>(func, errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename T, typename OutOther, typename ... InOther>
Job<OutOther, InOther ...> Job<Out, In ...>::then(T *object,
                                                  typename detail::funcHelper<T, OutOther, InOther ...>::type func,
                                                  ErrorHandler errorFunc)
{
    return Job<OutOther, InOther ...>(Private::ExecutorBasePtr(
        new Private::ThenExecutor<OutOther, InOther ...>(
            memberFuncWrapper<ThenTask<OutOther, InOther ...>, T, OutOther, InOther ...>(object, func),
            errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename OutOther, typename ... InOther>
Job<OutOther, InOther ...> Job<Out, In ...>::then(SyncThenTask<OutOther, InOther ...> func,
                                                  ErrorHandler errorFunc)
{
    return Job<OutOther, InOther ...>(Private::ExecutorBasePtr(
        new Private::SyncThenExecutor<OutOther, InOther ...>(func, errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename T, typename OutOther, typename ... InOther>
Job<OutOther, InOther ...> Job<Out, In ...>::then(T *object,
                                                  typename detail::syncFuncHelper<T, OutOther, InOther ...>::type func,
                                                  ErrorHandler errorFunc)
{
    return Job<OutOther, InOther ...>(Private::ExecutorBasePtr(
        new Private::SyncThenExecutor<OutOther, InOther ...>(
            memberFuncWrapper<SyncThenTask<OutOther, InOther ...>, T, OutOther, InOther ...>(object, func),
            errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename OutOther, typename ... InOther>
Job<OutOther, InOther ...> Job<Out, In ...>::then(Job<OutOther, InOther ...> otherJob, ErrorHandler errorFunc)
{
    return then<OutOther, InOther ...>(nestedJobWrapper<OutOther, InOther ...>(otherJob), errorFunc);
}

template<typename Out, typename ... In>
template<typename ReturnType, typename KJobType, ReturnType (KJobType::*KJobResultMethod)(), typename ... Args>
typename std::enable_if<std::is_base_of<KJob, KJobType>::value, Job<ReturnType, Args ...>>::type
Job<Out, In ...>::then()
{
    return start<ReturnType, KJobType, KJobResultMethod, Args ...>();
}

template<typename Out, typename ... In>
template<typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::each(EachTask<OutOther, InOther> func,
                                              ErrorHandler errorFunc)
{
    eachInvariants<OutOther>();
    return Job<OutOther, InOther>(Private::ExecutorBasePtr(
        new Private::EachExecutor<Out, OutOther, InOther>(func, errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename T, typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::each(T *object, MemberEachTask<T, OutOther, InOther> func,
                                              ErrorHandler errorFunc)
{
    eachInvariants<OutOther>();
    return Job<OutOther, InOther>(Private::ExecutorBasePtr(
        new Private::EachExecutor<Out, OutOther, InOther>(
            memberFuncWrapper<EachTask<OutOther, InOther>, T, OutOther, InOther>(object, func),
            errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::each(SyncEachTask<OutOther, InOther> func,
                                              ErrorHandler errorFunc)
{
    eachInvariants<OutOther>();
    return Job<OutOther, InOther>(Private::ExecutorBasePtr(
        new Private::SyncEachExecutor<Out, OutOther, InOther>(func, errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename T, typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::each(T *object,
                                              MemberSyncEachTask<T, OutOther, InOther> func,
                                              ErrorHandler errorFunc)
{
    eachInvariants<OutOther>();
    return Job<OutOther, InOther>(Private::ExecutorBasePtr(
        new Private::SyncEachExecutor<Out, OutOther, InOther>(
            memberFuncWrapper<SyncEachTask<OutOther, InOther>, T, OutOther, InOther>(object, func),
            errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::each(Job<OutOther, InOther> otherJob,
                                              ErrorHandler errorFunc)
{
    eachInvariants<OutOther>();
    return each<OutOther, InOther>(nestedJobWrapper<OutOther, InOther>(otherJob), errorFunc);
}

template<typename Out, typename ... In>
template<typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::reduce(ReduceTask<OutOther, InOther> func,
                                                ErrorHandler errorFunc)
{
    reduceInvariants<InOther>();
    return Job<OutOther, InOther>(Private::ExecutorBasePtr(
        new Private::ReduceExecutor<OutOther, InOther>(func, errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename T, typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::reduce(T *object,
                                                MemberReduceTask<T, OutOther, InOther> func,
                                                ErrorHandler errorFunc)
{
    reduceInvariants<InOther>();
    return Job<OutOther, InOther>(Private::ExecutorBasePtr(
        new Private::ReduceExecutor<OutOther, InOther>(
            memberFuncWrapper<ReduceTask<OutOther, InOther>, T, OutOther, InOther>(object, func),
            errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::reduce(SyncReduceTask<OutOther, InOther> func,
                                                ErrorHandler errorFunc)
{
    reduceInvariants<InOther>();
    return Job<OutOther, InOther>(Private::ExecutorBasePtr(
        new Private::SyncReduceExecutor<OutOther, InOther>(func, errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename T, typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::reduce(T *object,
                                                MemberSyncReduceTask<T, OutOther, InOther> func,
                                                ErrorHandler errorFunc)
{
    reduceInvariants<InOther>();
    return Job<OutOther, InOther>(Private::ExecutorBasePtr(
        new Private::ReduceExecutor<OutOther, InOther>(
            memberFuncWrapper<SyncReduceTask<OutOther, InOther>, T, OutOther, InOther>(object, func),
            errorFunc, mExecutor)));
}

template<typename Out, typename ... In>
template<typename OutOther, typename InOther>
Job<OutOther, InOther> Job<Out, In ...>::reduce(Job<OutOther, InOther> otherJob,
                                                ErrorHandler errorFunc)
{
    return reduce<OutOther, InOther>(nestedJobWrapper<OutOther, InOther>(otherJob), errorFunc);
}

template<typename Out, typename ... In>
Job<Out, Out> Job<Out, In ...>::error(ErrorHandler errorFunc)
{
    return Job<Out, Out>(Private::ExecutorBasePtr(
        new Private::SyncThenExecutor<Out, Out>(
            [](const Out &in) -> Out {
                return in;
            },
            errorFunc, mExecutor)));
}




template<typename Out, typename ... In>
template<typename FirstIn>
KAsync::Future<Out> Job<Out, In ...>::exec(FirstIn in)
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

template<typename Out, typename ... In>
KAsync::Future<Out> Job<Out, In ...>::exec()
{
    Private::ExecutionPtr execution = mExecutor->exec(mExecutor);
    KAsync::Future<Out> result = *execution->result<Out>();

    return result;
}

template<typename Out, typename ... In>
Job<Out, In ...>::Job(Private::ExecutorBasePtr executor)
    : JobBase(executor)
{}

template<typename Out, typename ... In>
template<typename OutOther>
void Job<Out, In ...>::eachInvariants()
{
    static_assert(detail::isIterable<Out>::value,
                    "The 'Each' task can only be connected to a job that returns a list or an array.");
    static_assert(std::is_void<OutOther>::value || detail::isIterable<OutOther>::value,
                    "The result type of 'Each' task must be void, a list or an array.");
}

template<typename Out, typename ... In>
template<typename InOther>
void Job<Out, In ...>::reduceInvariants()
{
    static_assert(KAsync::detail::isIterable<Out>::value,
                    "The 'Result' task can only be connected to a job that returns a list or an array");
    static_assert(std::is_same<typename Out::value_type, typename InOther::value_type>::value,
                    "The return type of previous task must be compatible with input type of this task");
}

template<typename Out, typename ... In>
template<typename OutOther, typename ... InOther>
inline std::function<void(InOther ..., KAsync::Future<OutOther>&)>
Job<Out, In ...>::nestedJobWrapper(Job<OutOther, InOther ...> otherJob)
{
    return [otherJob](InOther ... in, KAsync::Future<OutOther> &future) {
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
                                KAsync::detail::copyFutureValue(watcher->future(), outFuture);
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

template<typename Out, typename ... In>
template<typename Task, typename T, typename OutOther, typename ... InOther>
inline Task Job<Out, In ...>::memberFuncWrapper(T *object,
                                                typename detail::funcHelper<T, OutOther, InOther ...>::type func)
{
    return [object, func](InOther && ... inArgs, KAsync::Future<OutOther> &future) -> void
        {
            (object->*func)(std::forward<InOther>(inArgs) ..., future);
        };
}

template<typename Out, typename ... In>
template<typename Task, typename T, typename OutOther, typename ... InOther>
inline typename std::enable_if<!std::is_void<OutOther>::value, Task>::type
Job<Out, In ...>::memberFuncWrapper(T *object, typename detail::syncFuncHelper<T, OutOther, InOther ...>::type func)
{
    return [object, func](InOther && ... inArgs) -> OutOther
        {
            return (object->*func)(std::forward<InOther>(inArgs) ...);
        };
}

template<typename Out, typename ... In>
template<typename Task, typename T, typename OutOther, typename ... InOther>
inline typename std::enable_if<std::is_void<OutOther>::value, Task>::type
Job<Out, In ...>::memberFuncWrapper(T *object, typename detail::syncFuncHelper<T, void, InOther ...>::type func, int *)
{
    return [object, func](InOther && ... inArgs) -> void
        {
            (object->*func)(std::forward<InOther>(inArgs) ...);
        };
}

} // namespace KAsync

//@endconf

#endif // KASYNC_JOB_IMPL_H
