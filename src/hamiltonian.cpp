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

auto Heisenberg::operator()(SpinVector spin, std::complex<double> coeff,
    QuantumStateBuilder& psi) const -> void
{
    for (auto const& [coupling, edges] : _specs) {
        for (auto const& [i, j] : edges) {
            auto const aligned = spin[i] == spin[j];
            auto const sign    = static_cast<double>(-1 + 2 * aligned);
            psi += {sign * coeff * coupling, spin};
            if (!aligned) {
                psi += {2.0 * coeff * coupling, spin.flipped({i, j})};
            }
        }
    }
}

auto energy(Hamiltonian const& hamiltonian, QuantumState const& psi)
    -> std::complex<double>
{
    QuantumState h_psi{psi.soft_max(), psi.hard_max(), psi.number_workers()};
    QuantumStateBuilder h_psi_builder{h_psi};

    h_psi_builder.start();
    psi.for_each([&h_psi_builder, &hamiltonian](auto const& x) {
        hamiltonian(x.first, x.second, h_psi_builder);
    });
    h_psi_builder.stop();

    std::complex<double> energy;
    psi.for_each([&energy, &h_psi](auto const& x) {
        auto const where = h_psi.find(x.first);
        if (where.has_value()) {
            energy += std::conj(x.second) * (*where)->second;
        }
    });
    return energy;
}

auto operator>>(std::istream& is, std::pair<int, int>& edge) -> std::istream&
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
    is >> std::ws >> edge.first >> std::ws >> ch;
    expect(',');
    is >> std::ws >> edge.second >> std::ws >> ch;
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
    }
    else {
        is >> t;
        x.push_back(std::move(t));
    }

    do {
        ch = static_cast<char>((is >> std::ws).peek());
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

