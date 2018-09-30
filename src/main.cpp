
#include "hamiltonian.hpp"
#include "diffusion.hpp"
#include "quantum_state.hpp"
#include "format.hpp"
#include <boost/exception/get_error_info.hpp>
#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <tbb/task_scheduler_init.h>
#include <iostream>
#include <fstream>
#include <optional>
#include <filesystem>

namespace po = boost::program_options;


using IStreamPtr = std::unique_ptr<std::istream, void(*)(std::istream*)>;
using OStreamPtr = std::unique_ptr<std::ostream, void(*)(std::ostream*)>;

auto parse_options(int argc, char** argv, IStreamPtr& input_file,
    OStreamPtr& output_file, std::string& hamiltonian_file_name, double& lambda,
    std::size_t& iterations, std::size_t& soft_max,
    boost::optional<std::size_t>& hard_max) -> bool
{
    std::string                  input_file_name;
    boost::optional<std::string> output_file_name;
    po::options_description cmdline_options{"Command-line options"};
    // clang-format off
    cmdline_options.add_options()
        ("help", "Produce help message.")
        ("input-file", po::value(&input_file_name)->required(),
            "The file containing the initial quantum state.")
        ("output-file,o", po::value(&output_file_name),
            "Where to save the final quantum state.")
        ("hamiltonian,H", po::value(&hamiltonian_file_name)->required(),
            "The file containing the Hamiltonian specification.")
        ("lambda,L", po::value(&lambda)->default_value(1.0), "Λ.")
        ("iterations,n", po::value(&iterations)->default_value(1.0),
            "Number of iterations.")
        ("max", po::value(&soft_max)->default_value(1000),
            "Soft max on the number of elements.")
        ("hard-max", po::value(&hard_max),
            "Hard max on the number of elements.")
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
}

int main(int argc, char** argv)
{
    try {
        tbb::task_scheduler_init     init(2);
        IStreamPtr                   input_file{nullptr, [](auto* p) {}};
        OStreamPtr                   output_file{nullptr, [](auto* p) {}};
        std::string                  hamiltonian_file_name;
        double                       lambda;
        std::size_t                  iterations;
        std::size_t                  soft_max;
        boost::optional<std::size_t> hard_max;

        auto const proceed = parse_options(argc, argv, input_file, output_file,
            hamiltonian_file_name, lambda, iterations, soft_max, hard_max);
        if (!proceed) {
            return EXIT_SUCCESS;
        }

        QuantumState state{soft_max, hard_max ? *hard_max : 2 * soft_max};
        *input_file >> state;
        auto const hamiltonian = read_hamiltonian(hamiltonian_file_name);
        auto const initial_energy = energy(hamiltonian, state);
        *output_file << "# Result of evaluating (Λ - H)ⁿ|ψ₀〉for\n"
                     << "# Λ = " << lambda << '\n'
                     << "# n = " << iterations << '\n'
                     << "# E₀ = 〈ψ₀|H|ψ₀〉= " << initial_energy << '\n';
        state = diffusion_loop(lambda, hamiltonian, state, iterations);
        auto const final_energy = energy(hamiltonian, state);
        *output_file << "# => E = " << final_energy << '\n' << state;
        return EXIT_SUCCESS;
    }
    catch (std::exception const& e) {
        auto const* st = boost::get_error_info<errinfo_backtrace>(e);
        std::cerr << "Error: " << e.what() << '\n';
        if (st != nullptr) {
            std::cerr << "Backtrace:\n" << *st << '\n';
        }
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "Error: " << "Unknown error occured." << '\n';
        return EXIT_FAILURE;
    }
}
