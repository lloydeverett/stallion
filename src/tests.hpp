#pragma once

#ifdef NDEBUG
#error "Tests should not be compiled with NDEBUG defined."
#endif

#include <cassert>
#include <vector>

inline std::vector<void (*)()> g_tests;
