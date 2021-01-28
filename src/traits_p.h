/*
    SPDX-FileCopyrightText: 2019 Daniel Vrátil <dvratil@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
