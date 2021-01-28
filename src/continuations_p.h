/*
    SPDX-FileCopyrightText: 2014-2015 Daniel Vrátil <dvratil@redhat.com>
    SPDX-FileCopyrightText: 2016-2019 Daniel Vrátil <dvratil@kde.org>
    SPDX-FileCopyrightText: 2016 Christian Mollekopf <mollekopf@kolabsystems.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KASYNC_CONTINUATIONS_P_H_
#define KASYNC_CONTINUATIONS_P_H_

#include <limits>
#include <functional>
#include <type_traits>

namespace KAsync
{

template<typename Out, typename ... In>
class Job;

template<typename T>
class Future;

struct Error;

//@cond PRIVATE
namespace detail {
template<typename T>
struct identity {
    using type = T;
};
template<typename T>
using identity_t = typename identity<T>::type;
}
//@endcond

template<typename Out, typename ... In>
using AsyncContinuation = detail::identity_t<std::function<void(In ..., KAsync::Future<Out>&)>>;

template<typename Out, typename ... In>
using AsyncErrorContinuation = detail::identity_t<std::function<void(const KAsync::Error &, In ..., KAsync::Future<Out>&)>>;

template<typename Out, typename ... In>
using SyncContinuation = detail::identity_t<std::function<Out(In ...)>>;

template<typename Out, typename ... In>
using SyncErrorContinuation = detail::identity_t<std::function<Out(const KAsync::Error &, In ...)>>;

template<typename Out, typename ... In>
using JobContinuation = detail::identity_t<std::function<KAsync::Job<Out>(In ...)>>;

template<typename Out, typename ... In>
using JobErrorContinuation = detail::identity_t<std::function<KAsync::Job<Out>(const KAsync::Error &, In ...)>>;

//@cond PRIVATE
namespace Private
{
/**
 * FIXME: This should be a simple alias to std::variant once we can depend on C++17.
 */
template<typename Out, typename ... In>
struct ContinuationHolder
{
#ifndef KASYNC_TEST
private:
#endif
    using Tuple = std::tuple<
        AsyncContinuation<Out, In ...>,
        AsyncErrorContinuation<Out, In ...>,
        SyncContinuation<Out, In ...>,
        SyncErrorContinuation<Out, In ...>,
        JobContinuation<Out, In ...>,
        JobErrorContinuation<Out, In ...>
    >;

    template<typename T>
    struct tuple_max;

    template<typename T>
    struct tuple_max<std::tuple<T>> {
        static constexpr std::size_t size = sizeof(T);
        static constexpr std::size_t alignment = alignof(T);
    };
    template<typename T, typename ... Types>
    struct tuple_max<std::tuple<T, Types ...>> {
        static constexpr std::size_t size = std::max(sizeof(T), tuple_max<std::tuple<Types ...>>::size);
        static constexpr std::size_t alignment = std::max(alignof(T), tuple_max<std::tuple<Types ...>>::alignment);
    };

    template<typename T, typename Tuple>
    struct tuple_index;

    template<typename T, typename ... Types>
    struct tuple_index<T, std::tuple<T, Types ...>> {
        static constexpr std::size_t value = 0;
    };
    template<typename T, typename U, typename ... Types>
    struct tuple_index<T, std::tuple<U, Types ...>> {
        static constexpr std::size_t value = tuple_index<T, std::tuple<Types ...>>::value + 1;
    };

    template<typename T>
    inline static void move_helper(void *storage, void *data) {
        new (storage) T(std::move(*reinterpret_cast<T*>(data)));
    }

    template<typename T>
    inline static void destroy_helper(void *storage) {
        reinterpret_cast<T*>(storage)->~T();
    }

    template<typename Tuple,
             std::size_t I = std::tuple_size<Tuple>::value - 1>
    struct storage_helper {
        using T = std::tuple_element_t<I, Tuple>;
        inline static void move(std::size_t index, void *storage, void *data) {
            if (I == index) {
                move_helper<T>(storage, data);
            } else {
                storage_helper<Tuple, I - 1>::move(index, storage, data);
            }
        }
        inline static void destroy(std::size_t index, void *storage) {
            if (I == index) {
                destroy_helper<T>(storage);
            } else {
                storage_helper<Tuple, I - 1>::destroy(index, storage);
            }
        }
    };

    template<typename Tuple>
    struct storage_helper<Tuple, 0> {
        using T = std::tuple_element_t<0, Tuple>;
        inline static void move(std::size_t, void *storage, void *data) {
            move_helper<T>(storage, data);
        }
        inline static void destroy(std::size_t, void *storage) {
            destroy_helper<T>(storage);
        }
    };

    enum {
        Invalid = std::numeric_limits<std::size_t>::max() - 1
    };

    std::size_t mIndex = Invalid;
    std::aligned_storage_t<tuple_max<Tuple>::size, tuple_max<Tuple>::alignment> mStorage = {};

public:
    #define KASYNC_P_DEFINE_CONSTRUCTOR(type) \
        ContinuationHolder(type<Out, In ...> &&cont) \
            : mIndex(tuple_index<type<Out, In ...>, Tuple>::value) \
        { \
            move_helper<type<Out, In ...>>(&mStorage, &cont); \
        }
    KASYNC_P_DEFINE_CONSTRUCTOR(AsyncContinuation)
    KASYNC_P_DEFINE_CONSTRUCTOR(AsyncErrorContinuation)
    KASYNC_P_DEFINE_CONSTRUCTOR(SyncContinuation)
    KASYNC_P_DEFINE_CONSTRUCTOR(SyncErrorContinuation)
    KASYNC_P_DEFINE_CONSTRUCTOR(JobContinuation)
    KASYNC_P_DEFINE_CONSTRUCTOR(JobErrorContinuation)
    #undef KASYNC_P_DEFINE_CONSTRUCTOR

    ContinuationHolder(ContinuationHolder &&other) noexcept {
        std::swap(mIndex, other.mIndex);
        storage_helper<Tuple>::move(mIndex, &mStorage, &other.mStorage);
    }

    ContinuationHolder &operator=(ContinuationHolder &&other) noexcept {
        std::swap(mIndex, other.mIndex);
        storage_helper<Tuple>::move(mIndex, &mStorage, &other.mStorage);
        return *this;
    }

    ContinuationHolder(const ContinuationHolder &) = delete;
    ContinuationHolder &operator=(const ContinuationHolder &) = delete;

    ~ContinuationHolder() {
        if (mIndex != Invalid) {
            storage_helper<Tuple>::destroy(mIndex, &mStorage);
            mIndex = Invalid;
        }
    }

    template<typename T>
    inline bool is() const {
        return mIndex == tuple_index<T, Tuple>::value;
    }

    template<typename T>
    inline const T &get() const {
        if (!is<T>()) {
            throw std::bad_cast();
        }
        return *reinterpret_cast<const T *>(&mStorage);
    }

    template<typename T>
    inline T &&get() {
        if (!is<T>()) {
            throw std::bad_cast();
        }
        return std::move(*reinterpret_cast<T *>(&mStorage));
    }
};

template<typename T, typename Holder>
inline bool continuationIs(const Holder &holder) {
    return holder.template is<T>();
}

template<typename T, typename Holder>
inline const T &continuationGet(const Holder &holder) {
    return holder.template get<T>();
}

template<typename T, typename Holder>
inline T &&continuationGet(Holder &holder) {
    return holder.template get<T>();
}

} // namespace Private
//@endcond


}


#endif
