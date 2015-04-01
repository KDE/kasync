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

class QEventLoop;

#include <type_traits>

#include <QSharedDataPointer>
#include <QPointer>
#include <QVector>
#include <QEventLoop>

namespace Async {

namespace Private {
class Execution;
class ExecutorBase;

typedef QSharedPointer<Execution> ExecutionPtr;

}

class FutureBase
{
    friend class Async::Private::Execution;

public:
    virtual ~FutureBase();

    virtual void setFinished() = 0;
    virtual bool isFinished() const = 0;
    virtual void setError(int code = 1, const QString &message = QString()) = 0;

protected:
    virtual void releaseExecution() = 0;

    class PrivateBase : public QSharedData
    {
    public:
        PrivateBase(const Async::Private::ExecutionPtr &execution);
        virtual ~PrivateBase();

        void releaseExecution();

    private:
        QWeakPointer<Async::Private::Execution> mExecution;
    };

    FutureBase();
    FutureBase(const FutureBase &other);
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
    void setFinished()
    {
        if (d->finished) {
            return;
        }
        d->finished = true;
        for (auto watcher : d->watchers) {
            if (watcher) {
                watcher->futureReadyCallback();
            }
        }
    }

    bool isFinished() const
    {
        return d->finished;
    }

    void setError(int errorCode, const QString &message)
    {
        d->errorCode = errorCode;
        d->errorMessage = message;
        setFinished();
    }

    int errorCode() const
    {
        return d->errorCode;
    }

    QString errorMessage() const
    {
        return d->errorMessage;
    }

    void waitForFinished()
    {
        if (isFinished()) {
            return;
        }
        FutureWatcher<T> watcher;
        QEventLoop eventLoop;
        QObject::connect(&watcher, &Async::FutureWatcher<T>::futureReady,
                         &eventLoop, &QEventLoop::quit);
        watcher.setFuture(*static_cast<Async::Future<T>*>(this));
        eventLoop.exec();
    }

protected:
    FutureGeneric(const Async::Private::ExecutionPtr &execution)
        : FutureBase()
        , d(new Private(execution))
    {}

    FutureGeneric(const FutureGeneric<T> &other)
        : FutureBase(other)
        , d(other.d)
    {}

    class Private : public FutureBase::PrivateBase
    {
    public:
        Private(const Async::Private::ExecutionPtr &execution)
            : FutureBase::PrivateBase(execution)
            , finished(false)
            , errorCode(0)
        {}

        typename std::conditional<std::is_void<T>::value, int /* dummy */, T>::type
        value;

        QVector<QPointer<FutureWatcher<T>>> watchers;
        bool finished;
        int errorCode;
        QString errorMessage;
    };

    QExplicitlySharedDataPointer<Private> d;

    void releaseExecution()
    {
        d->releaseExecution();
    }

    void addWatcher(FutureWatcher<T> *watcher)
    {
        d->watchers.append(QPointer<FutureWatcher<T>>(watcher));
    }
};

template<typename T>
class Future : public FutureGeneric<T>
{
    friend class Async::Private::ExecutorBase;

    template<typename T_>
    friend class Async::FutureWatcher;

public:
    Future()
        : FutureGeneric<T>(Async::Private::ExecutionPtr())
    {}

    Future(const Future<T> &other)
        : FutureGeneric<T>(other)
    {}

    void setValue(const T &value)
    {
        this->d->value = value;
    }

    T value() const
    {
        return this->d->value;
    }

protected:
    Future(const Async::Private::ExecutionPtr &execution)
        : FutureGeneric<T>(execution)
    {}

};

template<>
class Future<void> : public FutureGeneric<void>
{
    friend class Async::Private::ExecutorBase;

    template<typename T_>
    friend class Async::FutureWatcher;

public:
    Future()
        : FutureGeneric<void>(Async::Private::ExecutionPtr())
    {}

    Future(const Future<void> &other)
        : FutureGeneric<void>(other)
    {}

protected:
    Future(const Async::Private::ExecutionPtr &execution)
        : FutureGeneric<void>(execution)
    {}
};


class FutureWatcherBase : public QObject
{
    Q_OBJECT

protected:
    FutureWatcherBase(QObject *parent = nullptr);
    virtual ~FutureWatcherBase();

Q_SIGNALS:
    void futureReady();
};

template<typename T>
class FutureWatcher : public FutureWatcherBase
{
    friend class Async::FutureGeneric<T>;

public:
    FutureWatcher(QObject *parent = nullptr)
        : FutureWatcherBase(parent)
    {}

    ~FutureWatcher()
    {}

    void setFuture(const Async::Future<T> &future)
    {
        mFuture = future;
        mFuture.addWatcher(this);
        if (future.isFinished()) {
            futureReadyCallback();
        }
    }

    Async::Future<T> future() const
    {
        return mFuture;
    }

private:
    void futureReadyCallback()
    {
        Q_EMIT futureReady();
    }

    Async::Future<T> mFuture;
};

} // namespace Async

#endif // FUTURE_H
