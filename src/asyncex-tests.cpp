#include <iostream>

#include "asyncex.hpp"
#include "tests.hpp"

using namespace asyncex;

template <bool IsConcurrent> void test_async_mutex() {
  std::cout << "[TEST] Running AsyncMutex mutual exclusion test..."
            << std::endl;
  boost::asio::io_context io;
  AsyncMutex<IsConcurrent> mutex(io.get_executor());
  int shared_counter = 0;

  // Worker coroutine that reads, suspends, and writes back.
  // Without a mutex, this guarantees a race condition.
  auto worker = [&]() -> boost::asio::awaitable<void> {
    auto lock = co_await mutex.lock();

    int temp = shared_counter;
    boost::asio::steady_timer timer(io, std::chrono::milliseconds(10));
    co_await timer.async_wait(boost::asio::use_awaitable);
    shared_counter = temp + 1;
  };

  // Spawn two concurrent coroutines
  boost::asio::co_spawn(io, worker(), boost::asio::detached);
  boost::asio::co_spawn(io, worker(), boost::asio::detached);

  io.run();

  // If the mutex works, the counter will be 2. If it fails, it will be 1.
  assert(shared_counter == 2 && "Mutex failed to protect shared resource!");
  std::cout << "  -> Passed!" << std::endl;
}

template <bool IsConcurrent> void test_initialization_gate_success() {
  std::cout << "[TEST] Running InitializationGate exactly-once test..."
            << std::endl;
  boost::asio::io_context io;
  InitializationGate<IsConcurrent> gate(io.get_executor());
  std::atomic<int> init_execution_count{0};

  auto worker = [&]() -> boost::asio::awaitable<void> {
    auto err = co_await gate.try_init([&]() -> boost::asio::awaitable<void> {
      // Simulate slow async initialization
      boost::asio::steady_timer timer(io, std::chrono::milliseconds(20));
      co_await timer.async_wait(boost::asio::use_awaitable);
      init_execution_count++;
    });
    assert(!err.has_value() &&
           "Initialization returned an error unexpectedly!");
  };

  // Bombard the gate with 5 concurrent requests
  for (int i = 0; i < 5; ++i) {
    boost::asio::co_spawn(io, worker(), boost::asio::detached);
  }

  io.run();

  // Initialization payload should have only executed exactly once
  assert(init_execution_count == 1 &&
         "Initialization block executed multiple times!");
  std::cout << "  -> Passed!" << std::endl;
}

template <bool IsConcurrent> void test_initialization_gate_failure_and_retry() {
  std::cout << "[TEST] Running InitializationGate failure recovery test..."
            << std::endl;
  boost::asio::io_context io;
  InitializationGate<IsConcurrent> gate(io.get_executor());
  int attempt_count = 0;

  auto init_logic = [&]() -> boost::asio::awaitable<void> {
    attempt_count++;
    if (attempt_count == 1) {
      throw std::runtime_error("Simulated network failure");
    }
    co_return;
  };

  auto worker1 = [&]() -> boost::asio::awaitable<void> {
    auto err = co_await gate.try_init(init_logic);
    assert(err.has_value() && "Expected first initialization to fail!");

    // Verify it's the exact exception we threw
    try {
      std::rethrow_exception(err.value());
    } catch (const std::runtime_error &e) {
      assert(std::string(e.what()) == "Simulated network failure");
    }
  };

  auto worker2 = [&]() -> boost::asio::awaitable<void> {
    auto err = co_await gate.try_init(init_logic);
    assert(!err.has_value() && "Expected second initialization to succeed!");
  };

  // 1. Run the failing initialization
  boost::asio::co_spawn(io, worker1(), boost::asio::detached);
  io.run();

  // Reset the IO context to run again
  io.restart();

  // 2. Run the retry initialization
  boost::asio::co_spawn(io, worker2(), boost::asio::detached);
  io.run();

  assert(attempt_count == 2 && "Expected exactly 2 initialization attempts!");
  std::cout << "  -> Passed!" << std::endl;
}

struct asyncex_tests {
  asyncex_tests() { g_tests.push_back(asyncex_tests::test); }
  static void test() {
    std::cout << "[Non-concurrent mutex and gate implementation]\n";
    test_async_mutex<false>();
    test_initialization_gate_success<false>();
    test_initialization_gate_failure_and_retry<false>();
    std::cout << "\n[Concurrent mutex and gate implementation]\n";
    test_async_mutex<true>();
    test_initialization_gate_success<true>();
    test_initialization_gate_failure_and_retry<true>();
    std::cout << "\nAll tests passed successfully!\n";
  }
} g_asyncex_tests;
