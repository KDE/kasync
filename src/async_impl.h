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

#ifndef ASYNC_IMPL_H
#define ASYNC_IMPL_H

#include "async.h"

namespace Async {

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

} // namespace Detail

} // namespace Async

#endif // ASYNC_IMPL_H
