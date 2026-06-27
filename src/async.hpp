
#pragma once

#include <boost/asio.hpp>

namespace async {

template <typename T>
using Awaitable = boost::asio::awaitable<T>;

using Executor = boost::asio::any_io_executor;

// Indicates whether a component must be safe for concurrent access across
// multiple threads or strands.
enum class Concurrency : bool { Single = false, Multi = true };

} // namespace async
