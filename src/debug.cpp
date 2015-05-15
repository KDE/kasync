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

#include "debug.h"
#include "async.h"

#include <QStringBuilder>

#ifdef __GNUG__
#include <cxxabi.h>
#include <memory>
#endif

namespace KAsync
{

Q_LOGGING_CATEGORY(Debug, "org.kde.async", QtWarningMsg);
Q_LOGGING_CATEGORY(Trace, "org.kde.async.trace", QtWarningMsg);

QString demangleName(const char *name)
{
#ifdef __GNUG__
    int status = 1; // uses -3 to 0 error codes
    std::unique_ptr<char, void(*)(void*)> demangled(abi::__cxa_demangle(name, 0, 0, &status), std::free);
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
#ifndef QT_NO_DEBUG
    qCDebug(Trace).nospace() << (QString().fill(QLatin1Char(' '), mId * 2) % 
                                 (msgType == KAsync::Tracer::Start ? QStringLiteral(" START ") : QStringLiteral(" END   ")) %
                                 QString::number(mId) % QStringLiteral(" ") %
                                 mExecution->executor->mExecutorName);
#endif
}
