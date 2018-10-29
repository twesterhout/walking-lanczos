
#include "spin_chain.hpp"
#include <iostream>

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

