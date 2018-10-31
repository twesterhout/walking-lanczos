
#include "spin_chain.hpp"
#include <cstdio>
#include <iostream>

#if 1
auto operator<<(std::ostream& out, SpinVector const& x) -> std::ostream&
{
    auto const to_char = [](Spin const s) TCM_NOEXCEPT -> char {
        switch (s) {
        case Spin::down: return '0';
        case Spin::up: return '1';
#if defined(BOOST_GCC)
        // GCC fails to notice that all cases have already been handled.
        default: TCM_ASSERT(false); std::terminate();
#endif
        }
    };

    for (auto i = 0; i < x.size(); ++i) {
        out << to_char(x[i]);
    }
    return out;
}
#endif

[[nodiscard]] auto fput_spin(
    SpinVector const& spin, std::FILE* const stream) noexcept -> int
{
    auto const to_char = [](Spin const x) TCM_NOEXCEPT -> char {
        switch (x) {
        case Spin::down: return '0';
        case Spin::up: return '1';
#if defined(BOOST_GCC)
        // GCC fails to notice that all cases have already been handled.
        default: TCM_ASSERT(false); std::terminate();
#endif
        }
    };

    char spin_str[SpinVector::max_size() + 1];
    for (auto i = 0; i < spin.size(); ++i) {
        spin_str[i] = to_char(spin[i]);
    }
    spin_str[std::size(spin_str) - 1] = '\0';

    return std::fputs(spin_str, stream);
}

auto strtospin(char* const str, char** const str_end) -> SpinVector
{
    auto const to_spin = [](char const ch) -> Spin {
        switch (ch) {
        case '0': return Spin::down;
        case '1': return Spin::up;
        default:
            throw_with_trace(
                std::runtime_error{"Failed to parse a spin-1/2 configuration: "
                                   "allowed spin values are {0, 1}, but got "
                                   + std::string{ch}});
        }
    };

    auto i = std::size_t{0};
    // Skipping spaces
    while (str[i] != '\0' && std::isspace(str[i]))
        ++i;
    if (str[i] == '\0') {
        throw_with_trace(std::runtime_error{
            "Failed to parse a spin-1/2 configuration: expected a spin "
            "configuration, but got a bunch of spaces."});
    }
    // Parsing the actual configuration
    Spin spin[SpinVector::max_size()];
    auto spin_index = std::size_t{0};
    for (; spin_index < std::size(spin) && str[i] != '\0'
           && !std::isspace(str[i]);
         ++i, ++spin_index) {
        spin[spin_index] = to_spin(str[i]);
    }
    // Overflow?
    if (spin_index == std::size(spin) && str[i] != '\0' && !std::isspace(str[i])) {
        throw_with_trace(
            std::runtime_error{"Failed to parse a spin-1/2 configuration: "
                               "configurations longer than "
                               + std::to_string(SpinVector::max_size())
                               + " are not (yet) supported."});
    }

    if (str_end != nullptr) { *str_end = str + i; }
    return SpinVector{std::begin(spin), std::end(spin)};
}

#if 1
auto operator>>(std::istream& in, SpinVector& x) -> std::istream&
{
    auto const to_spin = [&in](char const ch) -> Spin {
        switch (ch) {
        case '0': return Spin::down;
        case '1': return Spin::up;
        default:
            in.unget();
            in.setstate(std::ios_base::failbit);
            throw_with_trace(
                std::runtime_error{"Failed to parse a spin-1/2 configuration: "
                                   "Allowed spin values are {0, 1}, but got "
                                   + std::string{ch}});
        }
    };
    std::vector<Spin> spin;
    spin.reserve(256);
    while (std::isspace(in.peek())) {
        in.get();
    }
    while (std::isdigit(in.peek())) {
        spin.push_back(to_spin(static_cast<char>(in.get())));
    }
    x = SpinVector{std::begin(spin), std::end(spin)};
    return in;
}
#endif
