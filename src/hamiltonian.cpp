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

#include "hamiltonian.hpp"
#include "quantum_state.hpp"
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>

auto Heisenberg::operator()(SpinVector spin, std::complex<double> coeff,
    QuantumState& psi) const -> void
{
    // Returns a function that, given an edge, applies the part of H that
    // corresponds to that edge to psi.
    auto make_single_edge_kernel = [coeff, spin, &psi](auto const coupling) {
        return [spin, &psi, coeff, coupling](edge_type const edge) -> void {
            auto const [i, j]  = edge;
            auto const aligned = spin[i] == spin[j];
            auto const sign    = static_cast<double>(-1 + 2 * aligned);
            psi += {sign * coeff * coupling, spin};
            if (!aligned) {
                psi += {2.0 * coeff * coupling, spin.flipped({i, j})};
            }
        };
    };

    tbb::parallel_for(
        0ul, _specs.size(), [this, make_single_edge_kernel](auto const i) {
            auto const  hopping = std::get<0>(_specs[i]);
            auto const& edges   = std::get<1>(_specs[i]);
            std::for_each(std::cbegin(edges), std::cend(edges),
                make_single_edge_kernel(hopping));
        });
}

auto energy(Hamiltonian const& hamiltonian, QuantumState const& psi)
    -> std::complex<double>
{
    QuantumState h_psi{psi.soft_max(), psi.hard_max()};
    tbb::parallel_for(
        psi.range(), [&h_psi, &hamiltonian](auto const& elements) {
            for (auto [spin, coeff] : elements) {
                hamiltonian(spin, coeff, h_psi);
            }
        });

    struct Energy {
        constexpr Energy(QuantumState const& psi) noexcept
            : _psi{psi}, _energy{0}
        {
        }
        constexpr Energy(Energy const& other, tbb::split /*unused*/) noexcept
            : _psi{other._psi}, _energy{0}
        {
        }
        auto operator()(QuantumState::const_range_type& range) -> void
        {
            for (auto [spin, coeff] : range) {
                auto const where = _psi.find(spin);
                if (where != _psi.end()) {
                    _energy += std::conj(where->second) * coeff;
                }
            }
        }
        constexpr auto join(Energy const& other) noexcept
        {
            _energy += other._energy;
        }
        constexpr auto get() const noexcept { return _energy; }

      private:
        QuantumState const&  _psi;
        std::complex<double> _energy;
    };

    Energy energy{psi};
    tbb::parallel_reduce(h_psi.range(), energy);
    return energy.get();
}

auto operator>>(std::istream& is, std::pair<int, int>& x) -> std::istream&
{
    char       ch;
    auto const expect = [&ch, &is](auto const x) {
        if (ch != x) {
            is.setstate(std::ios_base::failbit);
            throw_with_trace(std::runtime_error{
                "Failed to parse an edge: expected '" + std::string{x}
                + "', but got '" + std::string{ch} + "'"});
        }
    };
    is >> ch;
    expect('(');
    is >> std::ws >> x.first >> std::ws >> ch;
    expect(',');
    is >> std::ws >> x.second >> std::ws >> ch;
    expect(')');
    return is;
}

template <class T>
auto operator>>(std::istream& is, std::vector<T>& x) -> std::istream&
{
    char ch;
    T    t;
    is >> ch;
    if (ch != '[') {
        is.setstate(std::ios_base::failbit);
        throw_with_trace(std::runtime_error{
            "Failed to parse an adjacency list: expected '[', but got '"
            + std::string{ch} + "'"});
    }
    if ((is >> std::ws).peek() == ']') {
        is.get();
        x.clear();
        return is;
    } else {
        is >> t;
        x.push_back(std::move(t));
    }

    do {
        ch = (is >> std::ws).peek();
        if (ch == ']') { is.get(); }
        else if (ch == ',') {
            is.get();
            is >> std::ws >> t;
            x.push_back(std::move(t));
        }
        else {
            is.setstate(std::ios_base::failbit);
            throw_with_trace(
                std::runtime_error{"Failed to parse an adjacency list: "
                                   "expected ',' or ']', but got '"
                                   + std::string{ch} + "'"});
        }
    } while (ch != ']');
    return is;
}

auto operator>>(std::istream& is, Heisenberg& x) -> std::istream&
{
    std::string                        line;
    double                             coupling;
    std::vector<Heisenberg::edge_type> edges;
    std::vector<Heisenberg::spec_type> specs;
    while (std::getline(is, line)) {
        if (!line.empty() && line.front() != '#') {
            std::istringstream line_stream{line};
            if ((line_stream >> coupling >> edges).fail()) {
                is.setstate(std::ios_base::failbit);
                return is;
            }
            specs.emplace_back(coupling, std::move(edges));
        }
    }
    x._specs = std::move(specs);
    return is;
}

