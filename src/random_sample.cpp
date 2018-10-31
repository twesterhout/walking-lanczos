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

#include "random_sample.hpp"
#include <algorithm>
#include <numeric>
#include <vector>

namespace {
auto normalise_weights(nonstd::span<double> weights)
{
    auto const sum = std::accumulate(
        std::begin(weights), std::end(weights), 0.0L, std::plus<void>{});
    if (sum == 0) {
        throw_with_trace(
            std::runtime_error{"Failed to normalise: all weights are zero."});
    }
    auto const scale = static_cast<long double>(weights.size()) / sum;
    for (auto& w : weights) {
        w *= scale;
    }
}
} // namespace

namespace {
auto partition_indices(
    nonstd::span<std::size_t> indices, nonstd::span<double const> weights)
    -> std::pair<nonstd::span<std::size_t>, nonstd::span<std::size_t>>
{
    using std::begin, std::end;
    using index_type  = nonstd::span<double const>::index_type;
    auto const middle = std::partition(
        begin(indices), end(indices), [&weights](auto const i) noexcept->bool {
            return weights[static_cast<index_type>(i)] < 1.0;
        });
    auto const small_length = middle - begin(indices);
    auto const small = nonstd::span<std::size_t>{indices.data(), small_length};
    auto const large = nonstd::span<std::size_t>{
        indices.data() + small_length, indices.data() + indices.size()};
    return {small, large};
}
} // namespace

auto WeightedDistribution::init(nonstd::span<double> const weights) -> void
{
    using std::begin, std::end;
    using index_type           = nonstd::span<double>::index_type;
    auto const               n = static_cast<std::size_t>(weights.size());
    std::vector<std::size_t> indices(n);
    std::vector<double>      prob(n);
    std::vector<std::size_t> alias(n);

    normalise_weights(weights);
    std::iota(begin(indices), end(indices), std::size_t{0});
    auto const [small, large] = partition_indices({indices}, weights);

    auto i_small = small.begin();
    auto i_large = large.begin();
    while (i_small != end(small) && i_large != end(large)) {
        auto const low  = *i_small;
        auto const high = *i_large;
        prob[low]       = weights[static_cast<index_type>(low)];
        alias[low]      = high;
        weights[static_cast<index_type>(high)] =
            (weights[static_cast<index_type>(low)]
                + weights[static_cast<index_type>(high)])
            - 1.0;
        if (weights[static_cast<index_type>(high)] < 1.0) {
            *i_small = high;
            ++i_large;
        }
        else {
            ++i_small;
        }
    }
    for (; i_large != std::end(large); ++i_large) {
        prob[*i_large]  = 1.0;
        alias[*i_large] = std::numeric_limits<std::size_t>::max();
    }
    // can only happen through numeric instability
    for (; i_small != std::end(small); ++i_small) {
        prob[*i_small]  = 1.0;
        alias[*i_small] = std::numeric_limits<std::size_t>::max();
    }

    _probabily = std::move(prob);
    _alias     = std::move(alias);
}

