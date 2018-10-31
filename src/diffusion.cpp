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

#include "diffusion.hpp"
#include "quantum_state.hpp"

auto trotter_step(double const lambda, Hamiltonian const& hamiltonian,
    QuantumState const& psi) -> QuantumState
{
    QuantumState        h_psi{psi.soft_max(), psi.estimate_hard_max(),
        psi.number_workers(), psi.uses_random_sampling()};
    QuantumStateBuilder h_psi_builder{h_psi};

    h_psi_builder.start();
    psi.for_each([&h_psi_builder, &hamiltonian, lambda](auto const& x) {
        auto const [spin, coeff] = x;
        hamiltonian(spin, -coeff, h_psi_builder);
        h_psi_builder += {coeff * lambda, spin};
    });
    h_psi_builder.stop();
    return h_psi;
}

auto diffusion_loop(double const lambda, Hamiltonian const& hamiltonian,
    QuantumState const& psi, std::size_t const iterations) -> QuantumState
{
    if (iterations == 0) {
        throw_with_trace(
            std::runtime_error{"Number of iterations must be positive!"});
    }
    std::fprintf(stdout, "[1/%zu]", iterations);
    std::fflush(stdout);
    QuantumState state = trotter_step(lambda, hamiltonian, psi);
    state.shrink();
    state.normalize();
    for (auto i = 1ul; i < iterations; ++i) {
        std::fprintf(stdout, "\r[%zu/%zu]", (i + 1), iterations);
        std::fflush(stdout);
        state = trotter_step(lambda, hamiltonian, state);
        state.shrink();
        state.normalize();
    }
    std::fprintf(stdout, "\n");
    return state;
}

