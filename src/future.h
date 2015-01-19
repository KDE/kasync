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

class FutureBase
{
public:
    virtual ~FutureBase();

    virtual void setFinished() = 0;
    virtual bool isFinished() const = 0;
    virtual void setError(int code = 1, const QString &message = QString()) = 0;

protected:
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
    FutureGeneric()
        : FutureBase()
        , d(new Private)
    {}

    FutureGeneric(const FutureGeneric<T> &other)
        : FutureBase(other)
        , d(other.d)
    {}

    class Private : public QSharedData
    {
    public:
        Private() : QSharedData(), finished(false) {}
        typename std::conditional<std::is_void<T>::value, int /* dummy */, T>::type
        value;

        QVector<QPointer<FutureWatcher<T>>> watchers;
        bool finished;
    };

    QExplicitlySharedDataPointer<Private> d;

    void addWatcher(FutureWatcher<T> *watcher)
    {
        d->watchers.append(QPointer<FutureWatcher<T>>(watcher));
    }
};

template<typename T>
class Future : public FutureGeneric<T>
{
public:
    Future()
        : FutureGeneric<T>()
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
};

template<>
class Future<void> : public FutureGeneric<void>
{
public:
    Future()
        : FutureGeneric<void>()
    {}

    Future(const Future<void> &other)
        : FutureGeneric<void>(other)
    {}
};


class FutureWatcherBase : public QObject
{
    Q_OBJECT

protected:
    FutureWatcherBase(QObject *parent = 0);
    virtual ~FutureWatcherBase();

Q_SIGNALS:
    void futureReady();
};

template<typename T>
class FutureWatcher : public FutureWatcherBase
{
    friend class Async::FutureGeneric<T>;

public:
    FutureWatcher(QObject *parent = 0)
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
