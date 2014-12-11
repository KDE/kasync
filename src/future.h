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

namespace Async {

class FutureBase
{
public:
    FutureBase();
    FutureBase(const FutureBase &other);
    virtual ~FutureBase();

    void setFinished();
    bool isFinished() const;
    void waitForFinished();

protected:
    bool mFinished;
    QEventLoop *mWaitLoop;
};

template<typename T>
class Future : public FutureBase
{
public:
    Future()
        : FutureBase()
    {}

    Future(const Future<T> &other)
        : FutureBase(other)
        , mValue(other.mValue)
    {}

    Future(const T &val)
        : FutureBase()
        , mValue(val)
    {}

    void setValue(const T &val)
    {
        mValue = val;
    }

    T value() const
    {
        return mValue;
    }

private:
    T mValue;
};

template<>
class Future<void> : public FutureBase
{
public:
    Future()
        : FutureBase()
    {}

    Future(const Future<void> &other)
        : FutureBase(other)
    {}
};

} // namespace Async

#endif // FUTURE_H
