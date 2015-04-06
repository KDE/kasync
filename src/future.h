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

class FutureWatcherBase;
template<typename T>
class FutureWatcher;

namespace Private {
class Execution;
class ExecutorBase;

typedef QSharedPointer<Execution> ExecutionPtr;
} // namespace Private

class FutureBase
{
    friend class Async::Private::Execution;
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
        PrivateBase(const Async::Private::ExecutionPtr &execution);
        virtual ~PrivateBase();

        void releaseExecution();

        bool finished;
        int errorCode;
        QString errorMessage;

        QVector<QPointer<FutureWatcherBase>> watchers;
    private:
        QWeakPointer<Async::Private::Execution> mExecution;
    };

    FutureBase();
    FutureBase(FutureBase::PrivateBase *dd);
    FutureBase(const FutureBase &other);

    void addWatcher(Async::FutureWatcherBase *watcher);
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
        QObject::connect(&watcher, &Async::FutureWatcher<T>::futureReady,
                         &eventLoop, &QEventLoop::quit);
        watcher.setFuture(*static_cast<Async::Future<T>*>(this));
        eventLoop.exec();
    }

protected:
    FutureGeneric(const Async::Private::ExecutionPtr &execution)
        : FutureBase(new Private(execution))
    {}

    FutureGeneric(const FutureGeneric<T> &other)
        : FutureBase(other)
    {}

protected:
    class Private : public FutureBase::PrivateBase
    {
    public:
        Private(const Async::Private::ExecutionPtr &execution)
            : FutureBase::PrivateBase(execution)
        {}

        typename std::conditional<std::is_void<T>::value, int /* dummy */, T>::type
        value;
    };
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
        static_cast<typename FutureGeneric<T>::Private*>(this->d.data())->value = value;
    }

    T value() const
    {
        return static_cast<typename FutureGeneric<T>::Private*>(this->d.data())->value;
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

    friend class FutureBase;

Q_SIGNALS:
    void futureReady();
    void futureProgress(qreal progress);

protected:
    FutureWatcherBase(QObject *parent = nullptr);
    virtual ~FutureWatcherBase();

    void futureReadyCallback();
    void futureProgressCallback(qreal progress);

    void setFutureImpl(const Async::FutureBase &future);

protected:
    class Private {
    public:
        Async::FutureBase future;
    };

    Private * const d;

private:
    Q_DISABLE_COPY(FutureWatcherBase);
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
        setFutureImpl(*static_cast<const Async::FutureBase*>(&future));
    }

    Async::Future<T> future() const
    {
        return *static_cast<Async::Future<T>*>(&d->future);
    }

private:
    Q_DISABLE_COPY(FutureWatcher<T>);
};

} // namespace Async

#endif // FUTURE_H
