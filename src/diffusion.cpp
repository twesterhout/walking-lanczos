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

#include <chrono>
#include <optional>

#if !(_POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _POSIX_SOURCE)
#error "fileno is a POSIX-specific thing"
#else
#include <stdio.h>
#include <unistd.h>
#endif

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

namespace {
auto make_status_updater(std::FILE* const stream) -> std::function<void(
    std::size_t, std::size_t, std::optional<std::chrono::seconds>)>
{
    int fd = ::fileno(stream);
    if (fd == -1) {
        auto const status = std::error_code{errno, std::generic_category()};
        errno             = 0;
        throw_with_trace(std::system_error{status, "fileno(FILE*)"});
    }
    if (::isatty(fd)) {
        // Is a terminal
        return [stream](auto const i, auto const n, auto const eta) {
            if (eta.has_value()) {
                std::fprintf(stream, "\r[%zu/%zu] ETA: %li:%li                ",
                    i, n, eta->count() / 60l, eta->count() % 60l);
            }
            else {
                std::fprintf(
                    stream, "\r[%zu/%zu]                             ", i, n);
            }
            std::fflush(stream);
        };
    }
    else if (errno == EINVAL || errno == ENOTTY) {
        // Is a file or pipe
        return [stream](auto const i, auto const n, auto const eta) {
            if (eta.has_value()) {
                std::fprintf(stream, "[%zu/%zu] ETA: %li:%li\n", i, n,
                    eta->count() / 60, eta->count() % 60);
            }
            else {
                std::fprintf(stream, "[%zu/%zu]\n", i, n);
            }
            std::fflush(stream);
        };
    }
    else { // invalid file descriptor: should never happen
        auto const status = std::error_code{errno, std::generic_category()};
        throw_with_trace(std::system_error{status, "isatty(int)"});
    }
}
} // namespace

auto diffusion_loop(double const lambda, Hamiltonian const& hamiltonian,
    QuantumState const& psi, std::size_t const iterations) -> QuantumState
{
    if (iterations == 0) {
        throw_with_trace(
            std::runtime_error{"Number of iterations must be positive!"});
    }
    auto const status_updater = make_status_updater(stdout);
    auto start = std::chrono::steady_clock::now();
    status_updater(1, iterations, std::nullopt);
    QuantumState state = trotter_step(lambda, hamiltonian, psi);
    state.shrink();
    state.normalize();
    auto stop = std::chrono::steady_clock::now();
    auto max = stop - start;
    for (auto i = 1ul; i < iterations; ++i) {
        auto const eta = std::chrono::duration_cast<std::chrono::seconds>(
            (iterations - i) * max);
        start = std::chrono::steady_clock::now();
        status_updater(i + 1, iterations, eta);
        state = trotter_step(lambda, hamiltonian, state);
        state.shrink();
        state.normalize();
        stop = std::chrono::steady_clock::now();
        if (stop - start > max) { max = stop - start; }
    }
    std::fprintf(stdout, "\n");
    return state;
}

