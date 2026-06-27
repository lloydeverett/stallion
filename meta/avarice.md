
## 1(a) avarice.hpp: async interface

Assume that resolving a ref is not synchronous.
Reflect that with a boost awaitable.

## 1(b) avarice.hpp: abstract async

Remove the explicit reliance on boost asio *in this file* by creating
wrappers or aliases as appropriate in async.hpp. All we need is basic
promise functionality so we really needn't go overboard.
 
## 1(c) avarice.hpp: correct lazy init

All of the shared_ptr based ref implementations should make use
of the async gate provided in asyncex.hpp.

Note that we will need to accept a flag indicating concurrency level.
This should be accepted as a template parameter to avarice itself.

Maybe we should generally use an enum to indicate concurrency levels?
Or an enum that converts to a boolean?

## 1(d) avarice.hpp: create docs above the struct laying out purpose and principles

 - automatic / stack storage for refs, OR refs directly owned by
 objects that have automatic storage (e.g. a std::vector)
 - always avoid taking references (&) to refs, though maybe we can
 allow const & references
   since you can't do much with them anyway
 - if you want to store a ref on the heap, consider a
 specialised container that supplies ref copies
   then the coroutine ref will stick around in the stack frame

