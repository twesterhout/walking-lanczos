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
#include "hamiltonian.hpp"
#include "quantum_state.hpp"
#include <boost/exception/get_error_info.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

namespace po = boost::program_options;

using IStreamPtr = std::unique_ptr<std::istream, void (*)(std::istream*)>;
using OStreamPtr = std::unique_ptr<std::ostream, void (*)(std::ostream*)>;

namespace {
auto parse_options(int argc, char** argv, IStreamPtr& input_file,
    OStreamPtr& output_file, std::string& hamiltonian_file_name, double& lambda,
    std::size_t& iterations, std::size_t& soft_max,
    boost::optional<std::size_t>& hard_max, bool& use_random_sampling) -> bool
{
    std::string                  input_file_name;
    boost::optional<std::string> output_file_name;
    po::options_description      cmdline_options{"Command-line options"};
    // clang-format off
    cmdline_options.add_options()
        ("help", "Produce the help message.")
        ("input-file", po::value(&input_file_name)->required(),
            "File containing the initial quantum state. '-' can be used to "
            "indicate that the initial state should be read from the standard input.")
        ("output-file,o", po::value(&output_file_name),
            "Where to save the final quantum state.")
        ("hamiltonian,H", po::value(&hamiltonian_file_name)->required(),
            "The file containing the Hamiltonian specification.")
        ("lambda,L", po::value(&lambda)->default_value(1.0),
            "Value of Λ in the diffusion operator (H - Λ).")
        ("iterations,n", po::value(&iterations)->default_value(1.0),
            "Number of application of (H - Λ) to perform.")
        ("max", po::value(&soft_max)->default_value(1000),
            "Maximum number of elements to keep after each application of (H - Λ).")
        ("hard-max", po::value(&hard_max),
            "Initial number of buckets in the hash table. This parameter should "
            "be chosen carefully, because too low a value will result in a lot of "
            "rehashing, but too high a value will use more memory and result in "
            "more cache misses.")
        ("random", po::value(&use_random_sampling)->default_value(false),
            "Whether to use random sampling.")
    ;
    // clang-format on
    po::positional_options_description positional;
    positional.add("input-file", 1);

    po::variables_map vm;
    store(po::command_line_parser(argc, argv)
              .options(cmdline_options)
              .positional(positional)
              .run(),
        vm);
    if (vm.count("help")) {
        std::cout << cmdline_options << '\n';
        return false;
    }
    po::notify(vm);

    if (input_file_name == "-") {
        input_file = IStreamPtr{std::addressof(std::cin), [](auto*) {}};
    }
    else if (!std::filesystem::exists({input_file_name})) {
        throw std::runtime_error{
            "Input file '" + input_file_name + "' does not exist."};
    }
    else {
        input_file = IStreamPtr{new std::ifstream{input_file_name},
            [](auto* p) { std::default_delete<std::istream>{}(p); }};
        if (!*input_file) {
            throw std::runtime_error{
                "Could not open '" + input_file_name + "' for reading."};
        }
    }

    if (!output_file_name) {
        output_file = OStreamPtr{std::addressof(std::cout), [](auto*) {}};
    }
    else {
        // Issue #1: Prevent the user from overwriting the input file.
        if (std::filesystem::exists({*output_file_name})
            && input_file_name != "-"
            && std::filesystem::equivalent(
                   {*output_file_name}, {input_file_name})) {
            throw std::runtime_error{"Input file '" + input_file_name
                                     + "' and output file '" + *output_file_name
                                     + "' are the same."};
        }
        output_file = OStreamPtr{new std::ofstream{*output_file_name},
            [](auto* p) { std::default_delete<std::ostream>{}(p); }};
        if (!*output_file) {
            throw std::runtime_error{
                "Could not open '" + *output_file_name + "' for writing."};
        }
    }
    return true;
}

auto read_hamiltonian(std::string const& hamiltonian_file_name) -> Hamiltonian
{
    if (!std::filesystem::exists({hamiltonian_file_name})) {
        throw std::runtime_error{"Hamiltonian specification file '"
                                 + hamiltonian_file_name + "' does not exist."};
    }

    std::FILE* stream = fopen(hamiltonian_file_name.c_str(), "r");
    if (stream == nullptr) {
        std::exit(1);
    }
    return read_hamiltonian(stream);
#if 0
    std::ifstream hamiltonian_file{hamiltonian_file_name};
    if (!hamiltonian_file) {
        throw std::runtime_error{
            "Could not open '" + hamiltonian_file_name + "' for reading."};
    }

    Heisenberg hamiltonian;
    if (!(hamiltonian_file >> hamiltonian) && !hamiltonian_file.eof()) {
        throw std::runtime_error{"Failed to parse the Hamiltonian."};
    }
    return {std::move(hamiltonian)};
#endif
}
} // namespace

int main(int argc, char** argv)
{
    try {
        IStreamPtr                   input_file{nullptr, [](auto*) {}};
        OStreamPtr                   output_file{nullptr, [](auto*) {}};
        std::string                  hamiltonian_file_name;
        double                       lambda;
        std::size_t                  iterations;
        std::size_t                  soft_max;
        boost::optional<std::size_t> hard_max;
        bool                         use_random_sampling;

        auto const proceed = parse_options(argc, argv, input_file, output_file,
            hamiltonian_file_name, lambda, iterations, soft_max, hard_max,
            use_random_sampling);
        if (!proceed) { return EXIT_SUCCESS; }

        QuantumState state{soft_max, hard_max ? *hard_max : 2 * soft_max, 1,
            use_random_sampling};
        *input_file >> state;
        auto const hamiltonian    = read_hamiltonian(hamiltonian_file_name);
        auto const initial_energy = energy(hamiltonian, state);
        *output_file << "# Result of evaluating (Λ - H)ⁿ|ψ₀〉for\n"
                     << "# Λ = " << lambda << '\n'
                     << "# n = " << iterations << '\n'
                     << "# E₀ = 〈ψ₀|H|ψ₀〉= " << initial_energy << '\n';
        state = diffusion_loop(lambda, hamiltonian, state, iterations);
        auto const final_energy = energy(hamiltonian, state);
        *output_file << "# => E = " << final_energy << '\n' << state;

        print(stdout, state);
        return EXIT_SUCCESS;
    }
    catch (std::exception const& e) {
        auto const* st = boost::get_error_info<errinfo_backtrace>(e);
        std::cerr << "Error: " << e.what() << '\n';
        if (st != nullptr) { std::cerr << "Backtrace:\n" << *st << '\n'; }
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "Error: "
                  << "Unknown error occured." << '\n';
        return EXIT_FAILURE;
    }
}
