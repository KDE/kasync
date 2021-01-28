/*
    SPDX-FileCopyrightText: 2014-2015 Daniel Vrátil <dvratil@redhat.com>
    SPDX-FileCopyrightText: 2015-2019 Daniel Vrátil <dvratil@kde.org>
    SPDX-FileCopyrightText: 2016 Christian Mollekopf <mollekopf@kolabsystems.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KASYNC_EXECUTOR_P_H
#define KASYNC_EXECUTOR_P_H

#include "execution_p.h"
#include "continuations_p.h"
#include "debug.h"

namespace KAsync {

template<typename T>
class Future;

template<typename T>
class FutureWatcher;

template<typename Out, typename ... In>
class ContinuationHolder;

template<typename Out, typename ... In>
class Job;

class Tracer;

namespace Private {

class ExecutorBase;
using ExecutorBasePtr = QSharedPointer<ExecutorBase>;

class ExecutorBase
{
    template<typename Out, typename ... In>
    friend class Executor;

    template<typename Out, typename ... In>
    friend class KAsync::Job;

    friend struct Execution;
    friend class KAsync::Tracer;

public:
    virtual ~ExecutorBase() = default;

    virtual ExecutionPtr exec(const ExecutorBasePtr &self, QSharedPointer<Private::ExecutionContext> context) = 0;

protected:
    ExecutorBase(const ExecutorBasePtr &parent)
        : mPrev(parent)
    {}

    template<typename T>
    KAsync::Future<T>* createFuture(const ExecutionPtr &execution) const
    {
        return new KAsync::Future<T>(execution);
    }

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
        mContext.push_back(entry);
    }

    void guard(const QObject *o)
    {
        mGuards.push_back(QPointer<const QObject>{o});
    }

    QString mExecutorName;
    QVector<QVariant> mContext;
    QVector<QPointer<const QObject>> mGuards;
    ExecutorBasePtr mPrev;
};

template<typename Out, typename ... In>
class Executor : public ExecutorBase
{
    using PrevOut = std::tuple_element_t<0, std::tuple<In ..., void>>;

public:
    explicit Executor(ContinuationHolder<Out, In ...> &&workerHelper, const ExecutorBasePtr &parent = {},
                      ExecutionFlag executionFlag = ExecutionFlag::GoodCase)
        : ExecutorBase(parent)
        , mContinuationHolder(std::move(workerHelper))
        , executionFlag(executionFlag)
    {
        STORE_EXECUTOR_NAME("Executor", Out, In ...);
    }

    virtual ~Executor() = default;

    void run(const ExecutionPtr &execution)
    {
        KAsync::Future<PrevOut> *prevFuture = nullptr;
        if (execution->prevExecution) {
            prevFuture = execution->prevExecution->result<PrevOut>();
            assert(prevFuture->isFinished());
        }

        //Execute one of the available workers
        KAsync::Future<Out> *future = execution->result<Out>();

        const auto &continuation = Executor<Out, In ...>::mContinuationHolder;
        if (continuationIs<AsyncContinuation<Out, In ...>>(continuation)) {
            continuationGet<AsyncContinuation<Out, In ...>>(continuation)(prevFuture ? prevFuture->value() : In() ..., *future);
        } else if (continuationIs<AsyncErrorContinuation<Out, In ...>>(continuation)) {
            continuationGet<AsyncErrorContinuation<Out, In ...>>(continuation)(
                    prevFuture->hasError() ? prevFuture->errors().first() : Error(),
                    prevFuture ? prevFuture->value() : In() ..., *future);
        } else if (continuationIs<SyncContinuation<Out, In ...>>(continuation)) {
            callAndApply(prevFuture ? prevFuture->value() : In() ...,
                         continuationGet<SyncContinuation<Out, In ...>>(continuation), *future, std::is_void<Out>());
            future->setFinished();
        } else if (continuationIs<SyncErrorContinuation<Out, In ...>>(continuation)) {
            assert(prevFuture);
            callAndApply(prevFuture->hasError() ? prevFuture->errors().first() : Error(),
                         prevFuture ? prevFuture->value() : In() ...,
                         continuationGet<SyncErrorContinuation<Out, In ...>>(continuation), *future, std::is_void<Out>());
            future->setFinished();
        } else if (continuationIs<JobContinuation<Out, In ...>>(continuation)) {
            executeJobAndApply(prevFuture ? prevFuture->value() : In() ...,
                               continuationGet<JobContinuation<Out, In ...>>(continuation), *future, std::is_void<Out>());
        } else if (continuationIs<JobErrorContinuation<Out, In ...>>(continuation)) {
            executeJobAndApply(prevFuture->hasError() ? prevFuture->errors().first() : Error(),
                               prevFuture ? prevFuture->value() : In() ...,
                               continuationGet<JobErrorContinuation<Out, In ...>>(continuation), *future, std::is_void<Out>());
        }

    }

    ExecutionPtr exec(const ExecutorBasePtr &self, QSharedPointer<Private::ExecutionContext> context) override
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

private:
    void runExecution(const KAsync::Future<PrevOut> *prevFuture, const ExecutionPtr &execution, bool guardIsBroken)
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
                copyFutureValue<PrevOut>(*prevFuture, *execution->result<PrevOut>());
                execution->resultBase->setFinished();
                return;
            }
        }
        run(execution);
    }

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

    void callAndApply(In && ... input, const SyncContinuation<Out, In ...> &func, Future<Out> &future, std::false_type)
    {
        future.setValue(func(std::forward<In>(input) ...));
    }

    void callAndApply(In && ... input, const SyncContinuation<Out, In ...> &func, Future<Out> &, std::true_type)
    {
        func(std::forward<In>(input) ...);
    }

    void callAndApply(const Error &error, In && ... input, const SyncErrorContinuation<Out, In ...> &func, Future<Out> &future, std::false_type)
    {
        future.setValue(func(error, std::forward<In>(input) ...));
    }

    void callAndApply(const Error &error, In && ... input, const SyncErrorContinuation<Out, In ...> &func, Future<Out> &, std::true_type)
    {
        func(error, std::forward<In>(input) ...);
    }

    template<typename T>
    std::enable_if_t<!std::is_void<T>::value>
    copyFutureValue(const KAsync::Future<T> &in, KAsync::Future<T> &out)
    {
        out.setValue(in.value());
    }

    template<typename T>
    std::enable_if_t<std::is_void<T>::value>
    copyFutureValue(const KAsync::Future<T> &, KAsync::Future<T> &)
    {
        //noop
    }
private:
    ContinuationHolder<Out, In ...> mContinuationHolder;
    const ExecutionFlag executionFlag;
};

} // namespace Private
} // nameapce KAsync

#endif

