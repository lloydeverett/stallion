cmake_minimum_required(VERSION 3.11)
project(MyFTXUIProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 1. Fetch FTXUI from GitHub
include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG 5b0046fd7222ed12ca3b9fb66e4ffd0900877717
)
FetchContent_MakeAvailable(ftxui)

# 2. Define your executable
add_executable(stallion main.cpp)

# 3. Link FTXUI components
# Use ftxui::screen, ftxui::dom, and/or ftxui::component as needed
target_link_libraries(stallion PRIVATE ftxui::screen ftxui::dom ftxui::component)

