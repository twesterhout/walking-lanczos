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

#include "spin_chain.hpp"
#include <iostream>
#include <vector>

inline auto operator<<(std::ostream& out, SpinVector const& x) -> std::ostream&
{
    auto const to_char = [](Spin const x) TCM_NOEXCEPT -> char {
        switch (x) {
        case Spin::down: return '0';
        case Spin::up: return '1';
        default: TCM_ASSERT(false); std::terminate();
        }
    };

    for (auto i = 0; i < x.size(); ++i) {
        out << to_char(x[i]);
    }
    return out;
}

inline auto operator>>(std::istream& in, SpinVector& x) -> std::istream&
{
    auto const to_spin = [&in](char const x) ->Spin
    {
        switch (x) {
        case '0': return Spin::down;
        case '1': return Spin::up;
        default:
            in.unget();
            in.setstate(std::ios_base::failbit);
            throw_with_trace(
                std::runtime_error{"Failed to parse a spin-1/2 configuration: "
                                   "Allowed spin values are {0, 1}, but got "
                                   + std::string{x}});
        }
    };
    std::vector<Spin> spin;
    spin.reserve(256);
    while (std::isspace(in.peek())) {
        in.get();
    }
    while (std::isdigit(in.peek())) {
        spin.push_back(to_spin(in.get()));
    }
    x = SpinVector{std::begin(spin), std::end(spin)};
    return in;
}

inline auto operator<<(std::ostream& out, QuantumState const& x) -> std::ostream&
{
    for (auto const& [spin, coeff] : x) {
        out << spin << '\t' << coeff.real() << '\t' << coeff.imag() << '\n';
    }
    return out;
}

inline auto operator>>(std::istream& is, QuantumState& x) -> std::istream&
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
