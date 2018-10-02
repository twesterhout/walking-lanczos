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
#include <numeric>

auto QuantumState::clear() -> void
{
    for (auto& table : _maps) {
        table.clear();
    }
}

auto QuantumState::insert(value_type&& x) -> std::pair<map_type::iterator, bool>
{
    return _maps[spin_to_index(x.first, _maps.size())].insert(std::move(x));
}

auto QuantumState::remove_least(std::size_t count) -> void
{
    _entries.clear();
    for (auto const& table : _maps) {
        for (auto const& x : table) {
            _entries.push_back({x.first, std::norm(x.second)});
        }
    }
    std::sort(std::begin(_entries), std::end(_entries),
        [](auto const& x, auto const& y) { return x.second < y.second; });
    for (std::size_t i = 0; i < count; ++i) {
        auto& table = _maps.at(spin_to_index(_entries[i].first, _maps.size()));
        TCM_ASSERT(table.count(_entries[i].first));
        table.erase(_entries[i].first);
    }
}

auto QuantumState::normalize() -> QuantumState&
{
    double norm = 0.0;
    for (auto const& table : _maps) {
        norm += std::accumulate(
            table.begin(), table.end(), 0.0, [](auto const acc, auto const& x) {
                return acc + std::norm(x.second);
            });
    }
    auto const scale = 1.0 / std::sqrt(norm);
    for (auto& table : _maps) {
        for (auto& [_, coeff] : table) {
            coeff *= scale;
        }
    }
    return *this;
}

auto QuantumState::shrink() -> void
{
    auto const size = std::accumulate(std::begin(_maps), std::end(_maps), 0ul,
        [](auto const acc, auto const& x) { return acc + x.size(); });
    if (size > _soft_max_size) { remove_least(size - _soft_max_size); }
}

auto operator<<(std::ostream& out, QuantumState const& psi) -> std::ostream&
{
    psi.for_each([&out](auto const& x) {
        out << x.first << '\t' << x.second.real() << '\t' << x.second.imag()
            << '\n';
    });
    return out;
}

auto operator>>(std::istream& is, QuantumState& x) -> std::istream&
{
    std::string line;
    SpinVector  spin;
    double      real, imag;
    x.clear();
    while (std::getline(is, line)) {
        if (!line.empty() && line.front() != '#') {
            std::istringstream line_stream{line};
            if ((line_stream >> spin >> real >> imag).fail()) {
                is.setstate(std::ios_base::failbit);
                throw_with_trace(std::runtime_error{"Failed to parse |ψ₀〉."});
            }
            if (!x.insert({spin, std::complex{real, imag}}).second) {
                is.setstate(std::ios_base::failbit);
                throw_with_trace(std::runtime_error{
                    "Failed to parse |ψ₀〉: Duplicate basis elements."});
            }
        }
    }
    if (!is.eof()) {
        is.setstate(std::ios_base::failbit);
        throw_with_trace(std::runtime_error{"Failed to parse |ψ₀〉."});
    }
    return is;
}
