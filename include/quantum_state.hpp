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

#include "spin_chain.hpp"
#include <complex>
#include <vector>

#if defined(BOOST_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <tbb/concurrent_unordered_map.h>
#if defined(BOOST_GCC)
#pragma GCC diagnostic pop
#endif

struct SpinHasher {
    auto operator()(SpinVector x) const noexcept { return x.hash(); }
};

class QuantumState
    : public tbb::concurrent_unordered_map<SpinVector, std::complex<double>,
          SpinHasher> {

    using base = tbb::concurrent_unordered_map<SpinVector, std::complex<double>,
        SpinHasher>;

    std::size_t _soft_max_size;
    std::size_t _hard_max_size;

  public:
    using base::const_range_type;
    using base::range_type;

    QuantumState(std::size_t soft_max)
        : base{}, _soft_max_size{soft_max}, _hard_max_size{2 * soft_max}
    {
    }

    QuantumState(std::size_t soft_max, std::size_t hard_max)
        : base{}, _soft_max_size{soft_max}, _hard_max_size{hard_max}
    {
    }

    QuantumState(QuantumState const&) = default;
    QuantumState(QuantumState&&)      = default;
    QuantumState& operator=(QuantumState const&) = default;
    QuantumState& operator=(QuantumState&&) = default;

    auto operator+=(std::pair<std::complex<double>, SpinVector>)
        -> QuantumState&;
    auto operator-=(std::pair<std::complex<double>, SpinVector>)
        -> QuantumState&;

    auto shrink() -> void;
    auto normalize() -> QuantumState&;

    constexpr auto soft_max() const noexcept { return _soft_max_size; }
    constexpr auto hard_max() const noexcept { return _hard_max_size; }

  private:
    auto remove_least(std::size_t count) -> void;
};

