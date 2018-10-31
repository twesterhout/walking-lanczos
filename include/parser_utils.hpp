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

#include "config.hpp"

#include <nonstd/span.hpp>

#include <cerrno>
#include <charconv>
#include <functional>

inline auto skip_spaces(nonstd::span<char const> const str) noexcept
    -> nonstd::span<char const>
{
    using index_type = nonstd::span<char const>::index_type;
    auto i           = index_type{0};
    for (; i < str.size() && std::isspace(str[i]); ++i)
        ;
    return str.subspan(i);
}

inline auto parse_char(char const c, nonstd::span<char const> const str)
    -> nonstd::span<char const>
{
    if (str.empty())
        throw_with_trace(
            std::runtime_error{"Expected '" + std::string{c}
                               + "', but reached the end of input."});
    if (str[0] != c)
        throw_with_trace(
            std::runtime_error{"Expected '" + std::string{c} + "', but got '"
                               + std::string{str[0]} + "'."});
    return str.subspan(1);
}

inline auto parse_int(nonstd::span<char const> str)
    -> std::tuple<int, nonstd::span<char const>>
{
    str = skip_spaces(str); // from_chars doesn't skip spaces
    int x;
    auto [end, status] =
        std::from_chars(str.data(), str.data() + str.size(), x);
    if (status == std::errc::invalid_argument) {
        auto const count = static_cast<std::size_t>(std::min(str.size(), 10l));
        throw_with_trace(
            std::runtime_error{"Expected an integer, but got \""
                               + std::string(str.data(), count) + "...\"."});
    }
    if (status == std::errc::result_out_of_range) {
        auto const count = static_cast<std::size_t>(end - str.data());
        throw_with_trace(std::runtime_error{
            "Encountered an overflow when parsing an integer from \""
            + std::string(str.data(), count) + "\"."});
    }
    // TCM_ASSERT(std::make_error_code(status) == std::error_code{});
    return {x, str.subspan(end - str.data())};
}

inline auto parse_double(nonstd::span<char const> str)
    -> std::tuple<double, nonstd::span<char const>>
{
#if 1
    // Unfortunately, g++ does not (yet) support std::from_chars for floating
    // point numbers and we have to use this very ugly alternative
    std::string buffer{std::begin(str), std::end(str)};
    char*       str_end = nullptr;
    auto const  x       = std::strtod(buffer.data(), &str_end);
    auto const  count   = str_end - buffer.data();
    if (errno == ERANGE) {
        errno = 0;
        throw_with_trace(std::runtime_error{
            "Encountered an overflow when parsing an integer from \""
            + std::string(buffer.data(), static_cast<std::size_t>(count))
            + "\"."});
    }
    if (x == 0 && count == 0) {
        throw_with_trace(std::runtime_error{
            "Expected a double, but got \""
            + std::string(buffer.data(), std::min(buffer.size(), 10ul))
            + "...\"."});
    }
    return {x, str.subspan(count)};
#else
    double x;
    auto [end, status] = std::from_chars(
        str.data(), str.data() + str.size(), x, std::chars_format::general);
    if (status == std::errc::invalid_argument) {
        auto const count = static_cast<std::size_t>(std::min(str.size(), 10l));
        throw_with_trace(
            std::runtime_error{"Expected a double, but got \""
                               + std::string(str.data(), count) + "...\"."});
    }
    if (status == std::errc::result_out_of_range) {
        auto const count = static_cast<std::size_t>(end - str.data());
        throw_with_trace(std::runtime_error{
            "Encountered an overflow when parsing a double from \""
            + std::string(str.data(), count) + "\"."});
    }
    TCM_ASSERT(std::make_error_code(status) == std::error_code{});
    return {x, str.subspan(end - str.data())};
#endif
}

// clang-format off
inline auto for_each_line(std::FILE* const stream,
    std::function<void(nonstd::span<char const>)> const func) -> void
// clang-format on
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

    while ((bytes_read = ::getline(&line_ptr, &line_length, stream)) != -1) {
        func(nonstd::span<char const>{line_ptr, bytes_read});
    }
    if (!std::feof(stream) && std::ferror(stream)) {
        throw_with_trace(std::system_error{
            errno, std::generic_category(), "for_each_line failed"});
    }
}

