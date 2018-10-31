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
#include "random.hpp"
#include "random_sample.hpp"
#include "parser_utils.hpp"
#include <boost/math/special_functions/ulp.hpp>
#include <nonstd/span.hpp>
#include <numeric>

auto QuantumState::clear() -> void
{
    for (auto& table : _maps) {
        table.clear();
    }
}

auto QuantumState::insert(value_type&& x) -> std::pair<map_type::iterator, bool>
{
    return _maps[spin_to_index(x.first)].insert(std::move(x));
}

auto QuantumState::remove_least(std::size_t count) -> void
{
    using std::begin, std::end;
    using item_type  = std::pair<SpinVector, double>;
    using index_type = nonstd::span<item_type>::index_type;
    auto const size =
        std::accumulate(std::begin(_maps), std::end(_maps), std::size_t{0},
            [](auto const acc, auto const& x) { return acc + x.size(); });
    auto const items_buffer = std::make_unique<item_type[]>(size);
    auto const items        = nonstd::span<item_type>{
        items_buffer.get(), static_cast<index_type>(size)};
    {
        auto i = std::size_t{0};
        for (auto const& table : _maps) {
            for (auto const& x : table) {
                items[i++] = {x.first, std::norm(x.second)};
            }
        }
    }
    std::partial_sort(begin(items), begin(items) + count, end(items),
        [](auto const& x, auto const& y) { return x.second < y.second; });
    for (std::size_t i = 0; i < count; ++i) {
        auto& table = _maps.at(spin_to_index(items[i].first));
        TCM_ASSERT(table.count(items[i].first));
        table.erase(items[i].first);
    }
}

auto QuantumState::random_resample(std::size_t count, RandomGenerator& gen)
    -> void
{
    using std::begin, std::end;
    auto const size =
        std::accumulate(std::begin(_maps), std::end(_maps), std::size_t{0},
            [](auto const acc, auto const& x) { return acc + x.size(); });
    auto const items_buffer = std::make_unique<value_type[]>(size);
    auto const items = nonstd::span<value_type>{items_buffer.get(), size};
    auto const weights_buffer = std::make_unique<double[]>(size);
    auto const weights = nonstd::span<double>{weights_buffer.get(), size};
    {
        auto i = std::size_t{0};
        for (auto const& table : _maps) {
            for (auto const& x : table) {
                items[i++] = x;
            }
        }
    }
    std::transform(begin(items), end(items), begin(weights),
        [](auto const& x) { return std::norm(x.second); });

    for (auto& table : _maps) {
        table.clear();
    }
    WeightedDistribution dist{weights};
    for (auto i = std::size_t{0}; i < count; ++i) {
        auto const  index = dist(gen);
        auto const& x     = items[index];
        _maps[spin_to_index(x.first)].insert(x);
    }
}

[[gnu::noinline]] auto QuantumState::normalize() -> QuantumState&
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

[[gnu::noinline]] auto QuantumState::shrink() -> void
{
    if (_use_random_sampling) {
        random_resample(_soft_max_size, global_random_generator());
    }
    else {
        auto const size = std::accumulate(std::begin(_maps), std::end(_maps),
            0ul, [](auto const acc, auto const& x) { return acc + x.size(); });
        if (size > _soft_max_size) { remove_least(size - _soft_max_size); }
    }
}

auto print(std::FILE* const stream, QuantumState const& psi) -> void
{
    psi.for_each([stream](auto const& x) {
        print(stream, x.first);
        if (std::fprintf(
                stream, "\t%.10e\t%.10e\n", x.second.real(), x.second.imag())
            < 0) {
            throw_with_trace(std::runtime_error{
                "fprintf() failed when writing psi to std::FILE*"});
        }
    });
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

auto read_state(std::FILE* const stream, QuantumState& state) -> void
{
    for_each_line(stream, [&state](auto line) {
        if (line.empty() || line[0] == '#') return;
        SpinVector spin;
        double     real, imag;
        std::tie(spin, line) = parse_spin(line);
        std::tie(real, line) = parse_double(line);
        std::tie(imag, line) = parse_double(line);
        if (!state.insert({spin, std::complex{real, imag}}).second) {
            throw_with_trace(std::runtime_error{
                "Failed to parse |ψ₀〉: Duplicate basis elements."});
        }
    });
}


