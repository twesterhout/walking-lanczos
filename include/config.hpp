#pragma once

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/exception/info.hpp>
#include <boost/exception/enable_error_info.hpp>
#include <boost/stacktrace.hpp>
#include <exception>
#include <type_traits>

#define TCM_ASSERT BOOST_ASSERT

#if defined(BOOST_ENABLE_ASSERT_HANDLER)
#define TCM_NOEXCEPT
#else
#define TCM_NOEXCEPT noexcept
#endif

using errinfo_backtrace = boost::error_info<struct errinfo_backtrace_tag,
    boost::stacktrace::stacktrace>;

template <class E, class = std::enable_if_t<std::is_base_of_v<std::exception,
                       std::remove_cv_t<std::remove_reference_t<E>>>>>
[[noreturn]] auto throw_with_trace(E&& e)
{
    // NOLINTNEXTLINE(hicpp-exception-baseclass)
    throw (boost::enable_error_info(std::forward<E>(e))
        << errinfo_backtrace(boost::stacktrace::stacktrace()));
}

namespace boost {
#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif
inline void assertion_failed_msg(char const* expr, char const* msg,
    char const* function, char const* file, long line)
{
    std::fprintf(stderr,
        "Expression '%s' is false in function '%s': %s: %li: %s\n", expr,
        function, file, line, (msg ? msg : "<...>"));
    std::terminate();
}
#if defined(BOOST_CLANG)
#pragma clang diagnostic pop
#endif

inline void assertion_failed(
    char const* expr, char const* function, char const* file, long line)
{
    ::boost::assertion_failed_msg(expr, nullptr, function, file, line);
}
} // namespace boost
