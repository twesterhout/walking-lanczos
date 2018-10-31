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
#include <nonstd/span.hpp>
#include <random>
#include <vector>

class WeightedDistribution {
  public:
    WeightedDistribution(nonstd::span<double> weights) : _probabily{}, _alias{}
    {
        init(weights);
    }

    // clang-format off
    WeightedDistribution(WeightedDistribution const&) noexcept = delete;
    WeightedDistribution(WeightedDistribution&&) noexcept = default;
    WeightedDistribution& operator=(WeightedDistribution const&) noexcept = delete;
    WeightedDistribution& operator=(WeightedDistribution&&) noexcept = default;
    // clang-format on

    template <class Generator>
    BOOST_FORCEINLINE auto operator()(Generator& gen) const -> std::size_t
    {
        using RealDist   = std::uniform_real_distribution<double>;
        using RealParams = RealDist::param_type;
        using IntDist    = std::uniform_int_distribution<std::size_t>;
        using IntParams  = IntDist::param_type;

        RealDist   urd;
        IntDist    uid;
        auto const n      = _probabily.size();
        auto const index  = uid(gen, IntParams{0, n - 1});
        auto const choice = urd(gen, RealParams{0.0, 1.0});
        return (choice < _probabily[index]) ? index : _alias[index];
    }

  private:
    auto init(nonstd::span<double>) -> void;

    std::vector<double>      _probabily;
    std::vector<std::size_t> _alias;
};

