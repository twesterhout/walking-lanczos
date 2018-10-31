// Copyright Tom Westerhout (c) 2018
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//
//     * Neither the name of Tom Westerhout nor the names of other
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "config.hpp"

#include <boost/functional/hash.hpp>
#include <nonstd/span.hpp>

#include <algorithm>
#include <cstdio>
#include <immintrin.h>

enum class Spin : unsigned char {
    down = 0x00,
    up   = 0x01,
};

class SpinVector {

  public:
    constexpr SpinVector() noexcept : _data{} {}
    constexpr SpinVector(SpinVector const&) noexcept = default;
    constexpr SpinVector(SpinVector&&) noexcept      = default;
    constexpr SpinVector& operator=(SpinVector const&) noexcept = default;
    constexpr SpinVector& operator=(SpinVector&&) noexcept = default;

    /// NOTE: This function is not very efficient and should not be used in
    /// hot paths.
    template <class RAIterB, class RAIterE>
    SpinVector(RAIterB begin, RAIterE end) TCM_NOEXCEPT
    {
        auto const n = static_cast<std::uint16_t>(end - begin);
        TCM_ASSERT(n <= std::size(_data.spin) * 8);
        // TCM_ASSERT(std::all_of(
        //     begin, end, [](auto const x) { return x == 0 || x == 1; }));

        _data.size = n;
        std::fill(std::begin(_data.spin), std::end(_data.spin), std::byte{0});
        TCM_ASSERT(_data.size == n);
        for (auto i = 0; begin != end; ++begin, ++i) {
            (*this)[i] = static_cast<Spin>(*begin);
        }
    }

    SpinVector(std::initializer_list<int> spins) TCM_NOEXCEPT
        : SpinVector{spins.begin(), spins.end()}
    {
    }

  private:
    static constexpr auto get_bit(std::byte const x, int const i) TCM_NOEXCEPT
        -> Spin
    {
        TCM_ASSERT(0 <= i && i < 8);
        return Spin{(x >> (7 - i)) & std::byte{0x01}};
    }

    static constexpr auto flip_bit(std::byte& x, int const i) TCM_NOEXCEPT
        -> void
    {
        TCM_ASSERT(0 <= i && i < 8);
        x ^= std::byte{0x01} << (7 - i);
    }

    class SpinReference {
      public:
        /// \brief Constructs a reference to a spin given a reference to the
        /// byte where the spin is contained and the index within that byte.
        constexpr SpinReference(std::byte& ref, int const n) TCM_NOEXCEPT
            : _ref{ref}
            , _i{7 - n}
        {
            TCM_ASSERT(0 <= n && n < 8);
        }

        SpinReference(SpinReference const&) = delete;
        SpinReference(SpinReference&&)      = delete;
        SpinReference& operator=(SpinReference&&) = delete;
        SpinReference& operator=(SpinReference const&) = delete;

        SpinReference& operator=(Spin const x) noexcept
        {
            _ref = (_ref & ~(std::byte{1} << _i))
                   | ((std::byte{x} & std::byte{0x01}) << _i);
            return *this;
        }

        constexpr operator Spin() const noexcept
        {
            return get_bit(_ref, 7 - _i);
        }

      private:
        std::byte& _ref;
        int        _i;
    };

  public:
    constexpr auto size() const noexcept { return _data.size; }

    static constexpr auto max_size() noexcept { return 8 * sizeof(_data.spin); }

    constexpr auto operator[](int const i) const TCM_NOEXCEPT -> Spin
    {
        TCM_ASSERT(0 <= i && i < size());
        // TODO(twesterhout): Check the generated assembly
        auto const chunk = i / 8;
        auto const rest  = i % 8;
        return get_bit(_data.spin[chunk], rest);
    }

    constexpr auto operator[](int const i) noexcept -> SpinReference
    {
        TCM_ASSERT(0 <= i && i < size());
        auto const chunk = i / 8;
        auto const rest  = i % 8;
        return SpinReference{_data.spin[chunk], rest};
    }

    constexpr auto flip(int const i) TCM_NOEXCEPT
    {
        TCM_ASSERT(0 <= i && i < size());
        auto const chunk = i / 8;
        auto const rest  = i % 8;
        flip_bit(_data.spin[chunk], rest);
    }

    constexpr auto flipped(std::initializer_list<int> is) const TCM_NOEXCEPT
        -> SpinVector
    {
        SpinVector temp{*this};
        for (auto i : is) {
            temp.flip(i);
        }
        return temp;
    }

    constexpr auto data() const noexcept -> std::byte const*
    {
        return _data.spin;
    }

    auto operator==(SpinVector const other) const noexcept -> bool
    {
        return _mm_movemask_epi8(_data.as_ints == other._data.as_ints)
               == 0xFFFF;
    }

    auto operator!=(SpinVector const other) const noexcept -> bool
    {
        return _mm_movemask_epi8(_data.as_ints == other._data.as_ints)
               != 0xFFFF;
    }

    auto hash() const noexcept -> std::size_t
    {
        static_assert(sizeof(_data.as_ints[0]) == sizeof(std::size_t));
        auto seed = static_cast<std::size_t>(_data.as_ints[0]);
        boost::hash_combine(seed, _data.as_ints[1]);
        return seed;
    }

  private:
#if defined(BOOST_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#elif defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#endif
    union {
        struct {
            std::byte     spin[14];
            std::uint16_t size;
        };
        __m128i as_ints;
    } _data;
    static_assert(sizeof(_data) == 16);
#if defined(BOOST_GCC)
#pragma GCC diagnostic pop
#elif defined(BOOST_CLANG)
#pragma clang diagnostic pop
#endif
};

// auto operator<<(std::ostream&, SpinVector const&) -> std::ostream&;
// auto operator>>(std::istream&, SpinVector&) -> std::istream&;
auto print(std::FILE*, SpinVector const&) -> void;
auto parse_spin(nonstd::span<char const>)
    -> std::pair<SpinVector, nonstd::span<char const>>;

