#pragma once

#include <atomic>
#include <cassert>
#include <optional>

#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

// An async mutex built on top of an Asio channel.
//
// Callers `co_await lock()` to acquire, receiving an RAII `Lock` that releases
// on destruction. The `IsConcurrent` flag selects between a thread-safe
// `concurrent_channel` (true) for use across multiple threads and a lighter
// single-strand `channel` (false) for use within one Asio strand.
template <bool IsConcurrent> class AsyncMutex {
  using ChannelType =
      typename std::conditional<IsConcurrent,
                                boost::asio::experimental::concurrent_channel<
                                    void(boost::system::error_code, bool)>,
                                boost::asio::experimental::channel<void(
                                    boost::system::error_code, bool)>>::type;
  friend class Lock;
  // We use a channel of bools. The value itself doesn't matter;
  // the presence of an item represents an available lock.
  ChannelType channel_;

public:
  // RAII guard returned by `lock()` and `try_lock_uncontested()`. Releases the
  // mutex when it goes out of scope. Move-only; not copyable.
  class Lock {
    friend class AsyncMutex;
    AsyncMutex *mutex_;

    Lock(AsyncMutex *mutex) : mutex_(mutex) {}

  public:
    Lock(const Lock &) = delete;
    Lock &operator=(const Lock &) = delete;

    // Custom move constructor to transfer ownership
    Lock(Lock &&other) noexcept : mutex_(other.mutex_) {
      other.mutex_ = nullptr;
    }

    Lock &operator=(Lock &&other) noexcept {
      if (this != &other) {
        if (mutex_) {
          mutex_->unlock();
        }
        mutex_ = other.mutex_;
        other.mutex_ = nullptr;
      }
      return *this;
    }

    ~Lock() {
      if (mutex_) {
        mutex_->unlock();
      }
    }
  };

  explicit AsyncMutex(boost::asio::any_io_executor executor)
      : channel_(executor, 1) // Capacity of 1
  {
    // Pre-populate the channel with a single token.
    // This signifies that the mutex is initially unlocked.
    [[maybe_unused]] bool success =
        channel_.try_send(boost::system::error_code(), true);
    assert(success);
  }

  // Acquires the lock. If another coroutine holds it, this suspends
  // until the lock is released.
  boost::asio::awaitable<Lock> lock() {
    co_await channel_.async_receive(boost::asio::use_awaitable);
    co_return Lock{this};
  }

  std::optional<Lock> try_lock_uncontested() {
    if (channel_.try_receive([](boost::system::error_code, bool) {})) {
      return Lock{this};
    } else {
      return std::nullopt;
    }
  }

private:
  // Releases the lock, allowing another waiting coroutine to proceed.
  void unlock() {
    [[maybe_unused]] bool success =
        channel_.try_send(boost::system::error_code(), true);
    assert(success);
  }
};

// Drop-in replacement for `std::atomic<T>` that performs no synchronization
// and ignores memory orders. Used as the `FlagType` in `InitializationGate`
// when `IsConcurrent=false`, where actual atomics would be unnecessary
// overhead.
template <typename T> class PseudoAtomic {
  T obj;

public:
  PseudoAtomic(T value) : obj(std::move(value)) {}
  T load(std::memory_order ignored) { return obj; }
  void store(T value, std::memory_order ignored) { obj = std::move(value); }
};

// Ensures an async initialization callback runs exactly once, even when called
// concurrently. Callers pass an awaitable callback to `try_init()`; the first
// uncontested caller runs the init while any late arrivals wait on the mutex
// and re-check the flag (double-checked locking). Returns nullopt on success or
// if already initialized; returns the stored exception_ptr on failure.
//
// Failure behaviour:
//   - Concurrent waiters (callers blocked on the mutex while init ran) receive
//     the stored exception propagated to them without re-running the callback.
//   - Subsequent non-concurrent callers (arriving after all prior callers have
//     finished, so the mutex is free) acquire uncontested and retry the init
//     from scratch. Failure is therefore not permanent: the gate resets each
//     time a new uncontested caller arrives.
//
// `IsConcurrent` follows the same thread-safety semantics as `AsyncMutex`.
template <bool IsConcurrent> class InitializationGate {
private:
  using FlagType = typename std::conditional<IsConcurrent, std::atomic<bool>,
                                             PseudoAtomic<bool>>::type;
  FlagType initialized{false};
  AsyncMutex<IsConcurrent> mutex;
  std::exception_ptr last_exception{nullptr};

public:
  explicit InitializationGate(boost::asio::any_io_executor executor)
      : mutex(executor) {}

  template <typename InitAwaitable>
  boost::asio::awaitable<std::optional<std::exception_ptr>>
  try_init(InitAwaitable &&init_callback) {
    if (initialized.load(std::memory_order_acquire)) {
      co_return std::nullopt;
    }

    std::optional<typename AsyncMutex<IsConcurrent>::Lock> lock;

    lock = mutex.try_lock_uncontested();
    bool acquired_uncontested = lock.has_value();

    if (!acquired_uncontested) {
      lock = co_await mutex.lock();
    }

    if (initialized.load(std::memory_order_acquire)) {
      co_return std::nullopt;
    }

    if (acquired_uncontested) {
      last_exception = nullptr;

      try {
        co_await init_callback();

        initialized.store(true, std::memory_order_release);
      } catch (...) {
        last_exception = std::current_exception();
      }
    }

    if (last_exception) {
      co_return last_exception;
    }
    co_return std::nullopt;
  }
};

template class AsyncMutex<true>;
template class AsyncMutex<false>;

template class InitializationGate<true>;
template class InitializationGate<false>;
