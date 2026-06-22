#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "log.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;

// Return a reasonable mime type based on the extension of a file.
beast::string_view mime_type(beast::string_view path) {
  using beast::iequals;
  auto const ext = [&path] {
    auto const pos = path.rfind(".");
    if (pos == beast::string_view::npos)
      return beast::string_view{};
    return path.substr(pos);
  }();
  if (iequals(ext, ".htm"))
    return "text/html";
  if (iequals(ext, ".html"))
    return "text/html";
  if (iequals(ext, ".php"))
    return "text/html";
  if (iequals(ext, ".css"))
    return "text/css";
  if (iequals(ext, ".txt"))
    return "text/plain";
  if (iequals(ext, ".js"))
    return "application/javascript";
  if (iequals(ext, ".json"))
    return "application/json";
  if (iequals(ext, ".xml"))
    return "application/xml";
  if (iequals(ext, ".swf"))
    return "application/x-shockwave-flash";
  if (iequals(ext, ".flv"))
    return "video/x-flv";
  if (iequals(ext, ".png"))
    return "image/png";
  if (iequals(ext, ".jpe"))
    return "image/jpeg";
  if (iequals(ext, ".jpeg"))
    return "image/jpeg";
  if (iequals(ext, ".jpg"))
    return "image/jpeg";
  if (iequals(ext, ".gif"))
    return "image/gif";
  if (iequals(ext, ".bmp"))
    return "image/bmp";
  if (iequals(ext, ".ico"))
    return "image/vnd.microsoft.icon";
  if (iequals(ext, ".tiff"))
    return "image/tiff";
  if (iequals(ext, ".tif"))
    return "image/tiff";
  if (iequals(ext, ".svg"))
    return "image/svg+xml";
  if (iequals(ext, ".svgz"))
    return "image/svg+xml";
  return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string path_cat(beast::string_view base, beast::string_view path) {
  if (base.empty())
    return std::string(path);
  std::string result(base);
#ifdef BOOST_MSVC
  char constexpr path_separator = '\\';
  if (result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
  for (auto &c : result)
    if (c == '/')
      c = path_separator;
#else
  char constexpr path_separator = '/';
  if (result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
#endif
  return result;
}

template <class Body, class Allocator>
http::message_generator
handle_request(beast::string_view doc_root,
               http::request<Body, http::basic_fields<Allocator>> &&req) {
  auto const bad_request = [&req](beast::string_view why) {
    http::response<http::string_body> res{http::status::bad_request,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::string(why);
    res.prepare_payload();
    return res;
  };

  auto const not_found = [&req](beast::string_view target) {
    http::response<http::string_body> res{http::status::not_found,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + std::string(target) + "' was not found.";
    res.prepare_payload();
    return res;
  };

  auto const server_error = [&req](beast::string_view what) {
    http::response<http::string_body> res{http::status::internal_server_error,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "An error occurred: '" + std::string(what) + "'";
    res.prepare_payload();
    return res;
  };

  if (req.method() != http::verb::get && req.method() != http::verb::head) {
    return bad_request("Unknown HTTP-method");
  }

  if (req.target().empty() || req.target()[0] != '/' ||
      req.target().find("..") != beast::string_view::npos) {
    return bad_request("Illegal request-target");
  }

  std::string path = path_cat(doc_root, req.target());
  if (req.target().back() == '/') {
    path.append("index.html");
  }

  beast::error_code ec;
  http::file_body::value_type body;
  body.open(path.c_str(), beast::file_mode::scan, ec);

  if (ec == beast::errc::no_such_file_or_directory) {
    return not_found(req.target());
  }

  if (ec) {
    return server_error(ec.message());
  }

  // cache the size since we need it after the move
  auto const size = body.size();

  if (req.method() == http::verb::head) {
    http::response<http::empty_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return res;
  }

  http::response<http::file_body> res{
      std::piecewise_construct, std::make_tuple(std::move(body)),
      std::make_tuple(http::status::ok, req.version())};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, mime_type(path));
  res.content_length(size);
  res.keep_alive(req.keep_alive());
  return res;
}

asio::awaitable<void> do_session(beast::tcp_stream stream,
                                 std::shared_ptr<std::string const> doc_root) {
  beast::flat_buffer buffer;

  for (;;) {
    stream.expires_after(std::chrono::seconds(30));

    http::request<http::string_body> req;
    co_await http::async_read(stream, buffer, req);

    http::message_generator msg = handle_request(*doc_root, std::move(req));

    bool keep_alive = msg.keep_alive();

    co_await beast::async_write(stream, std::move(msg));

    if (!keep_alive) {
      // this means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic
      break;
    }
  }

  stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send);
}

asio::awaitable<void> do_listen(asio::ip::tcp::endpoint endpoint,
                                std::shared_ptr<std::string const> doc_root) {
  auto executor = co_await asio::this_coro::executor;
  auto acceptor = asio::ip::tcp::acceptor{executor, endpoint};

  for (;;) {
    asio::co_spawn(
        executor,
        do_session(beast::tcp_stream{co_await acceptor.async_accept()},
                   doc_root),
        [](std::exception_ptr e) {
          if (e) {
            try {
              std::rethrow_exception(e);
            } catch (std::exception const &e) {
              SERVER_LOG_ERROR() << "Error in session: " << e.what();
            }
          }
        });
  }
}

int server_main(const char *address_str, unsigned short port,
                const char *doc_root_str, int threads) {
  auto const address = asio::ip::make_address(address_str);
  auto const doc_root = std::make_shared<std::string>(doc_root_str);

  asio::io_context ioc{threads};

  asio::co_spawn(ioc,
                 do_listen(asio::ip::tcp::endpoint{address, port}, doc_root),
                 [](std::exception_ptr e) {
                   if (e) {
                     try {
                       std::rethrow_exception(e);
                     } catch (std::exception const &e) {
                       SERVER_LOG_ERROR() << "Error: " << e.what();
                     }
                   }
                 });

  std::cout << "serving " << doc_root_str << " on " << address_str << ":"
            << port << " (" << threads << " threads)" << std::endl;

  std::vector<std::thread> v;
  v.reserve(threads - 1);
  for (auto i = threads - 1; i > 0; --i) {
    v.emplace_back([&ioc] { ioc.run(); });
  }
  ioc.run();

  return 0;
}
