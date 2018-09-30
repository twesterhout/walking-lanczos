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

#include "quantum_state.hpp"
#include "format.hpp"
#include <boost/config/workaround.hpp>
#include <numeric>

auto QuantumState::operator+=(std::pair<std::complex<double>, SpinVector> element)
    -> QuantumState&
{
    auto [coeff, spin] = element;
    auto where         = find(spin);
    if (where == end()) {
#if BOOST_WORKAROUND(__clang_major__, BOOST_TESTED_AT(8))
        insert({spin, coeff});
#else
        emplace(spin, coeff);
#endif
        if (size() > _hard_max_size) {
            std::cerr << "WARNING! hard_max_size exceeded: performance will be "
                         "bad.\n";
            remove_least(size() - _hard_max_size);
        }
    }
    else {
        where->second += coeff;
    }
    return *this;
}

auto QuantumState::operator-=(std::pair<std::complex<double>, SpinVector> element)
    -> QuantumState&
{
    return (*this) += {-element.first, element.second};
}

auto QuantumState::remove_least(std::size_t count) -> void
{
    std::vector<base::iterator> elements;
    elements.reserve(size());
    for (auto it = begin(); it != end(); ++it) {
        elements.push_back(it);
    }
    std::sort(std::begin(elements), std::end(elements),
        [](auto const& x, auto const& y) {
            return std::norm(x->second) < std::norm(y->second);
        });
    for (std::size_t i = 0; i < count; ++i) {
        unsafe_erase(elements[i]);
    }
}

auto QuantumState::normalize() -> QuantumState&
{
    auto const norm  = std::accumulate(begin(), end(), 0.0,
        [](auto const acc, auto const x) { return acc + std::norm(x.second); });
    auto const scale = 1.0 / std::sqrt(norm);
    for (auto& [_, coeff] : *this) {
        coeff *= scale;
    }
    return *this;
}

auto QuantumState::shrink() -> void
{
    if (size() > _soft_max_size) {
        remove_least(size() - _soft_max_size);
    }
}

