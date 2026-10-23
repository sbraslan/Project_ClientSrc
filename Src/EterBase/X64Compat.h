#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace m2
{
    template <typename To, typename From>
    constexpr To narrow(From value)
    {
        static_assert(std::is_integral_v<To>);
        static_assert(std::is_integral_v<From>);

        if constexpr (std::is_signed_v<From> == std::is_signed_v<To>)
        {
            if constexpr (sizeof(To) < sizeof(From))
            {
                assert(
                    value >= static_cast<From>(std::numeric_limits<To>::lowest()) &&
                    value <= static_cast<From>(std::numeric_limits<To>::max())
                );
            }
        }
        else if constexpr (std::is_signed_v<From>)
        {
            assert(value >= 0);

            using UFrom = std::make_unsigned_t<From>;

            assert(
                static_cast<UFrom>(value) <=
                static_cast<UFrom>(std::numeric_limits<To>::max())
            );
        }
        else
        {
            using UTo = std::make_unsigned_t<To>;

            if constexpr (sizeof(To) <= sizeof(From))
            {
                assert(
                    value <= static_cast<From>(
                        static_cast<UTo>(std::numeric_limits<To>::max())
                    )
                );
            }
        }

        return static_cast<To>(value);
    }

    template <typename T>
    constexpr std::uintptr_t ptr_value(T* ptr) noexcept
    {
        return reinterpret_cast<std::uintptr_t>(ptr);
    }

    template <typename T>
    constexpr T* ptr_from_value(std::uintptr_t value) noexcept
    {
        return reinterpret_cast<T*>(value);
    }

    template <typename T>
    constexpr std::ptrdiff_t pointer_offset(
        const T* from,
        const T* to
    ) noexcept
    {
        return
            reinterpret_cast<const unsigned char*>(to) -
            reinterpret_cast<const unsigned char*>(from);
    }
}