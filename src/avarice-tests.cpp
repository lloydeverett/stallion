#include <iostream>
#include <string>

#include <boost/asio.hpp>

#include "async.hpp"
#include "avarice.hpp"
#include "tests.hpp"

// TODO: Write actual tests

class MyObjectT {
public:
  virtual ~MyObjectT() = default;
};

class BaseStateT {
public:
  virtual ~BaseStateT() = default;
};

using av = avarice<MyObjectT, BaseStateT, async::Concurrency::Single>;

class MyObject : public MyObjectT {
  std::string str;

public:
  MyObject(std::string str) : str(std::move(str)) {}
};

async::Awaitable<void> avarice_tests_coro(async::Executor exec) {
  struct CommitRefState : public BaseStateT {
    std::string x;
    void emplace(av::Emplacer<MyObject> emplacer) const {
      emplacer("foo");
      // TODO: Implement
    }
    void traverse() const & {
      // TODO: Should really just be the router that calls this, so it can
      //       filter paths before access
      // TODO: Implement
    }
    void traverse() && {
      // TODO: Should really just be the router that calls this, so it can
      //       filter paths before access
      // TODO: Implement
    }
  } state;

  av::RefTo<MyObject> a{
      av::known_thread_safe_ref_type<MyObject, CommitRefState>, state, exec};
  co_await a.resolve();

  std::cout << sizeof(av::RefTo<MyObject>) << std::endl;
  std::cout << alignof(std::max_align_t) << std::endl;

  av::RefTo<MyObject> a_copy = a;

  av::Ref b{av::copying_ref_type<MyObject, CommitRefState>, state, exec};
  co_await b.resolve();

  av::Ref c{av::known_thread_safe_ref_type<MyObject, CommitRefState>, state,
            exec};
  co_await b.resolve();

  av::Ref b_copy{b};

  av::Ref aa{a.decay()};
  co_await aa.resolve();

  av::RefTo<MyObject> aaa{aa.undecay<MyObject>()};
  co_await aaa.resolve();

  co_await a.resolve();
  co_await aa.resolve();

  std::cout << sizeof(av::Ref) << std::endl;
}

struct avarice_tests {
  avarice_tests() { g_tests.push_back(avarice_tests::test); }
  static void test() {
    std::cout << "TEST SUITE: avarice.hpp\n";
    boost::asio::io_context io;
    boost::asio::co_spawn(io, avarice_tests_coro(io.get_executor()),
                          boost::asio::detached);
    io.run();
  }
} g_avarice_tests;
