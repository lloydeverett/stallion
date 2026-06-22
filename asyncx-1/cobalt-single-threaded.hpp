#pragma once

#include <concepts>
#include <utility>
#include <cassert>
#include <queue>
#include <coroutine>
#include <boost/cobalt.hpp>

namespace cobalt = boost::cobalt;

class Mutex {
    bool locked_ = false;
    std::queue<std::coroutine_handle<void>> waiters_;

    void unlock() {
        if (waiters_.empty()) {
            locked_ = false;
        } else {
            // Pop the next coroutine waiting inline
            auto next_coro = waiters_.front();
            waiters_.pop();

            // Resume the next coroutine on the current execution thread immediately.
            // Since Cobalt is single-threaded, this acts as a direct handoff.
            next_coro.resume();
        }
    }

public:
    friend class ScopedLock;

    class ScopedLock {
        Mutex* mutex_ptr;

    public:
        ScopedLock(Mutex& mtx) : mutex_ptr(&mtx) {}

        ~ScopedLock() {
            if (mutex_ptr) {
                mutex_ptr->unlock();
            }
        }

        ScopedLock(const ScopedLock&) = delete;
        ScopedLock& operator=(const ScopedLock&) = delete;

        ScopedLock(ScopedLock&& other) noexcept : mutex_ptr(other.mutex_ptr) {
            other.mutex_ptr = nullptr;
        }
        ScopedLock& operator=(ScopedLock&& other) noexcept {
            if (this != &other) {
                mutex_ptr = other.mutex_ptr;
                other.mutex_ptr = nullptr;
            }
            return *this;
        }
    };

    struct LockAwaiter {
        Mutex& mtx;

        bool await_ready() const noexcept {
            return !mtx.locked_;
        }

        void await_suspend(std::coroutine_handle<void> h) noexcept {
            mtx.waiters_.push(h);
        }

        ScopedLock await_resume() noexcept {
            mtx.locked_ = true;
            return ScopedLock{ mtx };
        }
    };

    LockAwaiter lock() noexcept {
        return LockAwaiter{ *this };
    }
};

