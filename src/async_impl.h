/*
 * Copyright 2014 - 2015 Daniel Vrátil <dvratil@redhat.com>
 * Copyright 2016        Daniel Vrátil <dvratil@kde.org>
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

#include <type_traits>
#include <tuple>

//@cond PRIVATE

namespace KAsync {

template<typename T>
class Future;

namespace detail {

template<typename T, typename Enable = void>
struct isIterable {
    enum { value = 0 };
};

template<typename T>
struct isIterable<T, std::conditional_t<false, typename T::iterator, void>> {
    enum { value = 1 };
};

template<typename ... T>
struct prevOut {
    using type = std::tuple_element_t<0, std::tuple<T ..., void>>;
};

template<typename T, typename Out, typename ... In>
struct funcHelper {
    using type = void(T::*)(In ..., KAsync::Future<Out> &);
};

template<typename T, typename Out, typename ... In>
struct syncFuncHelper {
    using type =  Out(T::*)(In ...);
};

template<typename T,
         class = std::enable_if_t<!std::is_void<T>::value>,
         class = std::enable_if_t<std::is_move_constructible<T>::value>>
void copyFutureValue(KAsync::Future<T> &in, KAsync::Future<T> &out)
{
    out.setValue(std::move(in.value()));
}

template<typename T,
         class = std::enable_if_t<!std::is_void<T>::value>,
         class = std::enable_if_t<!std::is_move_constructible<T>::value>,
         class = std::enable_if_t<std::is_copy_constructible<T>::value>>
void copyFutureValue(const KAsync::Future<T> &in, KAsync::Future<T> &out)
{
    out.setValue(in.value());
}

template<typename T>
inline std::enable_if_t<std::is_void<T>::value, void>
copyFutureValue(const KAsync::Future<T> &/* in */, KAsync::Future<T> &/* out */)
{
    // noop
}

template<typename T>
inline std::enable_if_t<!std::is_void<T>::value, void>
aggregateFutureValue(const KAsync::Future<T> &in, KAsync::Future<T> &out)
{
    out.setValue(out.value() + in.value());
}

template<typename T>
inline std::enable_if_t<std::is_void<T>::value, void>
aggregateFutureValue(const KAsync::Future<T> & /*in */, KAsync::Future<T> & /*out */)
{
    // noop
}

} // namespace Detail


} // namespace KAsync

//@endcond

#endif // KASYNC_IMPL_H
