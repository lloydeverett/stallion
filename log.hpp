#pragma once

#include <iostream>
#include <fstream>

#ifndef NDEBUG
inline std::ofstream log_file("stallion.log", std::ios_base::app);
#else
inline std::ofstream log_file(nullptr);
#endif

