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

#include <complex>
#include <functional>
#include <vector>

class SpinVector;
class QuantumState;
class QuantumStateBuilder;

using Hamiltonian = std::function<
    auto(SpinVector, std::complex<double>, QuantumStateBuilder&)->void>;

auto energy(Hamiltonian const&, QuantumState const&) -> std::complex<double>;

/// \brief Represents the Heisenberg Hamiltonian.
class Heisenberg {
  public:
    using edge_type = std::pair<int, int>;
    using spec_type = std::pair<std::complex<double>, std::vector<edge_type>>;

  private:
    std::vector<spec_type> _specs;

  public:
    Heisenberg() noexcept = default;

    /// Constructs a new operator given an adjacency list and the coupling.
    Heisenberg(
        std::vector<edge_type> edges, std::complex<double> const coupling = 1.0)
        : _specs{}
    {
        _specs.emplace_back(coupling, std::move(edges));
    }

    Heisenberg(std::vector<spec_type> specs) : _specs{std::move(specs)} {}

    /// Copy and Move constructors/assignments
    Heisenberg(Heisenberg const&)     = default;
    Heisenberg(Heisenberg&&) noexcept = default;
    Heisenberg& operator=(Heisenberg const&) = default;
    Heisenberg& operator=(Heisenberg&&) noexcept = default;

    /// Performs |ψ〉+= c * H|σ〉.
    auto operator()(
        SpinVector, std::complex<double>, QuantumStateBuilder&) const -> void;

    friend auto operator>>(std::istream&, Heisenberg&) -> std::istream&;
};

