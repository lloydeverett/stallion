#pragma once

#include <concepts>
#include <utility>
#include <cassert>

#include "cobalt-common.hpp"

class Mutex {
    using ChannelType = boost::cobalt::channel<void>;
    ChannelType ch_{1};

public:
    Mutex() {
        // Seed the channel with exactly one token (the lock)
        bool success = ch_.try_write({});
        assert(success);
    }

    // Movable but non-copyable RAII Guard
    class [[nodiscard]] ScopedGuard {
        Mutex* mutex_;

        // Only the parent mutex can construct a guard
        friend class Mutex;
        explicit ScopedGuard(Mutex& m) : mutex_(&m) {}

    public:
        ~ScopedGuard() {
            if (mutex_) {
                bool success = mutex_->ch_.try_write({});
                assert(success);
            }
        }

        ScopedGuard(ScopedGuard&& other) noexcept : mutex_(std::exchange(other.mutex_, nullptr)) {}
        ScopedGuard& operator=(ScopedGuard&& other) noexcept {
            if (this != &other) {
                if (mutex_) {
                    bool success = mutex_->ch_.try_write({});
                    assert(success);
                }
                mutex_ = std::exchange(other.mutex_, nullptr);
            }
            return *this;
        }

        // Delete copy operations to prevent multiple unlocks
        ScopedGuard(const ScopedGuard&) = delete;
        ScopedGuard& operator=(const ScopedGuard&) = delete;
    };

    boost::cobalt::task<ScopedGuard> lock() {
        co_await ch_.read();
        co_return ScopedGuard{*this};
    }
};

