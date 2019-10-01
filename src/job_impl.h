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

//@cond PRIVATE

namespace KAsync
{

namespace Private {

template<typename Out, typename ... In>
class ThenExecutor: public Executor<typename detail::prevOut<In ...>::type, Out, In ...>
{
public:
    ThenExecutor(ContinuationHelper<Out, In ...> &&workerHelper, const ExecutorBasePtr &parent = {},
                 ExecutionFlag executionFlag = ExecutionFlag::GoodCase)
        : Executor<typename detail::prevOut<In ...>::type, Out, In ...>(parent, executionFlag)
        , mContinuationHelper(std::move(workerHelper))
    {
        STORE_EXECUTOR_NAME("ThenExecutor", Out, In ...);
    }

    void run(const ExecutionPtr &execution) Q_DECL_OVERRIDE
    {
        KAsync::Future<typename detail::prevOut<In ...>::type> *prevFuture = nullptr;
        if (execution->prevExecution) {
            prevFuture = execution->prevExecution->result<typename detail::prevOut<In ...>::type>();
            assert(prevFuture->isFinished());
        }

        //Execute one of the available workers
        KAsync::Future<Out> *future = execution->result<Out>();

        const auto &helper = ThenExecutor<Out, In ...>::mContinuationHelper;
        if (helper.handleContinuation) {
            helper.handleContinuation(prevFuture ? prevFuture->value() : In() ..., *future);
        } else if (helper.handleErrorContinuation) {
            helper.handleErrorContinuation(prevFuture->hasError() ? prevFuture->errors().first() : Error(),
                                           prevFuture ? prevFuture->value() : In() ..., *future);
        } else if (helper.jobContinuation) {
            executeJobAndApply(prevFuture ? prevFuture->value() : In() ...,
                               helper.jobContinuation, *future, std::is_void<Out>());
        } else if (helper.jobErrorContinuation) {
            executeJobAndApply(prevFuture->hasError() ? prevFuture->errors().first() : Error(),
                               prevFuture ? prevFuture->value() : In() ...,
                               helper.jobErrorContinuation, *future, std::is_void<Out>());
        }
    }

private:

    void executeJobAndApply(In && ... input, const JobContinuation<Out, In ...> &func,
                            Future<Out> &future, std::false_type)
    {
        func(std::forward<In>(input) ...)
            .template then<void, Out>([&future](const KAsync::Error &error, const Out &v,
                                                KAsync::Future<void> &f) {
                if (error) {
                    future.setError(error);
                } else {
                    future.setResult(v);
                }
                f.setFinished();
            }).exec();
    }

    void executeJobAndApply(In && ... input, const JobContinuation<Out, In ...> &func,
                            Future<Out> &future, std::true_type)
    {
        func(std::forward<In>(input) ...)
            .template then<void>([&future](const KAsync::Error &error, KAsync::Future<void> &f) {
                if (error) {
                    future.setError(error);
                } else {
                    future.setFinished();
                }
                f.setFinished();
            }).exec();
    }

    void executeJobAndApply(const Error &error, In && ... input, const JobErrorContinuation<Out, In ...> &func,
                            Future<Out> &future, std::false_type)
    {
        func(error, std::forward<In>(input) ...)
            .template then<void, Out>([&future](const KAsync::Error &error, const Out &v,
                                                KAsync::Future<void> &f) {
                if (error) {
                    future.setError(error);
                } else {
                    future.setResult(v);
                }
                f.setFinished();
            }).exec();
    }

    void executeJobAndApply(const Error &error, In && ... input, const JobErrorContinuation<Out, In ...> &func,
                            Future<Out> &future, std::true_type)
    {
        func(error, std::forward<In>(input) ...)
            .template then<void>([&future](const KAsync::Error &error, KAsync::Future<void> &f) {
                if (error) {
                    future.setError(error);
                } else {
                    future.setFinished();
                }
                f.setFinished();
            }).exec();
    }

    ContinuationHelper<Out, In ...> mContinuationHelper;
};


template<typename Out, typename ... In>
class SyncThenExecutor: public Executor<typename detail::prevOut<In ...>::type, Out, In ...>
{
private:

    void callAndApply(In && ... input, const SyncContinuation<Out, In ...> &func,
                      Future<Out> &future, std::false_type)
    {
        future.setValue(func(std::forward<In>(input) ...));
    }

    void callAndApply(In && ... input, const SyncContinuation<Out, In ...> &func,
                      Future<Out> &, std::true_type)
    {
        func(std::forward<In>(input) ...);
    }

    void callAndApply(const Error &error, In && ... input, const SyncErrorContinuation<Out, In ...> &func,
                      Future<Out> &future, std::false_type)
    {
        future.setValue(func(error, std::forward<In>(input) ...));
    }

    void callAndApply(const Error &error, In && ... input, const SyncErrorContinuation<Out, In ...> &func,
                      Future<Out> &, std::true_type)
    {
        func(error, std::forward<In>(input) ...);
    }

    const SyncContinuation<Out, In ...> mContinuation;
    const SyncErrorContinuation<Out, In ...> mErrorContinuation;

public:
    SyncThenExecutor(SyncContinuation<Out, In ...> &&worker,
                     const ExecutorBasePtr &parent = ExecutorBasePtr(),
                     ExecutionFlag executionFlag = Always)
        : Executor<typename detail::prevOut<In ...>::type, Out, In ...>(parent, executionFlag)
        , mContinuation(std::move(worker))
    {

    }

    SyncThenExecutor(SyncErrorContinuation<Out, In ...> &&worker,
                     const ExecutorBasePtr &parent = ExecutorBasePtr(),
                     ExecutionFlag executionFlag = Always)
        : Executor<typename detail::prevOut<In ...>::type, Out, In ...>(parent, executionFlag)
        , mErrorContinuation(std::move(worker))
    {

    }

    void run(const ExecutionPtr &execution) Q_DECL_OVERRIDE
    {
        KAsync::Future<typename detail::prevOut<In ...>::type> *prevFuture = nullptr;
        if (execution->prevExecution) {
            prevFuture = execution->prevExecution->result<typename detail::prevOut<In ...>::type>();
            assert(prevFuture->isFinished());
        }

        KAsync::Future<Out> *future = execution->result<Out>();
        if (SyncThenExecutor<Out, In ...>::mContinuation) {
            callAndApply(prevFuture ? prevFuture->value() : In() ...,
                        SyncThenExecutor<Out, In ...>::mContinuation,
                        *future, std::is_void<Out>());
        }

        if (SyncThenExecutor<Out, In ...>::mErrorContinuation) {
            assert(prevFuture);
            callAndApply(prevFuture->hasError() ? prevFuture->errors().first() : Error(),
                        prevFuture ? prevFuture->value() : In() ...,
                        SyncThenExecutor<Out, In ...>::mErrorContinuation,
                        *future, std::is_void<Out>());
        }
        future->setFinished();
    }
};

template<typename Out, typename ... In>
class SyncErrorExecutor: public Executor<typename detail::prevOut<In ...>::type, Out, In ...>
{
private:
    const SyncErrorContinuation<void> mContinuation;

public:
    SyncErrorExecutor(SyncErrorContinuation<void> &&worker,
                      const ExecutorBasePtr &parent = ExecutorBasePtr(),
                      ExecutionFlag executionFlag = Always)
        : Executor<typename detail::prevOut<In ...>::type, Out, In ...>(parent, executionFlag)
        , mContinuation(std::move(worker))
    {

    }

    void run(const ExecutionPtr &execution) Q_DECL_OVERRIDE
    {
        KAsync::Future<typename detail::prevOut<In ...>::type> *prevFuture = nullptr;
        if (execution->prevExecution) {
            prevFuture = execution->prevExecution->result<typename detail::prevOut<In ...>::type>();
            assert(prevFuture->isFinished());
        }

        KAsync::Future<Out> *future = execution->result<Out>();
        assert(prevFuture->hasError());
        mContinuation(prevFuture->errors().first());
        future->setError(prevFuture->errors().first());
    }
};

template<typename T>
KAsync::Future<T>* ExecutorBase::createFuture(const ExecutionPtr &execution) const
{
    return new KAsync::Future<T>(execution);
}

template<typename PrevOut, typename Out, typename ... In>
void Executor<PrevOut, Out, In ...>::runExecution(const KAsync::Future<PrevOut> *prevFuture,
                                                  const ExecutionPtr &execution, bool guardIsBroken)
{
    if (guardIsBroken) {
        execution->resultBase->setFinished();
        return;
    }
    if (prevFuture) {
        if (prevFuture->hasError() && executionFlag == ExecutionFlag::GoodCase) {
            //Propagate the error to the outer Future
            Q_ASSERT(prevFuture->errors().size() == 1);
            execution->resultBase->setError(prevFuture->errors().first());
            return;
        }
        if (!prevFuture->hasError() && executionFlag == ExecutionFlag::ErrorCase) {
            //Propagate the value to the outer Future
            KAsync::detail::copyFutureValue<PrevOut>(*prevFuture, *execution->result<PrevOut>());
            execution->resultBase->setFinished();
            return;
        }
    }
    run(execution);
}

class ExecutionContext {
public:
    typedef QSharedPointer<ExecutionContext> Ptr;

    QVector<QPointer<const QObject>> guards;
    bool guardIsBroken() const
    {
        for (const auto &g : guards) {
            if (!g) {
                return true;
            }
        }
        return false;
    }
};

template<typename PrevOut, typename Out, typename ... In>
ExecutionPtr Executor<PrevOut, Out, In ...>::exec(const ExecutorBasePtr &self, ExecutionContext::Ptr context)
{
    /*
     * One executor per job, created with the construction of the Job object.
     * One execution per job per exec(), created only once exec() is called.
     *
     * The executors make up the linked list that makes up the complete execution chain.
     *
     * The execution then tracks the execution of each executor.
     */

    // Passing 'self' to execution ensures that the Executor chain remains
    // valid until the entire execution is finished
    ExecutionPtr execution = ExecutionPtr::create(self);
#ifndef QT_NO_DEBUG
    execution->tracer = std::make_unique<Tracer>(execution.data()); // owned by execution
#endif

    context->guards += mGuards;

    // chainup
    execution->prevExecution = mPrev ? mPrev->exec(mPrev, context) : ExecutionPtr();

    execution->resultBase = ExecutorBase::createFuture<Out>(execution);
    //We watch our own future to finish the execution once we're done
    auto fw = new KAsync::FutureWatcher<Out>();
    QObject::connect(fw, &KAsync::FutureWatcher<Out>::futureReady,
                     [fw, execution]() {
                         execution->setFinished();
                         delete fw;
                     });
    fw->setFuture(*execution->result<Out>());

    KAsync::Future<PrevOut> *prevFuture = execution->prevExecution ? execution->prevExecution->result<PrevOut>()
                                                                   : nullptr;
    if (!prevFuture || prevFuture->isFinished()) { //The previous job is already done
        runExecution(prevFuture, execution, context->guardIsBroken());
    } else { //The previous job is still running and we have to wait for it's completion
        auto prevFutureWatcher = new KAsync::FutureWatcher<PrevOut>();
        QObject::connect(prevFutureWatcher, &KAsync::FutureWatcher<PrevOut>::futureReady,
                         [prevFutureWatcher, execution, this, context]() {
                             auto prevFuture = prevFutureWatcher->future();
                             assert(prevFuture.isFinished());
                             delete prevFutureWatcher;
                             runExecution(&prevFuture, execution, context->guardIsBroken());
                         });

        prevFutureWatcher->setFuture(*static_cast<KAsync::Future<PrevOut>*>(prevFuture));
    }

    return execution;
}

} // namespace Private


template<typename Out, typename ... In>
template<typename ... InOther>
Job<Out, In ...>::operator std::conditional_t<std::is_void<OutType>::value, IncompleteType, Job<void>> ()
{
    return thenImpl<void, InOther ...>({[](InOther ...){ return KAsync::null<void>(); }}, {});
}

template<typename Out, typename ... In>
template<typename OutOther, typename ... InOther>
Job<OutOther, In ...> Job<Out, In ...>::thenImpl(Private::ContinuationHelper<OutOther, InOther ...> workHelper,
                                                 Private::ExecutionFlag execFlag) const
{
    thenInvariants<InOther ...>();
    return Job<OutOther, In ...>(QSharedPointer<Private::ThenExecutor<OutOther, InOther ...>>::create(
                std::forward<Private::ContinuationHelper<OutOther, InOther ...>>(workHelper), mExecutor, execFlag));
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
template<typename OutOther, typename ... InOther>
Job<OutOther, In ...> Job<Out, In ...>::syncThenImpl(SyncContinuation<OutOther, InOther ...> &&func,
                                                     Private::ExecutionFlag execFlag) const
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    thenInvariants<InOther ...>();
    return Job<OutOther, In...>(QSharedPointer<Private::SyncThenExecutor<OutOther, InOther ...>>::create(
                std::forward<SyncContinuation<OutOther, InOther ...>>(func), mExecutor, execFlag));
}

template<typename Out, typename ... In>
template<typename OutOther, typename ... InOther>
Job<OutOther, In ...> Job<Out, In ...>::syncThenImpl(SyncErrorContinuation<OutOther, InOther ...> &&func,
                                                     Private::ExecutionFlag execFlag) const
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    thenInvariants<InOther ...>();
    return Job<OutOther, In...>(QSharedPointer<Private::SyncThenExecutor<OutOther, InOther ...>>::create(
                std::forward<SyncErrorContinuation<OutOther, InOther ...>>(func), mExecutor, execFlag));
}

template<typename Out, typename ... In>
Job<Out, In ...> Job<Out, In ...>::onError(const SyncErrorContinuation<void> &errorFunc) const
{
    return Job<Out, In...>(QSharedPointer<Private::SyncErrorExecutor<Out, Out>>::create(
            [=](const Error &error) {
                errorFunc(error);
            }, mExecutor, Private::ExecutionFlag::ErrorCase));
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

    first->mPrev = QSharedPointer<Private::ThenExecutor<FirstIn>>::create(
            Private::ContinuationHelper<FirstIn>([val = std::move(in)](Future<FirstIn> &future) {
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
    : JobBase(new Private::ThenExecutor<Out, In ...>(std::forward<JobContinuation<Out, In ...>>(func), {}))
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


template<typename Out, typename ... In>
Job<Out, In ...> startImpl(Private::ContinuationHelper<Out, In ...> &&helper)
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return Job<Out, In...>(QSharedPointer<Private::ThenExecutor<Out, In ...>>::create(
                std::forward<Private::ContinuationHelper<Out, In...>>(helper), nullptr, Private::ExecutionFlag::GoodCase));
}

template<typename Out, typename ... In>
Job<Out, In ...> syncStartImpl(SyncContinuation<Out, In ...> &&func)
{
    static_assert(sizeof...(In) <= 1, "Only one or zero input parameters are allowed.");
    return Job<Out, In...>(QSharedPointer<Private::SyncThenExecutor<Out, In ...>>::create(
                std::forward<SyncContinuation<Out, In ...>>(func)));
}

static inline KAsync::Job<void> waitForCompletion(QList<KAsync::Future<void>> &futures)
{
    auto context = new QObject;
    return start<void>([futures, context](KAsync::Future<void> &future) {
            const auto total = futures.size();
            auto count = QSharedPointer<int>::create();
            int i = 0;
            for (KAsync::Future<void> subFuture : futures) {
                i++;
                if (subFuture.isFinished()) {
                    *count += 1;
                    continue;
                }
                // FIXME bind lifetime all watcher to future (repectively the main job
                auto watcher = QSharedPointer<KAsync::FutureWatcher<void>>::create();
                QObject::connect(watcher.data(), &KAsync::FutureWatcher<void>::futureReady,
                                 [count, total, &future, context]() {
                                    *count += 1;
                                    if (*count == total) {
                                        delete context;
                                        future.setFinished();
                                    }
                                });
                watcher->setFuture(subFuture);
                context->setProperty(QString::fromLatin1("future%1").arg(i).toLatin1().data(),
                                     QVariant::fromValue(watcher));
            }
            if (*count == total) {
                delete context;
                future.setFinished();
            }
        });
        // .finally<void>([context]() { delete context; });
}

template<typename List, typename ValueType>
Job<void, List> forEach(KAsync::Job<void, ValueType> job)
{
    auto cont = [job] (const List &values) mutable {
            auto error = QSharedPointer<KAsync::Error>::create();
            QList<KAsync::Future<void>> list;
            for (const auto &v : values) {
                auto future = job
                    .template then<void>([error] (const KAsync::Error &e) {
                        if (e && !*error) {
                            //TODO ideally we would aggregate the errors instead of just using the first one
                            *error = e;
                        }
                    })
                    .exec(v);
                list << future;
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
    return Job<void, List>(QSharedPointer<Private::ThenExecutor<void, List>>::create(
                Private::ContinuationHelper<void, List>(std::move(cont)), nullptr, Private::ExecutionFlag::GoodCase));
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
    return Job<void, List>(QSharedPointer<Private::ThenExecutor<void, List>>::create(
            Private::ContinuationHelper<void, List>(std::move(cont)), nullptr, Private::ExecutionFlag::GoodCase));
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


} // namespace KAsync

//@endconf

#endif // KASYNC_JOB_IMPL_H
