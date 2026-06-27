#include <functional>
#include <string_view>

int expose_http_main(const char *address_str, unsigned short port,
                     const char *doc_root_str, int threads);

int tui_main();

int main(int argc, char *argv[]) {
  if (argc >= 2 && std::string_view(argv[1]) == "--server") {
    const int server_argc = 5;
    const char *server_argv[] = {"http-server-awaitable", "0.0.0.0", "8080",
                                 ".", "1"};
    return expose_http_main("0.0.0.0", 8080, ".", 1);
  }

  // --spawn-git 'dir=.' --expose-http '/'
  // maybe use HJSON
  // https://github.com/hjson/hjson-cpp
  // can turn into json
  // then parse with boost

  return tui_main();
}
