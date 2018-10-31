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
#include <clocale>
#include <cstdio>
#include <filesystem>
#include <optional>

namespace po = boost::program_options;

using FilePtr = std::unique_ptr<std::FILE, void (*)(std::FILE*)>;

namespace {
auto open_file(char const* filename, char const* mode) -> FilePtr
{
    auto fp = std::fopen(filename, mode);
    if (fp == nullptr) {
        if (errno != 0) {
            throw_with_trace(
                std::system_error{errno, std::generic_category(), filename});
        }
        else {
            throw_with_trace(std::runtime_error{
                "Failed to open '" + std::string{filename} + "'"});
        }
    }
    return {fp, [](auto* p) { std::fclose(p); }};
}

auto parse_options(int argc, char** argv, FilePtr& input_file,
    FilePtr& output_file, FilePtr& hamiltonian_file, double& lambda,
    std::size_t& iterations, std::size_t& soft_max,
    boost::optional<std::size_t>& hard_max, bool& use_random_sampling) -> bool
{
    std::string                  input_file_name;
    boost::optional<std::string> output_file_name;
    std::string                  hamiltonian_file_name;
    po::options_description      cmdline_options{"Command-line options"};
    use_random_sampling = false;
    // clang-format off
    cmdline_options.add_options()
        ("help", "Produce the help message.")
        ("input-file", po::value(&input_file_name)->required(),
            "File containing the initial quantum state. '-' can be used to "
            "indicate that the initial state should be read from the standard input.")
        ("output-file,o", po::value(&output_file_name),
            "File where to save the final quantum state. If not given the "
            "final state will be written to standard output.")
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
        ("random", po::value(&use_random_sampling)->implicit_value(true)->zero_tokens(),
            "Use random sampling.")
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
        std::ostringstream msg;
        msg << cmdline_options << '\n';
        auto const str = msg.str();
        std::fputs(str.c_str(), stdout);
        return false;
    }
    po::notify(vm);

    if (input_file_name == "-") {
        input_file = FilePtr{stdin, [](auto*) {}};
    }
    else {
        input_file = open_file(input_file_name.c_str(), "r");
    }

    if (!output_file_name) {
        output_file = FilePtr{stdout, [](auto*) {}};
    }
    // Issue #1: Prevent the user from overwriting the input file.
    else if (std::filesystem::exists({*output_file_name})
             && input_file_name != "-"
             && std::filesystem::equivalent(
                    {*output_file_name}, {input_file_name})) {
        throw std::runtime_error{"Input file '" + input_file_name
                                 + "' and output file '" + *output_file_name
                                 + "' are the same."};
    }
    else {
        output_file = open_file(output_file_name->c_str(), "w");
    }

    hamiltonian_file = open_file(hamiltonian_file_name.c_str(), "r");
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    try {
        std::setlocale(LC_ALL, ""); // WARNING: Don't remove me!
        FilePtr                      input_file{nullptr, [](auto*) {}};
        FilePtr                      output_file{nullptr, [](auto*) {}};
        FilePtr                      hamiltonian_file{nullptr, [](auto*) {}};
        double                       lambda;
        std::size_t                  iterations;
        std::size_t                  soft_max;
        boost::optional<std::size_t> hard_max;
        bool                         use_random_sampling;

        auto const proceed =
            parse_options(argc, argv, input_file, output_file, hamiltonian_file,
                lambda, iterations, soft_max, hard_max, use_random_sampling);
        if (!proceed) { return EXIT_SUCCESS; }

        QuantumState state{soft_max, hard_max ? *hard_max : 2 * soft_max, 1,
            use_random_sampling};
        read_state(input_file.get(), state);
        auto const hamiltonian    = read_hamiltonian(hamiltonian_file.get());

        auto const initial_energy = energy(hamiltonian, state);
        std::fprintf(output_file.get(),
            "# Result of evaluating (Λ - H)ⁿ|ψ₀〉for\n"
            "# Λ = %g\n"
            "# n = %zu\n"
            "# E₀ = 〈ψ₀|H|ψ₀〉= %lf + %.2ei\n",
            lambda, iterations, initial_energy.real(), initial_energy.imag());
        std::fflush(output_file.get());

        state = diffusion_loop(lambda, hamiltonian, state, iterations);

        auto const final_energy = energy(hamiltonian, state);
        std::fprintf(output_file.get(),
            "# => E = %lf + %.2ei\n",
            final_energy.real(), final_energy.imag());
        print(output_file.get(), state);
        return EXIT_SUCCESS;
    }
    catch (std::exception const& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        auto const* st = boost::get_error_info<errinfo_backtrace>(e);
        if (st != nullptr) {
            std::ostringstream msg;
            msg << *st << '\n';
            auto const str = msg.str();
            std::fprintf(stderr, "Backtrace:\n%s\n", str.c_str());
        }
        return EXIT_FAILURE;
    }
    catch (...) {
        std::fprintf(stderr, "Error: unknown error occured.\n");
        return EXIT_FAILURE;
    }
}
