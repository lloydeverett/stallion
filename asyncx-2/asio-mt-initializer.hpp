#pragma once

#include <boost/asio.hpp>
#include <boost/asio/error.hpp>             // For boost::asio::error::operation_aborted
#include <boost/system/system_error.hpp>     // For boost::system::system_error
#include <exception>
#include <atomic>
#include <chrono>

#include "asio-mt-ambient-executor.hpp"

//  TODO: No, this is broken

class Initializer {
private:
    enum class State : int {
        Uninitialized,
        Initializing,
        Ready,
        Failed
    };

    boost::asio::strand<boost::asio::any_io_executor> strand_;
    std::atomic<State> state_{State::Uninitialized};

    std::exception_ptr init_error_ = nullptr;
    // Used purely as a multi-consumer broadcast mechanism via cancel()
    boost::asio::steady_timer cv_timer_;

    boost::asio::awaitable<void> do_init_async() {
        boost::asio::steady_timer timer(strand_);
        timer.expires_after(std::chrono::seconds(1));
        co_await timer.async_wait(boost::asio::use_awaitable);
    }

public:
    explicit Initializer(AmbientExecutor ioc)
        : strand_(boost::asio::make_strand(ioc)),
          cv_timer_(strand_, boost::asio::steady_timer::time_point::max()) {}

    boost::asio::awaitable<void> reset_error() {
        co_await boost::asio::post(strand_, boost::asio::use_awaitable);

        if (state_.load(std::memory_order_relaxed) == State::Failed) {
            init_error_ = nullptr;
            // Reset the timer so it can be used for waiting again
            cv_timer_.expires_at(boost::asio::steady_timer::time_point::max());
            state_.store(State::Uninitialized, std::memory_order_release);
        }
        co_return;
    }

    boost::asio::awaitable<void> ensure_initialized() {
        // 1. Safe Fast Path: Read unified state atomically
        State current_state = state_.load(std::memory_order_acquire);
        if (current_state == State::Ready) {
            co_return;
        }

        // 2. Hop onto strand for synchronized state transitions
        co_await boost::asio::post(strand_, boost::asio::use_awaitable);

        while (true) {
            current_state = state_.load(std::memory_order_relaxed);

            if (current_state == State::Ready) {
                co_return;
            }
            if (current_state == State::Failed) {
                std::rethrow_exception(init_error_);
            }

            if (current_state == State::Initializing) {
                boost::system::error_code ec;
                // Wait for the broadcast cancellation
                co_await cv_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                // Loop around to re-evaluate the new state safely
            } else if (current_state == State::Uninitialized) {
                state_.store(State::Initializing, std::memory_order_relaxed);

                try {
                    co_await do_init_async();
                    state_.store(State::Ready, std::memory_order_release);
                    cv_timer_.cancel(); // Wake up all waiters
                    co_return;
                }
                catch (const boost::system::system_error& ex) {
                    if (ex.code() == boost::asio::error::operation_aborted) {
                        // This specific coroutine was cancelled. Revert state so others can try.
                        state_.store(State::Uninitialized, std::memory_order_release);
                        cv_timer_.cancel();
                        throw;
                    }
                    // Other system errors count as a true initialization failure
                    init_error_ = std::current_exception();
                    state_.store(State::Failed, std::memory_order_release);
                    cv_timer_.cancel();
                    std::rethrow_exception(init_error_);
                }
                catch (...) {
                    init_error_ = std::current_exception();
                    state_.store(State::Failed, std::memory_order_release);
                    cv_timer_.cancel(); // Wake up all waiters to receive the exception
                    std::rethrow_exception(init_error_);
                }
            }
        }
    }
};

