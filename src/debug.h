/*
    SPDX-FileCopyrightText: 2015 Daniel Vrátil <dvratil@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KASYNC_DEBUG_H
#define KASYNC_DEBUG_H

//krazy:excludeall=dpointer

#include "kasync_export.h"

#include <QLoggingCategory>
#include <QStringBuilder>

#ifndef QT_NO_DEBUG
#include <typeinfo>
#endif

namespace KAsync
{

Q_DECLARE_LOGGING_CATEGORY(Debug)
Q_DECLARE_LOGGING_CATEGORY(Trace)

KASYNC_EXPORT QString demangleName(const char *name);

namespace Private
{
struct Execution;
}

class KASYNC_EXPORT Tracer
{
public:
    explicit Tracer(Private::Execution *execution);
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
        return KAsync::demangleName(typeid(T).name());
    }

    template<typename T, typename ... Tail>
    auto storeExecutorNameExpanded() -> std::enable_if_t<sizeof ... (Tail) != 0, QString>
    {
        return storeExecutorNameExpanded<T>() % QStringLiteral(", ") % storeExecutorNameExpanded<Tail ...>();
    }

    #define STORE_EXECUTOR_NAME(name, ...) \
        ExecutorBase::mExecutorName = QStringLiteral(name "<") % storeExecutorNameExpanded<__VA_ARGS__>() % QStringLiteral(">")
#else
    #define STORE_EXECUTOR_NAME(...)
#endif

#endif // KASYNC_DEBUG_H
