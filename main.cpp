#include <stddef.h>
#include <ftxui/component/loop.hpp>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "dummy.hpp"

using namespace ftxui;

int main() {
  auto screen = App::Fullscreen();

  DummyRootView drv;

  auto main_renderer = drv.renderer();

  Loop loop(&screen, main_renderer);
  loop.Run();

  return 0;
}
