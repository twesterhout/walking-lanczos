#pragma once

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/exception/info.hpp>
#include <boost/exception/enable_error_info.hpp>
#include <boost/stacktrace/stacktrace.hpp>
#include <exception>
#include <iostream>
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
inline void assertion_failed_msg(char const* expr, char const* msg,
    char const* function, char const* /*file*/, long /*line*/)
{
    std::cerr << "Expression '" << expr << "' is false in function '"
              << function << "': " << (msg ? msg : "<...>") << ".\n"
              << "Backtrace:\n"
              << boost::stacktrace::stacktrace() << '\n';
    std::abort();
}

inline void assertion_failed(
    char const* expr, char const* function, char const* file, long line)
{
    ::boost::assertion_failed_msg(expr, 0 /*nullptr*/, function, file, line);
}
} // namespace boost
