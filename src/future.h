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

#ifndef FUTURE_H
#define FUTURE_H

#include "kasync_export.h"

class QEventLoop;

#include <type_traits>

#include <QSharedDataPointer>
#include <QPointer>
#include <QVector>
#include <QEventLoop>

namespace KAsync {

//@cond PRIVATE

class FutureWatcherBase;
template<typename T>
class FutureWatcher;

namespace Private {
class Execution;
class ExecutorBase;

typedef QSharedPointer<Execution> ExecutionPtr;
} // namespace Private

class KASYNC_EXPORT FutureBase
{
    friend class KAsync::Private::Execution;
    friend class FutureWatcherBase;

public:
    virtual ~FutureBase();


    void setFinished();
    bool isFinished() const;

    void setError(int code = 1, const QString &message = QString());
    int errorCode() const;
    QString errorMessage() const;

    void setProgress(qreal progress);
    void setProgress(int processed, int total);

protected:
    class PrivateBase : public QSharedData
    {
    public:
        explicit PrivateBase(const KAsync::Private::ExecutionPtr &execution);
        virtual ~PrivateBase();

        void releaseExecution();

        bool finished;
        int errorCode;
        QString errorMessage;

        QVector<QPointer<FutureWatcherBase>> watchers;
    private:
        QWeakPointer<KAsync::Private::Execution> mExecution;
    };

    explicit FutureBase();
    explicit FutureBase(FutureBase::PrivateBase *dd);
    FutureBase(const FutureBase &other);

    void addWatcher(KAsync::FutureWatcherBase *watcher);
    void releaseExecution();

protected:
    QExplicitlySharedDataPointer<PrivateBase> d;
};

template<typename T>
class FutureWatcher;

template<typename T>
class Future;

template<typename T>
class FutureGeneric : public FutureBase
{
    friend class FutureWatcher<T>;

public:

    void waitForFinished() const
    {
        if (isFinished()) {
            return;
        }
        FutureWatcher<T> watcher;
        QEventLoop eventLoop;
        QObject::connect(&watcher, &KAsync::FutureWatcher<T>::futureReady,
                         &eventLoop, &QEventLoop::quit);
        watcher.setFuture(*static_cast<const KAsync::Future<T>*>(this));
        eventLoop.exec();
    }

protected:
    //@cond PRIVATE
    explicit FutureGeneric(const KAsync::Private::ExecutionPtr &execution)
        : FutureBase(new Private(execution))
    {}

    FutureGeneric(const FutureGeneric<T> &other)
        : FutureBase(other)
    {}

protected:
    class Private : public FutureBase::PrivateBase
    {
    public:
        Private(const KAsync::Private::ExecutionPtr &execution)
            : FutureBase::PrivateBase(execution)
        {}

        typename std::conditional<std::is_void<T>::value, int /* dummy */, T>::type
        value;
    };
};
//@endcond



/**
 * @ingroup Future
 *
 * @brief Future is a promise that is used by Job to deliver result
 * of an asynchronous execution.
 *
 * The Future is passed internally to each executed task, and the task can use
 * it to report its progress, result and notify when it is finished.
 *
 * Users use Future they receive from calling Job::exec() to get access
 * to the overall result of the execution. FutureWatcher&lt;T&gt; can be used
 * to wait for the Future to finish in non-blocking manner.
 *
 * @see Future<void>
 */
template<typename T>
class Future : public FutureGeneric<T>
{
    //@cond PRIVATE
    friend class KAsync::Private::ExecutorBase;

    template<typename T_>
    friend class KAsync::FutureWatcher;
    //@endcond
public:
    /**
     * @brief Constructor
     */
    explicit Future()
        : FutureGeneric<T>(KAsync::Private::ExecutionPtr())
    {}

    /**
     * @brief Copy constructor
     */
    Future(const Future<T> &other)
        : FutureGeneric<T>(other)
    {}

    /**
     * Set the result of the Future. This method is called by the task upon
     * calculating the result. After setting the value, the caller must also
     * call setFinished() to notify users that the result
     * is available.
     *
     * @warning This method must only be called by the tasks inside Job,
     * never by outside users.
     *
     * @param value The result value
     */
    void setValue(const T &value)
    {
        static_cast<typename FutureGeneric<T>::Private*>(this->d.data())->value = value;
    }

    /**
     * Retrieve the result of the Future. Calling this method when the future has
     * not yet finished (i.e. isFinished() returns false)
     * returns undefined result.
     */
    T value() const
    {
        return static_cast<typename FutureGeneric<T>::Private*>(this->d.data())->value;
    }

#ifdef ONLY_DOXYGEN
    /**
     * Will block until the Future has finished.
     *
     * @note Internally this method is using a nested QEventLoop, which can
     * in some situation cause problems and deadlocks. It is recommended to use
     * FutureWatcher.
     *
     * @see isFinished()
     */
    void waitForFinished() const;

    /**
     * Marks the future as finished. This will cause all FutureWatcher&lt;T&gt;
     * objects watching this particular instance to emit FutureWatcher::futureReady()
     * signal, and will cause all callers currently blocked in Future::waitForFinished()
     * method of this particular instance to resume.
     *
     * @warning This method must only be called by the tasks inside Job,
     * never by outside users.
     *
     * @see isFinished()
     */
    void setFinished();

    /**
     * Query whether the Future has already finished.
     *
     * @see setFinished()
     */
    bool isFinished() const;

    /**
     * Used by tasks to report an error that happened during execution. If an
     * error handler was provided to the task, it will be executed with the
     * given arguments. Otherwise the error will be propagated to next task
     * that has an error handler, or all the way up to user.
     *
     * This method also internally calls setFinished()
     *
     * @warning This method must only be called by the tasks inside Job,
     * never by outside users.
     *
     * @param code Optional error code
     * @param message Optional error message
     *
     * @see errorCode(), errorMessage()
     */
    void setError(int code = 1, const QString &message = QString());

    /**
     * Returns error code set via setError() or 0 if no
     * error has occurred.
     *
     * @see setError(), errorMessage()
     */
    int errorCode() const;

    /**
     * Returns error message set via setError() or empty
     * string if no error occured.
     *
     * @see setError(), errorCode()
     */
    QString errorMessage() const;

    /**
     * Sets progress of the task. All FutureWatcher instances watching
     * this particular future will then emit FutureWatcher::futureProgress()
     * signal.
     *
     * @param processed Already processed amount
     * @param total Total amount to process
     */
    void setProgress(int processed, int total);

    /**
     * Sets progress of the task.
     *
     * @param progress Progress
     */
    void setProgress(qreal progress);

#endif // ONLY_DOXYGEN

protected:
    //@cond PRIVATE
    Future(const KAsync::Private::ExecutionPtr &execution)
        : FutureGeneric<T>(execution)
    {}
    //@endcond

};

/**
 * @ingroup Future
 *
 * @brief A specialization of Future&lt;T&gt; for tasks that have no (void)
 * result.
 *
 * Unlike the generic Future&lt;T&gt; this specialization does not have
 * setValue() and value() methods to set/retrieve result.
 *
 * @see Future
 */
template<>
class Future<void> : public FutureGeneric<void>
{
    friend class KAsync::Private::ExecutorBase;

public:
    /**
     * @brief Constructor
     */
    Future()
        : FutureGeneric<void>(KAsync::Private::ExecutionPtr())
    {}

    /**
     * @brief Copy constructor
     */
    Future(const Future<void> &other)
        : FutureGeneric<void>(other)
    {}

protected:
    //@cond PRIVATE
    Future(const KAsync::Private::ExecutionPtr &execution)
        : FutureGeneric<void>(execution)
    {}
    //@endcond
};




//@cond PRIVATE
class KASYNC_EXPORT FutureWatcherBase : public QObject
{
    Q_OBJECT

    friend class FutureBase;

Q_SIGNALS:
    void futureReady();
    void futureProgress(qreal progress);

protected:
    FutureWatcherBase(QObject *parent = nullptr);

    virtual ~FutureWatcherBase();

    void futureReadyCallback();
    void futureProgressCallback(qreal progress);

    void setFutureImpl(const KAsync::FutureBase &future);

protected:
    class Private {
    public:
        KAsync::FutureBase future;
    };

    Private * const d;

private:
    Q_DISABLE_COPY(FutureWatcherBase);
};
//@endcond


/**
 * @ingroup Future
 *
 * @brief The FutureWatcher allows monitoring of Job results using
 * signals and slots.
 *
 * FutureWatcher is returned by Job upon execution. User can then
 * connect to its futureReady() and futureProgress() signals to be notified
 * about progress of the asynchronous job. When futureReady() signal is emitted,
 * the result of the job is available in Future::value().
 */
template<typename T>
class FutureWatcher : public FutureWatcherBase
{
    //@cond PRIVATE
    friend class KAsync::FutureGeneric<T>;
    //@endcond

public:
    /**
     * Constructs a new FutureWatcher that can watch for status of Future&lt;T&gt;
     */
    FutureWatcher(QObject *parent = nullptr)
        : FutureWatcherBase(parent)
    {}

    ~FutureWatcher()
    {}

    /**
     * Set future to watch.
     *
     * @param future Future object to watch
     */
    void setFuture(const KAsync::Future<T> &future)
    {
        setFutureImpl(*static_cast<const KAsync::FutureBase*>(&future));
    }

    /**
     * Returns currently watched future.
     */
    KAsync::Future<T> future() const
    {
        return *static_cast<KAsync::Future<T>*>(&d->future);
    }

#ifdef ONLY_DOXYGEN
Q_SIGNALS:
    /**
     * The signal is emitted when the execution has finished and the result
     * can be collected.
     *
     * @see Future::setFinished(), Future::setError()
     */
    void futureReady();

    /**
     * The signal is emitted when progress of the execution changes. This has
     * to be explicitly supported by the job being executed, otherwise the
     * signal is not emitted.
     *
     * @see Future::setProgress()
     */
    void futureProgress(qreal progress);
#endif

private:
    Q_DISABLE_COPY(FutureWatcher<T>);
};

} // namespace Async

#endif // FUTURE_H
