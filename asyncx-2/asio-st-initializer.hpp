#pragma once

#include <boost/asio.hpp>
#include <vector>
#include <exception>

enum class state {
    empty,
    error,
    initializing,
    initialized
} s; // todo atomic

struct strand {
};

bool get() {
}

inline boost::asio::awaitable<state> try_initialize(boost::asio::strand<boost::asio::any_io_executor> st) {
    auto ex = co_await boost::asio::this_coro::executor;
    if (s == state::initialized || s == state::error) {
        co_return s;
    }
    // co_await asio::dispatch(asio::bind_executor(st, asio::use_awaitable));
    if (s == state::empty) {
        // do the init
        // s = something nonempty
    }
    // co_await asio::dispatch(asio::bind_executor(go_back_executor, asio::use_awaitable));
    // or however this works, idk
    co_return s;
}

class SharedAsyncGate {
private:
    bool initialized_ = false;
    bool initializing_ = false;
    std::exception_ptr error_ = nullptr;
    std::vector<boost::asio::any_completion_handler<void(std::exception_ptr)>> waiters_;

public:
    template<typename InitFunc>
    boost::asio::awaitable<void> execute(InitFunc init_func) {
        if (initialized_) {
            if (error_) std::rethrow_exception(error_);
            co_return;
        }

        if (initializing_) {
            co_await boost::asio::async_initiate<const boost::asio::use_awaitable_t<>&, void(std::exception_ptr)>(
                [this](auto&& handler) {
                    waiters_.push_back(std::move(handler));
                }, boost::asio::use_awaitable);
            co_return;
        }

        initializing_ = true;

        try {
            co_await init_func();
            initialized_ = true;
        } catch (const boost::system::system_error& e) {
            if (e.code() == boost::asio::error::operation_aborted) {
                initialized_ = false; // Allow retry if cancelled
            } else {
                initialized_ = true;
            }
            error_ = std::current_exception();
        } catch (...) {
            initialized_ = true;
            error_ = std::current_exception();
        }

        initializing_ = false;

        auto ex = co_await boost::asio::this_coro::executor;
        auto to_resume = std::move(waiters_);
        waiters_.clear();

        for (auto& caller : to_resume) {
            auto caller_ex = boost::asio::get_associated_executor(caller, ex);
            boost::asio::post(boost::asio::bind_executor(caller_ex, [c = std::move(caller), err = error_]() mutable {
                c(err);
            }));
        }

        if (error_) {
            // Clear the error if we aborted, so the next caller can try fresh
            std::exception_ptr temp_err = error_;
            if (!initialized_) error_ = nullptr;
            std::rethrow_exception(temp_err);
        }
    }
};

// 2. Your actual class becomes beautifully simple
class Initializer {
private:
    SharedAsyncGate init_gate_;

    boost::asio::awaitable<void> do_init_async() {
        boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);
        timer.expires_after(std::chrono::seconds(1));
        co_await timer.async_wait(boost::asio::use_awaitable);
        // Do your file reading here
    }

public:
    boost::asio::awaitable<void> ensure_initialized() {
        // Just await the shared gate!
        co_await init_gate_.execute([this]() { return do_init_async(); });
    }
};

