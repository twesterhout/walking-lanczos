
#include "spin_chain.hpp"
#include "parser_utils.hpp"

#include <nonstd/span.hpp>

#include <cstdio>
#include <iostream>


auto print(std::FILE* const stream, SpinVector const& spin) -> void
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
    TCM_ASSERT(spin.size() <= SpinVector::max_size());
    spin_str[spin.size()] = '\0';
    for (auto i = 0; i < spin.size(); ++i) {
        spin_str[i] = to_char(spin[i]);
    }

    auto const status = std::fputs(spin_str, stream);
    if (status == EOF && std::ferror(stream)) {
        throw_with_trace(std::runtime_error{"std::fputs() failed"});
    }
}

#if 0
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

auto parse_spin(nonstd::span<char const> str)
    -> std::pair<SpinVector, nonstd::span<char const>>
{
    using index_type = nonstd::span<char const>::index_type;
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
    str = skip_spaces(str);
    Spin spin[SpinVector::max_size()];
    auto i = index_type{0};
    for (; i < static_cast<index_type>(std::size(spin)) && i < std::size(str)
           && !std::isspace(str[i]);
         ++i) {
        spin[i] = to_spin(str[i]);
    }
    // Overflow?
    if (i == static_cast<index_type>(std::size(spin)) && i < std::size(str)
        && !std::isspace(str[i])) {
        throw_with_trace(
            std::runtime_error{"Failed to parse a spin-1/2 configuration: "
                               "configurations longer than "
                               + std::to_string(SpinVector::max_size())
                               + " are not (yet) supported."});
    }
    return {SpinVector{std::begin(spin), std::begin(spin) + i}, str.subspan(i)};
}

#if 0
auto operator>>(std::istream& in, SpinVector& x) -> std::istream&
{
    std::string str;
    in >> str;
    auto const [spin, rest] = parse_spin({str});
    TCM_ASSERT(rest.empty());
    x = spin;
    return in;
}
#endif
