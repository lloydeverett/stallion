#pragma once

#ifdef ASYNCX_MODE_COBALT

// avast::asio::async_mutex mtx;
// auto lock = co_await self._storage->mtx.async_scoped_lock(boost::cobalt::use_op);

template <typename T>
using Promise = boost::cobalt::promise<T>;

// avast async lock implementation

#else

class Promise {
};
class Mutex {
};
class ScopedLock {
};

// Make promises co_await-able / co_return-able
// Even though in reality everything will be totally sync - make a trivial implementation

#endif

