#pragma once

#include <boost/cobalt.hpp>

#include "asio-st-ambient-executor.hpp"   /* IWYU pragma: keep */
#include "asio-st-initializer.hpp"        /* IWYU pragma: keep */

template <typename T>
using Promise = boost::cobalt::promise<T>;

