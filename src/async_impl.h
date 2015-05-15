/*
 * Copyright 2014 - 2015 Daniel Vrátil <dvratil@redhat.com>
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

#ifndef KASYNC_IMPL_H
#define KASYNC_IMPL_H

#include "async.h"
#include <type_traits>

namespace KAsync {

namespace detail {

template<typename T>
struct identity
{
    typedef T type;
};

template<typename T, typename Enable = void>
struct isIterable {
    enum { value = 0 };
};

template<typename T>
struct isIterable<T, typename std::conditional<false, typename T::iterator, void>::type> {
    enum { value = 1 };
};

template<typename ... T>
struct prevOut {
    using type = typename std::tuple_element<0, std::tuple<T ..., void>>::type;
};

template<typename T>
inline typename std::enable_if<!std::is_void<T>::value, void>::type
copyFutureValue(const KAsync::Future<T> &in, KAsync::Future<T> &out)
{
    out.setValue(in.value());
}

template<typename T>
inline typename std::enable_if<std::is_void<T>::value, void>::type
copyFutureValue(const KAsync::Future<T> &in, KAsync::Future<T> &out)
{
    // noop
}

template<typename T>
inline typename std::enable_if<!std::is_void<T>::value, void>::type
aggregateFutureValue(const KAsync::Future<T> &in, KAsync::Future<T> &out)
{
    out.setValue(out.value() + in.value());
}

template<typename T>
inline typename std::enable_if<std::is_void<T>::value, void>::type
aggregateFutureValue(const KAsync::Future<T> &in, KAsync::Future<T> &out)
{
    // noop
}

} // namespace Detail

} // namespace KAsync

#endif // KASYNC_IMPL_H
