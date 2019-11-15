/*
 * Copyright 2019 Daniel Vrátil <dvratil@kde.org>
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

#ifndef KASYNC_TRAITS_H_
#define KASYNC_TRAITS_H_

#include <utility>

namespace KAsync {

namespace traits {

template<typename T, typename = void>
struct isContainer {
    enum { value = 0 };
};

template<typename T>
struct isContainer<T, std::void_t<decltype(std::declval<T&>().begin()),
                                  decltype(std::declval<T&>().end()),
                                  typename T::value_type>>
{
    enum { value = 1 };
};



} // namespace traits
} // namespace KAsync

#endif
