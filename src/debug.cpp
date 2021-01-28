/*
    SPDX-FileCopyrightText: 2015 Daniel Vrátil <dvratil@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "debug.h"
#include "async.h"

#include <QStringBuilder>

#ifdef __GNUG__
#include <cxxabi.h>
#include <memory>
#include <cstdlib>
#endif

namespace KAsync
{

Q_LOGGING_CATEGORY(Debug, "org.kde.async", QtWarningMsg)
Q_LOGGING_CATEGORY(Trace, "org.kde.async.trace", QtWarningMsg)

QString demangleName(const char *name)
{
    if (!name || !*name) {
        return {};
    }
#ifdef __GNUG__
    int status = 1; // uses -3 to 0 error codes
    std::unique_ptr<char, void(*)(void*)> demangled(abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free);
    if (status == 0) {
        return QString::fromLatin1(demangled.get());
    }
#endif
    return QString::fromLatin1(name);
}

}

using namespace KAsync;

int Tracer::lastId = 0;

Tracer::Tracer(Private::Execution *execution)
    : mId(lastId++)
    , mExecution(execution)
{
    msg(KAsync::Tracer::Start);
}

Tracer::~Tracer()
{
    msg(KAsync::Tracer::End);
    // FIXME: Does this work on parallel executions?
    --lastId;
    --mId;
}

void Tracer::msg(Tracer::MsgType msgType)
{
    qCDebug(Trace).nospace() << (QString().fill(QLatin1Char(' '), mId * 2) % 
                                 (msgType == KAsync::Tracer::Start ? QStringLiteral(" START ") : QStringLiteral(" END   ")) %
                                 QString::number(mId) % QStringLiteral(" ") %
                                 mExecution->executor->mExecutorName);
}
