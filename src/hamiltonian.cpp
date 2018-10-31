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
#include "parser_utils.hpp"
#include "quantum_state.hpp"
#include <nonstd/span.hpp>

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
    QuantumState        h_psi{psi.soft_max(), psi.estimate_hard_max(),
        psi.number_workers(), psi.uses_random_sampling()};
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

#if 0
inline auto operator>>(std::istream& is, std::pair<int, int>& edge)
    -> std::istream&
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
#endif


namespace {
auto parse_edge(nonstd::span<char const> str)
    -> std::tuple<std::pair<int, int>, nonstd::span<char const>>
{
    std::pair<int, int> edge;
    str                        = parse_char('(', skip_spaces(str));
    std::tie(edge.first, str)  = parse_int(str);
    str                        = parse_char(',', skip_spaces(str));
    std::tie(edge.second, str) = parse_int(str);
    str                        = parse_char(')', skip_spaces(str));
    return {edge, str};
}

auto parse_adjacency_list(nonstd::span<char const> str)
    -> std::tuple<std::vector<std::pair<int, int>>, nonstd::span<char const>>
{
    using index_type = nonstd::span<char const>::index_type;

    str = skip_spaces(parse_char('[', skip_spaces(str)));
    if (str.empty()) {
        throw_with_trace(std::runtime_error{"Missing the closing ']'."});
    }
    if (str[0] == ']') { return {{}, str.subspan(1)}; }

    std::pair<int, int>              edge;
    std::vector<std::pair<int, int>> edges;

    std::tie(edge, str) = parse_edge(str);
    edges.push_back(edge);
    do {
        str = skip_spaces(str);
        if (str.empty()) {
            throw_with_trace(std::runtime_error{"Missing the closing ']'."});
        }
        else if (str[0] == ']') {
            str = str.subspan(1);
            break;
        }
        else if (str[0] == ',') {
            std::tie(edge, str) = parse_edge(str.subspan(1));
            edges.push_back(edge);
        }
        else {
            throw_with_trace(std::runtime_error{
                "Expected ',' or ']', but got '" + std::string{str[0]} + "'."});
        }
    } while (true);
    return {std::move(edges), str};
}
} // namespace

#if 0
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
#endif

auto read_hamiltonian(std::FILE* const stream) -> Heisenberg
{
    using Specs = std::vector<Heisenberg::spec_type>;
    using Edges = std::vector<Heisenberg::edge_type>;

    Specs specs;
    for_each_line(stream, [&specs](auto line) {
        if (!line.empty() && line[0] != '#') {
            Edges  edges;
            double coupling;
            std::tie(coupling, line) = parse_double(line);
            std::tie(edges, line)    = parse_adjacency_list(line);
            specs.emplace_back(coupling, std::move(edges));
        }
    });
    return Heisenberg{std::move(specs)};
}

#if 0
auto read_hamiltonian(std::FILE* const stream) -> Heisenberg
{
    char*          line_ptr    = nullptr;
    std::size_t    line_length = 0;
    std::ptrdiff_t bytes_read  = 0;
    struct free_line_ptr {
        char*& _p;

        ~free_line_ptr()
        {
            if (_p != nullptr) { ::free(_p); }
        }
    } _dummy{line_ptr};

    std::vector<Heisenberg::spec_type> specs;
    while ((bytes_read = ::getline(&line_ptr, &line_length, stream)) != -1) {
        nonstd::span<char const>           line{line_ptr, bytes_read};
        if (!line.empty() && line[0] != '#') {
            std::vector<Heisenberg::edge_type> edges;
            double                             coupling;
            std::tie(coupling, line) = parse_double(line);
            std::tie(edges, line)    = parse_adjacency_list(line);
            specs.emplace_back(coupling, std::move(edges));
        }
    }
    if (!std::feof(stream) && std::ferror(stream)) {
        throw_with_trace(std::system_error{errno, std::generic_category(),
            "Failed to read the Heisenberg Hamiltonian."});
    }
    return Heisenberg{std::move(specs)};
}
#endif

#if 0
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
#endif

