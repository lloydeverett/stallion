#include "tests.hpp"

int main() {
  for (auto test : g_tests) {
    test();
  }
  return 0;
}
