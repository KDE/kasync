/*
 * Copyright 2014 - 2015 Daniel Vrátil <dvratil@redhat.com>
 * Copyright 2015 - 2016 Daniel Vrátil <dvratil@kde.org>
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

#ifndef KASYNC_JOB_IMPL_H
#define KASYNC_JOB_IMPL_H

#include "async.h"

#include <QTimer>

//@cond PRIVATE

namespace KAsync
{

template<typename Out, typename ... In>
template<typename ... InOther>
Job<Out, In ...>::operator std::conditional_t<std::is_void<OutType>::value, IncompleteType, Job<void>> ()
{
    return thenImpl<void, InOther ...>({JobContinuation<void, InOther ...>([](InOther ...){ return KAsync::null<void>(); })}, {});
}

template<typename Out, typename ... In>
template<typename OutOther, typename ... InOther>
Job<OutOther, In ...> Job<Out, In ...>::thenImpl(Private::ContinuationHolder<OutOther, InOther ...> workHelper,
                                                 Private::ExecutionFlag execFlag) const
{
    thenInvariants<InOther ...>();
    return Job<OutOther, In ...>(QSharedPointer<Private::Executor<OutOther, InOther ...>>::create(
                std::forward<Private::ContinuationHolder<OutOther, InOther ...>>(workHelper), mExecutor, execFlag));
}

template<typename Out, typename ... In>
template<typename OutOther, typename ... InOther>
Job<OutOther, In ...> Job<Out, In ...>::then(const Job<OutOther, InOther ...> &job) const
{
    thenInvariants<InOther ...>();
    auto executor = job.mExecutor;
    executor->prepend(mExecutor);
    return Job<OutOther, In ...>(executor);
}

template<typename Out, typename ... In>
Job<Out, In ...> Job<Out, In ...>::onError(SyncErrorContinuation<void> &&errorFunc) const
{
    return Job<Out, In...>(QSharedPointer<Private::Executor<Out, Out>>::create(
                // Extra indirection to allow propagating the result of a previous future when no
                // error occurs
                Private::ContinuationHolder<Out, Out>([errorFunc = std::move(errorFunc)](const Error &error, const Out &val) {
                    errorFunc(error);
                    return val;
                }), mExecutor, Private::ExecutionFlag::ErrorCase));
}

template<> // Specialize for void jobs
inline Job<void> Job<void>::onError(SyncErrorContinuation<void> &&errorFunc) const
{
    return Job<void>(QSharedPointer<Private::Executor<void>>::create(
                Private::ContinuationHolder<void>(std::forward<SyncErrorContinuation<void>>(errorFunc)),
                mExecutor, Private::ExecutionFlag::ErrorCase));
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

    first->mPrev = QSharedPointer<Private::Executor<FirstIn>>::create(
            Private::ContinuationHolder<FirstIn>([val = std::move(in)](Future<FirstIn> &future) {
                 future.setResult(val);
            }));

    auto result = exec();
    // Remove the injected executor
    first->mPrev.reset();
    return result;
}

template<typename Out, typename ... In>
KAsync::Future<Out> Job<Out, In ...>::exec()
{
    Private::ExecutionPtr execution = mExecutor->exec(mExecutor, Private::ExecutionContext::Ptr::create());
    KAsync::Future<Out> result = *execution->result<Out>();

    return result;
}

template<typename Out, typename ... In>
Job<Out, In ...>::Job(Private::ExecutorBasePtr executor)
    : JobBase(executor)
{}

template<typename Out, typename ... In>
Job<Out, In ...>::Job(JobContinuation<Out, In ...> &&func)
    : JobBase(new Private::Executor<Out, In ...>(std::forward<JobContinuation<Out, In ...>>(func), {}))
{
    qWarning() << "Creating job job";
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
}

template<typename Out, typename ... In>
template<typename OutOther>
void Job<Out, In ...>::eachInvariants() const
{
    static_assert(detail::isIterable<Out>::value,
                    "The 'Each' task can only be connected to a job that returns a list or an array.");
    static_assert(std::is_void<OutOther>::value || detail::isIterable<OutOther>::value,
                    "The result type of 'Each' task must be void, a list or an array.");
}

template<typename Out, typename ... In>
template<typename InOtherFirst, typename ... InOtherTail>
void Job<Out, In ...>::thenInvariants() const
{
    static_assert(!std::is_void<Out>::value && (std::is_convertible<Out, InOtherFirst>::value || std::is_base_of<Out, InOtherFirst>::value),
                    "The return type of previous task must be compatible with input type of this task");
}

template<typename Out, typename ... In>
template<typename ... InOther>
auto Job<Out, In ...>::thenInvariants() const -> std::enable_if_t<(sizeof...(InOther) == 0)>
{
}

inline KAsync::Job<void> waitForCompletion(QVector<KAsync::Future<void>> &futures)
{
    struct Context {
        void removeWatcher(KAsync::FutureWatcher<void> *w)
        {
            pending.erase(std::remove_if(pending.begin(), pending.end(), [w](const auto &watcher) {
                return w == watcher.get();
            }));
        }

        std::vector<std::unique_ptr<KAsync::FutureWatcher<void>>> pending;
    };

    return start<Context *>([]() {
            return new Context();
        })
        .then<Context*, Context*>([futures](Context *context, KAsync::Future<Context *> &future) {
            for (KAsync::Future<void> subFuture : futures) {
                if (subFuture.isFinished()) {
                    continue;
                }
                // FIXME bind lifetime all watcher to future (repectively the main job
                auto watcher = std::make_unique<KAsync::FutureWatcher<void>>();
                QObject::connect(watcher.get(), &KAsync::FutureWatcher<void>::futureReady,
                                 [&future, watcher = watcher.get(), context]() {
                                    context->removeWatcher(watcher);
                                    if (context->pending.empty()) {
                                        future.setResult(context);
                                    }
                                });
                watcher->setFuture(subFuture);
                context->pending.push_back(std::move(watcher));
            }
            if (context->pending.empty()) {
                future.setResult(context);
            }
        })
        .then<void, Context*>([](Context *context) {
            delete context;
        });
        // .finally<void>([context]() { delete context; });
}

template<typename List, typename ValueType>
Job<void, List> forEach(KAsync::Job<void, ValueType> job)
{
    auto cont = [job] (const List &values) mutable {
            auto error = QSharedPointer<KAsync::Error>::create();
            QVector<KAsync::Future<void>> list;
            for (const auto &v : values) {
                auto future = job
                    .template then<void>([error] (const KAsync::Error &e) {
                        if (e && !*error) {
                            //TODO ideally we would aggregate the errors instead of just using the first one
                            *error = e;
                        }
                    })
                    .exec(v);
                list.push_back(future);
            }
            return waitForCompletion(list)
                .then<void>([error](KAsync::Future<void> &future) {
                    if (*error) {
                        future.setError(*error);
                    } else {
                        future.setFinished();
                    }
                });
        };
    return Job<void, List>(QSharedPointer<Private::Executor<void, List>>::create(
                Private::ContinuationHolder<void, List>(JobContinuation<void, List>(std::move(cont))), nullptr, Private::ExecutionFlag::GoodCase));
}


template<typename List, typename ValueType>
Job<void, List> serialForEach(KAsync::Job<void, ValueType> job)
{
    auto cont = [job] (const List &values) mutable {
            auto error = QSharedPointer<KAsync::Error>::create();
            auto serialJob = KAsync::null<void>();
            for (const auto &value : values) {
                serialJob = serialJob.then<void>([value, job, error](KAsync::Future<void> &future) {
                    job.template then<void>([&future, error] (const KAsync::Error &e) {
                        if (e && !*error) {
                            //TODO ideally we would aggregate the errors instead of just using the first one
                            *error = e;
                        }
                        future.setFinished();
                    })
                    .exec(value);
                });
            }
            return serialJob
                .then<void>([error](KAsync::Future<void> &future) {
                    if (*error) {
                        future.setError(*error);
                    } else {
                        future.setFinished();
                    }
                });
        };
    return Job<void, List>(QSharedPointer<Private::Executor<void, List>>::create(
            Private::ContinuationHolder<void, List>(JobContinuation<void, List>(std::move(cont))), nullptr, Private::ExecutionFlag::GoodCase));
}

template<typename List, typename ValueType>
Job<void, List> forEach(JobContinuation<void, ValueType> &&func)
{
    return forEach<List, ValueType>(KAsync::start<void, ValueType>(std::forward<JobContinuation<void, ValueType>>(func)));
}

template<typename List, typename ValueType>
Job<void, List> serialForEach(JobContinuation<void, ValueType> &&func)
{
    return serialForEach<List, ValueType>(KAsync::start<void, ValueType>(std::forward<JobContinuation<void, ValueType>>(func)));
}

template<typename Out>
Job<Out> null()
{
    return KAsync::start<Out>(
        [](KAsync::Future<Out> &future) {
            future.setFinished();
        });
}

template<typename Out>
Job<Out> value(Out v)
{
    return KAsync::start<Out>(
        [val = std::move(v)](KAsync::Future<Out> &future) {
            future.setResult(val);
        });
}

template<typename Out>
Job<Out> error(int errorCode, const QString &errorMessage)
{
    return error<Out>({errorCode, errorMessage});
}

template<typename Out>
Job<Out> error(const char *message)
{
    return error<Out>(Error(message));
}

template<typename Out>
Job<Out> error(const Error &error)
{
    return KAsync::start<Out>(
        [error](KAsync::Future<Out> &future) {
            future.setError(error);
        });
}

inline Job<void> doWhile(const Job<ControlFlowFlag> &body)
{
    return KAsync::start<void>([body] (KAsync::Future<void> &future) {
        auto job = body.then<void, ControlFlowFlag>([&future, body](const KAsync::Error &error, ControlFlowFlag flag) {
            if (error) {
                future.setError(error);
                future.setFinished();
            } else if (flag == ControlFlowFlag::Continue) {
                doWhile(body).then<void>([&future](const KAsync::Error &error) {
                    if (error) {
                        future.setError(error);
                    }
                    future.setFinished();
                }).exec();
            } else {
                future.setFinished();
            }
        }).exec();
    });
}

inline Job<void> doWhile(const JobContinuation<ControlFlowFlag> &body)
{
    return doWhile(KAsync::start<ControlFlowFlag>([body] {
        return body();
    }));
}

inline Job<void> wait(int delay)
{
    return KAsync::start<void>([delay](KAsync::Future<void> &future) {
        QTimer::singleShot(delay, [&future]() {
            future.setFinished();
        });
    });
}
} // namespace KAsync

//@endcond

#endif // KASYNC_JOB_IMPL_H
