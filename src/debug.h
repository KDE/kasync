/*
 * Copyright 2015  Daniel Vrátil <dvratil@redhat.com>
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

#ifndef ASYNC_DEBUG_H
#define ASYNC_DEBUG_H

#include <QLoggingCategory>
#include <QStringBuilder>

#ifndef QT_NO_DEBUG
#include <typeinfo>
#endif

namespace Async
{

Q_DECLARE_LOGGING_CATEGORY(Debug)
Q_DECLARE_LOGGING_CATEGORY(Trace)

QString demangleName(const char *name);

namespace Private
{
class Execution;
}

class Tracer
{
public:
    Tracer(Private::Execution *execution);
    ~Tracer();

private:
    enum MsgType {
        Start,
        End
    };
    void msg(MsgType);

    int mId;
    Private::Execution *mExecution;

    static int lastId;
};

}

#ifndef QT_NO_DEBUG
    template<typename T>
    QString storeExecutorNameExpanded() {
        return Async::demangleName(typeid(T).name());
    }

    template<typename T, typename ... Tail>
    typename std::enable_if<sizeof ... (Tail) != 0, QString>::type
    storeExecutorNameExpanded() {
        return storeExecutorNameExpanded<T>() % QStringLiteral(", ") % storeExecutorNameExpanded<Tail ...>();
    }

    #define STORE_EXECUTOR_NAME(name, ...) \
        ExecutorBase::mExecutorName = QStringLiteral(name) % QStringLiteral("<") % storeExecutorNameExpanded<__VA_ARGS__>() % QStringLiteral(">")
#else
    #define STORE_EXECUTOR_NAME(...)
#endif

#endif // ASYNC_DEBUG_H