// Single threaded modifications: 2026 Lloyd Everett <lloydeverett@gmail.com>
//
// SPDX-FileCopyrightText: 2023 Daniel Vrátil <daniel.vratil@gendigital.com>
// SPDX-FileCopyrightText: 2023 Martin Beran <martin.beran@gendigital.com>
//
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <mutex>

#ifdef ASIO_STANDALONE
#define ASIO_NS ::asio
#else
#define ASIO_NS boost::asio
#endif

namespace avast::asio {

class async_mutex_lock;
class async_mutex;

/** \internal **/
namespace detail {

/**
 * \brief Represents a suspended coroutine that is awaiting lock acquisition.
 **/
struct locked_waiter {
    /**
     * \brief Constructs a new locked_waiter.
     * \param next_waiter Pointer to the waiter to prepend this locked_waiter to.
     **/
    explicit locked_waiter(locked_waiter *next_waiter): next(next_waiter) {}
    locked_waiter(locked_waiter &&) = delete;
    locked_waiter(const locked_waiter &) = delete;
    locked_waiter &operator=(locked_waiter &&) = delete;
    locked_waiter &operator=(const locked_waiter &) = delete;
    /**
     * \brief Destructor.
     **/
    virtual ~locked_waiter() = default;

    /**
     * \brief Completes the pending asynchronous operation.
     *
     * Resumes the currently suspended coroutine with the acquired lock.
     **/
    virtual void completion() = 0;

    /**
     * The waiters are held in a linked list. This is a pointer to the next member of the list.
     **/
    locked_waiter *next = nullptr;
};

/**
 * \brief Locked waiter that used `async_mutex::async_lock()` to acquire the lock.
 **/
template <typename Token>
struct async_locked_waiter final: public locked_waiter {
    /**
     * \brief Constructs a new async_locked_waiter.
     * \param mutex A mutex that the waiter is trying to acquire a lock for.
     * \param next_waiter Pointer to the head of the waiters linked list to prepend this waiter to.
     * \param token The complention token to call when the asynchronous operation is completed.
     **/
    async_locked_waiter([[maybe_unused]] async_mutex *mutex, locked_waiter *next_waiter, Token &&token):
        locked_waiter(next_waiter), m_token(std::move(token)) {}

    void completion() override {
        auto executor = ASIO_NS::get_associated_executor(m_token);
        ASIO_NS::post(std::move(executor), [token = std::move(m_token)]() mutable { token(); });
    }

private:
    Token m_token; //!< The completion token to invoke when the lock is acquired.
};

/**
 * \brief Locked waiter that used `async_mutex::async_scoped_lock()` to acquire the lock.
 **/
template <typename Token>
struct scoped_async_locked_waiter final: public locked_waiter {
    /**
     * \brief Constructs a new scoped_async_locked_waiter.
     * \param mutex A mutex that the waiter is trying to acquire a lock for.
     * \param next_waiter Pointer to the head of the waiters linked list to prepend this waiter to.
     * \param token The complention token to call when the asynchronous operation is completed.
     **/
    scoped_async_locked_waiter(async_mutex *mutex, locked_waiter *next_waiter, Token &&token):
        locked_waiter(next_waiter), m_mutex(mutex), m_token(std::move(token)) {}

    void completion() override;

private:
    async_mutex *m_mutex; //!< The mutex whose lock is being awaited.
    Token m_token;        //!< The completion token to invoke when the lock is acquired.
};

/**
 * \brief An initiator for asio::async_initiate().
 **/
template <template <typename Token> typename Waiter>
class async_lock_initiator_base {
public:
    /**
     * Constructs a new initiator for an operation on the given mutex.
     *
     * \param mutex A mutex on which the asynchronous lock operation is being initiated.
     **/
    explicit async_lock_initiator_base(async_mutex *mutex): m_mutex(mutex) {}

    /**
     * \brief Invoked by boost asio when the asynchronous operation is initiated.
     *
     * \param handler A completion handler (a callable) to be called when the asynchronous operation
     *                has completed (in our case, the lock has been acquired).
     * \tparam Handler A callable with signature void(T) where T is the type of the object that will be
     *                 returned as a result of `co_await`ing the operation. In our case that's either
     *                 `void` for `async_lock()` or `async_mutex_lock` for `async_scoped_lock()`.
     **/
    template <typename Handler>
    void operator()(Handler &&handler);

protected:
    async_mutex *m_mutex; //!< The mutex whose lock is being awaited.
};

/**
 * \brief Initiator for the async_lock() operation.
 **/
using initiate_async_lock = async_lock_initiator_base<async_locked_waiter>;

/**
 * \brief Initiator for the async_scoped_lock() operation.
 **/
using initiate_scoped_async_lock = async_lock_initiator_base<scoped_async_locked_waiter>;

} // namespace detail
/** \endinternal **/

/**
 * \brief A basic mutex that can acquire lock asynchronously using asio coroutines.
 **/
class async_mutex {
public:
    async_mutex() noexcept = default;
    async_mutex(const async_mutex &) = delete;
    async_mutex(async_mutex &&) = delete;
    async_mutex &operator=(const async_mutex &) = delete;
    async_mutex &operator=(async_mutex &&) = delete;

    ~async_mutex() {
        assert(!m_locked);
        assert(m_waiters_head == nullptr);
    }

    [[nodiscard]] bool try_lock() noexcept {
        if (!m_locked) {
            m_locked = true;
            return true;
        }
        return false;
    }

    template <ASIO_NS::completion_token_for<void()> LockToken>
    [[nodiscard]] auto async_lock(LockToken &&token) {
        return ASIO_NS::async_initiate<LockToken, void()>(detail::initiate_async_lock(this), token);
    }

    template <ASIO_NS::completion_token_for<void(async_mutex_lock)> LockToken>
    [[nodiscard]] auto async_scoped_lock(LockToken &&token) {
        return ASIO_NS::async_initiate<LockToken, void(async_mutex_lock)>(detail::initiate_scoped_async_lock(this), token);
    }

    void unlock() {
        assert(m_locked);

        // If no one is waiting, just unlock and we are done.
        if (m_waiters_head == nullptr) {
            m_locked = false;
            return;
        }

        // Pop the first waiter from the head of the queue (FIFO)
        auto *to_resume = m_waiters_head;
        m_waiters_head = m_waiters_head->next;
        if (m_waiters_head == nullptr) {
            m_waiters_tail = nullptr;
        }

        // Note: m_locked remains true because ownership passes directly to this waiter
        to_resume->next = nullptr;
        to_resume->completion();
        delete to_resume;
    }

private:
    template <template <typename Token> typename Waiter>
    friend class detail::async_lock_initiator_base;

    bool m_locked = false; // Replaces the atomic m_state
    detail::locked_waiter *m_waiters_head = nullptr; // Track the front of the queue
    detail::locked_waiter *m_waiters_tail = nullptr; // Track the back of the queue
};

/**
 * \brief A RAII-style lock for async_mutex which automatically unlocks the mutex when destroyed.
 **/
class async_mutex_lock {
public:
    using mutex_type = async_mutex;

    /**
     * Constructs a new async_mutex_lock without any associated mutex.
     **/
    explicit async_mutex_lock() noexcept = default;

    /**
     * Constructs a new async_mutex_lock, taking ownership of the \c mutex.
     *
     * \param mutex Locked mutex to be unlocked when this objectis destroyed.
     *
     * \warning The \c mutex must be in a locked state.
     **/
    explicit async_mutex_lock(mutex_type &mutex, std::adopt_lock_t) noexcept: m_mutex(&mutex) {}

    /**
     * \brief Initializes the lock with contents of other. Leaves other with no associated mutex.
     * \param other The moved-from object.
     **/
    async_mutex_lock(async_mutex_lock &&other) noexcept { swap(other); }

    /**
     * \brief Move assignment operator.
     * Replaces the current mutex with those of \c other using move semantics.
     * If \c *this already has an associated mutex, the mutex is unlocked.
     *
     * \param other The moved-from object.
     * \returns *this.
     */
    async_mutex_lock &operator=(async_mutex_lock &&other) noexcept {
        if (m_mutex != nullptr) {
            m_mutex->unlock();
        }
        m_mutex = std::exchange(other.m_mutex, nullptr);
        return *this;
    }

    /**
     * \brief Copy constructor (deleted).
     **/
    async_mutex_lock(const async_mutex_lock &) = delete;

    /**
     * \brief Copy assignment operator (deleted).
     **/
    async_mutex_lock &operator=(const async_mutex_lock &) = delete;

    ~async_mutex_lock() {
        if (m_mutex != nullptr) {
            m_mutex->unlock();
        }
    }

    bool owns_lock() const noexcept { return m_mutex != nullptr; }
    mutex_type *mutex() const noexcept { return m_mutex; }

    /**
     * \brief Swaps state with \c other.
     * \param other the lock to swap state with.
     **/
    void swap(async_mutex_lock &other) noexcept { std::swap(m_mutex, other.m_mutex); }

private:
    mutex_type *m_mutex = nullptr; //!< The locked mutex being held by the scoped mutex lock.
};

/** \internal **/
namespace detail {

template <template <typename Token> typename Waiter>
template <typename Handler>
void async_lock_initiator_base<Waiter>::operator()(Handler &&handler) {
    // Scenario A: Mutex is free. Grab it immediately.
    if (!m_mutex->m_locked) {
        m_mutex->m_locked = true;
        // Resume immediately via Asio's post
        Waiter<Handler>(m_mutex, nullptr, std::forward<Handler>(handler)).completion();
        return;
    }

    // Scenario B: Mutex is busy. Queue up at the tail.
    auto *waiter = new Waiter<Handler>(m_mutex, nullptr, std::forward<Handler>(handler));

    if (m_mutex->m_waiters_tail == nullptr) {
        m_mutex->m_waiters_head = waiter;
        m_mutex->m_waiters_tail = waiter;
    } else {
        m_mutex->m_waiters_tail->next = waiter;
        m_mutex->m_waiters_tail = waiter;
    }
}

} // namespace detail
/** \endinternal **/

} // namespace avast::asio

