#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/asio.hpp>

#include "async.hpp"
#include "avarice.hpp"
#include "tests.hpp"

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
  std::string str_;

public:
  explicit MyObject(std::string s) : str_(std::move(s)) {}
  const std::string &str() const { return str_; }
};

// State whose emplace() constructs a MyObject and optionally tracks call count.
struct SimpleState : public BaseStateT {
  std::string value;
  int *init_count = nullptr;

  explicit SimpleState(std::string v, int *count = nullptr)
      : value(std::move(v)), init_count(count) {}

  void emplace(av::Emplacer<MyObject> em) const {
    if (init_count)
      (*init_count)++;
    em(value);
  }
};

// State whose emplace() always throws.
struct ThrowingState : public BaseStateT {
  void emplace(av::Emplacer<MyObject>) const {
    throw std::runtime_error("init failed");
  }
};

static void test_emplacer() {
  std::cout << "[TEST] Emplacer emplaces into optional..." << std::endl;
  std::optional<MyObject> opt;
  av::Emplacer<MyObject> em(&opt);
  assert(!opt.has_value());
  em("hello");
  assert(opt.has_value());
  assert(opt->str() == "hello");
  em("world"); // replaces existing value
  assert(opt->str() == "world");
  std::cout << "  -> Passed!" << std::endl;
}

static void test_conditional_buffer_stack() {
  std::cout << "[TEST] ConditionalBuffer stack path..." << std::endl;
  av::ConditionalBuffer<int, 48, false> a(42);
  assert(a.get() == 42);

  av::ConditionalBuffer<int, 48, false> b(std::as_const(
      a)); // explicit const to select copy ctor over variadic ctor
  assert(b.get() == 42);
  a.get() = 99;
  assert(b.get() == 42); // copies are independent

  av::ConditionalBuffer<int, 48, false> c(std::move(a));
  assert(c.get() == 99);

  av::ConditionalBuffer<int, 48, false> d(0);
  d = b;
  assert(d.get() == 42);

  av::ConditionalBuffer<int, 48, false> e(0);
  e = std::move(d);
  assert(e.get() == 42);

  std::cout << "  -> Passed!" << std::endl;
}

static void test_conditional_buffer_heap() {
  std::cout
      << "[TEST] ConditionalBuffer heap path (type exceeds stack limit)..."
      << std::endl;
  struct Big {
    char data[64];
    int value;
    explicit Big(int v) : data{}, value(v) {}
    Big(const Big &) = default;
    Big &operator=(const Big &) = default;
  };

  av::ConditionalBuffer<Big, 48, false> a(42);
  assert(a.get().value == 42);

  av::ConditionalBuffer<Big, 48, false> b(std::as_const(
      a)); // explicit const to select copy ctor over variadic ctor
  assert(b.get().value == 42);
  a.get().value = 99;
  assert(b.get().value == 42); // copies are independent

  av::ConditionalBuffer<Big, 48, false> c(std::move(a));
  assert(c.get().value == 99);

  av::ConditionalBuffer<Big, 48, false> d(0);
  d = b;
  assert(d.get().value == 42);

  av::ConditionalBuffer<Big, 48, false> e(0);
  e = std::move(d);
  assert(e.get().value == 42);

  std::cout << "  -> Passed!" << std::endl;
}

static void test_conditional_buffer_destructor() {
  std::cout << "[TEST] ConditionalBuffer calls T's destructor on destruction..."
            << std::endl;
  struct Tracked {
    int *count;
    explicit Tracked(int *c) : count(c) {}
    Tracked(const Tracked &o) : count(o.count) {}
    Tracked(Tracked &&o) noexcept : count(o.count) {}
    ~Tracked() {
      if (count)
        (*count)++;
    }
  };

  int count = 0;
  {
    av::ConditionalBuffer<Tracked, 48, false> buf(&count);
    assert(count == 0);
  }
  assert(count == 1);
  std::cout << "  -> Passed!" << std::endl;
}

// Base/derived pairs for LocalPolymorphic tests. Derived types must be
// copy-constructible and have noexcept move constructors.
struct LPBase {
  virtual ~LPBase() = default;
  virtual int val() const = 0;
};
struct LPA : LPBase {
  int v;
  explicit LPA(int v) : v(v) {}
  LPA(const LPA &) = default;
  LPA(LPA &&) noexcept = default;
  int val() const override { return v; }
};
struct LPB : LPBase {
  double d;
  explicit LPB(double d) : d(d) {}
  LPB(const LPB &) = default;
  LPB(LPB &&) noexcept = default;
  int val() const override { return static_cast<int>(d); }
};

static void test_local_polymorphic_basic() {
  std::cout << "[TEST] LocalPolymorphic emplace, ptr, ptr_as..." << std::endl;
  av::LocalPolymorphic<LPBase, 64> lp;
  assert(lp.ptr() == nullptr);

  lp.emplace<LPA>(42);
  assert(lp.ptr() != nullptr);
  assert(lp.ptr()->val() == 42);
  assert(lp.ptr_as<LPA>() != nullptr);
  assert(lp.ptr_as<LPA>()->v == 42);
  assert(lp.ptr_as<LPB>() == nullptr);

  lp.emplace<LPB>(3.0); // re-emplacing cleans up the previous object
  assert(lp.ptr()->val() == 3);
  assert(lp.ptr_as<LPB>() != nullptr);
  assert(lp.ptr_as<LPA>() == nullptr);

  std::cout << "  -> Passed!" << std::endl;
}

static void test_local_polymorphic_copy() {
  std::cout << "[TEST] LocalPolymorphic copy semantics..." << std::endl;
  av::LocalPolymorphic<LPBase, 64> lp;
  lp.emplace<LPA>(10);

  auto copy1 = lp;
  assert(copy1.ptr()->val() == 10);

  av::LocalPolymorphic<LPBase, 64> copy2;
  copy2.emplace<LPB>(7.0);
  copy2 = lp;
  assert(copy2.ptr()->val() == 10);

  lp.emplace<LPA>(99);
  assert(copy1.ptr()->val() == 10); // copies are independent

  av::LocalPolymorphic<LPBase, 64> empty;
  auto empty_copy = empty;
  assert(empty_copy.ptr() == nullptr);

  std::cout << "  -> Passed!" << std::endl;
}

static void test_local_polymorphic_move() {
  std::cout << "[TEST] LocalPolymorphic move semantics..." << std::endl;
  av::LocalPolymorphic<LPBase, 64> lp;
  lp.emplace<LPA>(99);

  auto moved = std::move(lp);
  assert(moved.ptr()->val() == 99);
  assert(lp.ptr() == nullptr); // source is emptied

  av::LocalPolymorphic<LPBase, 64> other;
  other.emplace<LPB>(5.0);
  other = std::move(moved);
  assert(other.ptr()->val() == 99);
  assert(moved.ptr() == nullptr);

  av::LocalPolymorphic<LPBase, 64> empty;
  auto empty_moved = std::move(empty);
  assert(empty_moved.ptr() == nullptr);

  std::cout << "  -> Passed!" << std::endl;
}

static async::Awaitable<void>
test_known_thread_safe_ref_coro(async::Executor exec) {
  std::cout << "[TEST] KnownThreadSafeRef copies share storage, init once..."
            << std::endl;
  int init_count = 0;
  SimpleState state("hello", &init_count);

  av::RefTo<MyObject> a{av::known_thread_safe_ref_type<MyObject, SimpleState>,
                        state, exec};
  av::RefTo<MyObject> a_copy = a;

  MyObject *p1 = co_await a.resolve();
  assert(p1 != nullptr);
  assert(p1->str() == "hello");
  assert(init_count == 1);

  MyObject *p2 = co_await a_copy.resolve();
  assert(p2 == p1);        // same underlying object
  assert(init_count == 1); // not re-initialized

  MyObject *p3 = co_await a.resolve(); // idempotent
  assert(p3 == p1);
  assert(init_count == 1);

  std::cout << "  -> Passed!" << std::endl;
}

static async::Awaitable<void> test_copying_ref_coro(async::Executor exec) {
  std::cout << "[TEST] CopyingRef copies have independent storage..."
            << std::endl;
  int init_count = 0;
  SimpleState state("world", &init_count);

  av::RefTo<MyObject> a{av::copying_ref_type<MyObject, SimpleState>, state,
                        exec};
  av::RefTo<MyObject> a_copy = a;

  MyObject *p1 = co_await a.resolve();
  assert(p1 != nullptr);
  assert(init_count == 1);

  MyObject *p2 = co_await a_copy.resolve();
  assert(p2 != nullptr);
  assert(p2 != p1); // independent objects
  assert(init_count == 2);

  std::cout << "  -> Passed!" << std::endl;
}

static async::Awaitable<void> test_decay_undecay_coro(async::Executor exec) {
  std::cout << "[TEST] RefTo::decay() and Ref::undecay() roundtrip..."
            << std::endl;
  int init_count = 0;
  SimpleState state("decay", &init_count);

  av::RefTo<MyObject> typed{
      av::known_thread_safe_ref_type<MyObject, SimpleState>, state, exec};

  av::Ref untyped = typed.decay();
  MyObjectT *p1 = co_await untyped.resolve();
  assert(p1 != nullptr);
  assert(init_count == 1);

  av::RefTo<MyObject> re_typed = untyped.undecay<MyObject>();
  MyObject *p2 = co_await re_typed.resolve();
  assert(p2 == p1); // same shared storage
  assert(init_count == 1);

  std::cout << "  -> Passed!" << std::endl;
}

static async::Awaitable<void> test_move_decay_coro(async::Executor exec) {
  std::cout
      << "[TEST] rvalue RefTo::decay() moves storage, leaves source empty..."
      << std::endl;
  int init_count = 0;
  SimpleState state("move", &init_count);

  av::RefTo<MyObject> a{av::known_thread_safe_ref_type<MyObject, SimpleState>,
                        state, exec};
  MyObject *p1 = co_await a.resolve();
  assert(init_count == 1);

  av::Ref r = std::move(a).decay();
  assert(a.ptr() == nullptr);

  MyObjectT *p2 = co_await r.resolve();
  assert(p2 == p1);
  assert(init_count == 1);

  std::cout << "  -> Passed!" << std::endl;
}

static async::Awaitable<void> test_state_accessor_coro(async::Executor exec) {
  std::cout << "[TEST] state() returns the correct stored state..."
            << std::endl;
  SimpleState state("state_test");

  av::RefTo<MyObject> ref{av::known_thread_safe_ref_type<MyObject, SimpleState>,
                          state, exec};

  const SimpleState &s = static_cast<const SimpleState &>(ref.state());
  assert(s.value == "state_test");

  av::Ref untyped = ref.decay();
  const SimpleState &s2 = static_cast<const SimpleState &>(untyped.state());
  assert(s2.value == "state_test");

  std::cout << "  -> Passed!" << std::endl;
  co_return;
}

static async::Awaitable<void>
test_resolve_exception_coro(async::Executor exec) {
  std::cout
      << "[TEST] Exception thrown during emplace propagates from resolve..."
      << std::endl;
  ThrowingState state;

  av::RefTo<MyObject> ref{
      av::known_thread_safe_ref_type<MyObject, ThrowingState>, state, exec};

  bool caught = false;
  try {
    co_await ref.resolve();
  } catch (const std::runtime_error &e) {
    caught = true;
    assert(std::string(e.what()) == "init failed");
  }
  assert(caught);

  std::cout << "  -> Passed!" << std::endl;
  co_return;
}

static async::Awaitable<void> test_ref_untyped_coro(async::Executor exec) {
  std::cout << "[TEST] Ref (untyped) construction and shared copies..."
            << std::endl;
  SimpleState state("base");

  av::Ref ref{av::known_thread_safe_ref_type<MyObject, SimpleState>, state,
              exec};
  av::Ref ref_copy = ref;

  MyObjectT *p1 = co_await ref.resolve();
  assert(p1 != nullptr);

  MyObjectT *p2 = co_await ref_copy.resolve();
  assert(p2 == p1);

  std::cout << "  -> Passed!" << std::endl;
}

struct avarice_tests {
  avarice_tests() { g_tests.push_back(avarice_tests::test); }

  static void run_async(async::Awaitable<void> (*fn)(async::Executor),
                        boost::asio::io_context &io) {
    boost::asio::co_spawn(io, fn(io.get_executor()), boost::asio::detached);
    io.run();
    io.restart();
  }

  static void test() {
    std::cout << "TEST SUITE: avarice.hpp\n";

    test_emplacer();
    test_conditional_buffer_stack();
    test_conditional_buffer_heap();
    test_conditional_buffer_destructor();
    test_local_polymorphic_basic();
    test_local_polymorphic_copy();
    test_local_polymorphic_move();

    boost::asio::io_context io;
    run_async(test_known_thread_safe_ref_coro, io);
    run_async(test_copying_ref_coro, io);
    run_async(test_decay_undecay_coro, io);
    run_async(test_move_decay_coro, io);
    run_async(test_state_accessor_coro, io);
    run_async(test_resolve_exception_coro, io);
    run_async(test_ref_untyped_coro, io);

    std::cout << "\nAll tests passed successfully!\n";
  }
} g_avarice_tests;
