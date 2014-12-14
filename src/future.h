/*
 * Copyright 2014  Daniel Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef FUTURE_H
#define FUTURE_H

class QEventLoop;

#include <type_traits>

#include <QSharedDataPointer>
#include <QPointer>
#include <QVector>

namespace Async {

class FutureBase
{
public:
    virtual ~FutureBase();

    virtual void setFinished() = 0;
    bool isFinished() const;

protected:
    FutureBase();
    FutureBase(const FutureBase &other);

    bool mFinished;
    QEventLoop *mWaitLoop;
};

template<typename T>
class FutureWatcher;

template<typename T>
class FutureGeneric : public FutureBase
{
    friend class FutureWatcher<T>;

public:
    void setFinished()
    {
        mFinished = true;
        for (auto watcher : d->watchers) {
            if (watcher) {
                watcher->futureReadyCallback();
            }
        }
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
        typename std::conditional<std::is_void<T>::value, int /* dummy */, T>::type
        value;

        QVector<QPointer<FutureWatcher<T>>> watchers;
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
