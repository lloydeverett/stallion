cmake_minimum_required(VERSION 3.11)
project(MyFTXUIProject)

cmake_policy(SET CMP0156 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# boost
set(BOOST_BUILD_SHARED_LIBS OFF)
set(BOOST_INCLUDE_LIBRARIES process filesystem asio)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
  boost
  URL      https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.tar.xz
  URL_HASH SHA256=cc5dc5006ecbdf0051f90979be31b4eee5987d9ae14ae9fb9c03cfa43fa3cdad
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
)
FetchContent_MakeAvailable(boost)

# ftxui
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG 5b0046fd7222ed12ca3b9fb66e4ffd0900877717
)
FetchContent_MakeAvailable(ftxui)

add_executable(stallion main.cpp)
target_link_libraries(stallion PRIVATE ftxui::screen ftxui::dom ftxui::component)
target_link_libraries(stallion PRIVATE Boost::process Boost::filesystem Boost::asio)

