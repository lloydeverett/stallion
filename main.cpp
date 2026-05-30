#include <thread>

#include <boost/asio.hpp>

#include <ftxui/component/loop.hpp>
#include "ftxui/component/app.hpp"
#include "ftxui/component/event.hpp"

#include "dummy.hpp"

using namespace ftxui;

int main() {
  auto screen = App::Fullscreen();

  boost::asio::io_context io_ctx;

  auto work_guard = boost::asio::make_work_guard(io_ctx);

  DummyRootView root_view { screen, io_ctx.get_executor() };

  auto main_renderer = root_view.renderer();

  std::thread worker_thread([&io_ctx]() {
      io_ctx.run();
  });

  Loop loop(&screen, main_renderer);
  loop.Run();

  io_ctx.stop(); // force exit; work_guard.reset() would wait for unfinished handlers

  if (worker_thread.joinable()) {
      worker_thread.join();
  }

  return 0;
}
