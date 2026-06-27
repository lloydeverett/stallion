#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <new> /* IWYU pragma: keep */
#include <optional>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "async.hpp"
#include "asyncex.hpp"

// Avarice manages typed, polymorphic, lazily-resolved object references
// (Ref / RefTo<T>) with value semantics and async resolution.
//
// Refs are value types. Store them on the stack or inside containers that
// themselves have automatic storage (e.g. a std::vector<Ref> local variable).
// Copy and move them freely.
//
// Avoid taking a non-const lvalue reference (&) to a Ref; there is seldom a
// reason to and it forces callers to reason about aliasing. A const reference
// (const Ref &) is acceptable because it is read-only and cannot trigger
// resolution.
//
// If a Ref must live on the heap, prefer a specialised container that supplies
// Ref copies on demand. Each coroutine then holds its own Ref copy in its
// stack frame for the lifetime of the co_await, avoiding dangling references.
template <typename ObjectT, typename BaseStateT,
          async::Concurrency ConcurrencyLevel>
  requires std::has_virtual_destructor_v<ObjectT>
struct avarice {
  static constexpr bool IsConcurrent =
      (ConcurrencyLevel == async::Concurrency::Multi);

  template <typename T> class Emplacer {
    std::optional<T> *opt_ptr;

  public:
    Emplacer(std::optional<T> *opt_ptr) : opt_ptr(opt_ptr) { assert(opt_ptr); }
    template <typename... Args> void operator()(Args &&...args) const {
      opt_ptr->emplace(std::forward<Args>(args)...);
    }
  };

  template <typename T, size_t MaxStackSize, bool IsSizeMeasurement>
  class ConditionalBuffer {
  private:
    static_assert(!IsSizeMeasurement || sizeof(T) <= MaxStackSize);
    static constexpr bool FitsOnStack =
        IsSizeMeasurement || sizeof(T) <= MaxStackSize;

    union StackBuffer {
      std::byte dummy[IsSizeMeasurement ? MaxStackSize : 1];
      T value;

      // Initialise with dummy state.
      StackBuffer() : dummy{} {}

      // We must provide a dummy destructor because T will be destroyed
      // manually.
      ~StackBuffer() {}
    };

    using BufferType =
        std::conditional_t<FitsOnStack, StackBuffer, std::unique_ptr<T>>;

    BufferType buffer;

  public:
    template <typename... Args> explicit ConditionalBuffer(Args &&...args) {
      if constexpr (FitsOnStack) {
        new (&buffer.value) T(std::forward<Args>(args)...);
      } else {
        buffer = std::make_unique<T>(std::forward<Args>(args)...);
      }
    }

    T &get() noexcept {
      if constexpr (FitsOnStack) {
        return buffer.value;
      } else {
        return *buffer;
      }
    }

    const T &get() const noexcept {
      if constexpr (FitsOnStack) {
        return buffer.value;
      } else {
        return *buffer;
      }
    }

    ConditionalBuffer(const ConditionalBuffer &other)
      requires std::copy_constructible<T>
    {
      if constexpr (FitsOnStack) {
        // Placement new using the other object's value
        new (&buffer.value) T(other.get());
      } else {
        // Allocate a new heap object using the other object's value
        buffer = std::make_unique<T>(other.get());
      }
    }

    ConditionalBuffer(ConditionalBuffer &&other) noexcept(
        std::is_nothrow_move_constructible_v<T>)
      requires std::move_constructible<T>
    {
      if constexpr (FitsOnStack) {
        // Placement new moving the other object's value
        new (&buffer.value) T(std::move(other.get()));
      } else {
        // Steal the heap pointer
        buffer = std::move(other.buffer);
      }
    }

    ConditionalBuffer &operator=(const ConditionalBuffer &other)
      requires std::copyable<T>
    {
      if (this != &other) {
        if constexpr (FitsOnStack) {
          // Delegate to T's copy assignment
          get() = other.get();
        } else {
          // If this object was previously moved-from, the unique_ptr might be
          // null
          if (!buffer) {
            buffer = std::make_unique<T>(other.get());
          } else {
            *buffer = other.get();
          }
        }
      }
      return *this;
    }

    ConditionalBuffer &operator=(ConditionalBuffer &&other) noexcept(
        std::is_nothrow_move_assignable_v<T>)
      requires std::is_move_assignable_v<T>
    {
      if (this != &other) {
        if constexpr (FitsOnStack) {
          // Delegate to T's move assignment
          get() = std::move(other.get());
        } else {
          // Steal the other's heap pointer (automatically destroys current heap
          // object if it exists)
          buffer = std::move(other.buffer);
        }
      }
      return *this;
    }

    ~ConditionalBuffer() {
      if constexpr (FitsOnStack) {
        get().~T();
      }
      // If on the heap, unique_ptr cleans itself up automatically.
    }
  };

  enum class PlacementNewAction { Copy, Move };

  template <typename Base, size_t MaxSize,
            size_t Alignment = alignof(std::max_align_t)>
  class LocalPolymorphic {
    static_assert(std::has_virtual_destructor_v<Base>,
                  "Base class lacks a virtual destructor!");

  protected:
    alignas(Alignment) std::byte buffer[MaxSize];
    Base *_ptr = nullptr;

    using ManagerFn = void *(*)(PlacementNewAction action, void *dest,
                                void *src);
    ManagerFn _manager_fn = nullptr;

  public:
    LocalPolymorphic() = default;

    template <typename Derived, typename... Args>
    explicit LocalPolymorphic(std::in_place_type_t<Derived>, Args &&...args) {
      emplace<Derived>(std::forward<Args>(args)...);
    }

    template <typename Derived, typename... Args> void emplace(Args &&...args) {
      static_assert(sizeof(Derived) <= MaxSize,
                    "Object exceeds local stack buffer space.");
      static_assert(std::is_base_of_v<Base, Derived>,
                    "Type must extend the base class.");
      static_assert(
          Alignment >= alignof(Derived),
          "Container alignment is too loose for the requested derived type.");
      static_assert(
          std::is_copy_constructible_v<Derived>,
          "Derived type must be copyable to support copying LocalPolymorphic.");
      static_assert(std::is_nothrow_move_constructible_v<Derived>,
                    "Derived type must have a noexcept move constructor to be "
                    "used in LocalPolymorphic.");

      cleanup();

      _ptr = ::new (static_cast<void *>(buffer))
          Derived(std::forward<Args>(args)...);
      _manager_fn = [](PlacementNewAction action, void *dest,
                       void *src) -> void * {
        Base *base_src = static_cast<Base *>(src);
        if (action == PlacementNewAction::Copy) {
          // NB: Source is logically const, be careful not to mutate it
          // Safe to cast to const Derived& for copying
          const Derived &typed_src = static_cast<const Derived &>(*base_src);
          return static_cast<Base *>(::new (dest) Derived(typed_src));
        } else {
          // Safe to cast to mutable Derived& for moving
          Derived &typed_src = static_cast<Derived &>(*base_src);
          return static_cast<Base *>(::new (dest)
                                         Derived(std::move(typed_src)));
        }
      };
    }

    /* Weak exception guarantee for copies */
    LocalPolymorphic(const LocalPolymorphic &other) {
      if (other._ptr) {
        assert(other._manager_fn);
        _ptr = static_cast<Base *>(
            other._manager_fn(PlacementNewAction::Copy, buffer, other._ptr));
        _manager_fn = other._manager_fn;
      }
    }

    /* Weak exception guarantee for copies */
    LocalPolymorphic &operator=(const LocalPolymorphic &other) {
      if (this != &other) {
        cleanup();
        if (other._ptr) {
          assert(other._manager_fn);
          _ptr = static_cast<Base *>(
              other._manager_fn(PlacementNewAction::Copy, buffer, other._ptr));
          _manager_fn = other._manager_fn;
        }
      }
      return *this;
    }

    /* Weak exception guarantee for moves */
    LocalPolymorphic(LocalPolymorphic &&other) noexcept {
      if (other._ptr) {
        assert(other._manager_fn);
        _ptr = static_cast<Base *>(
            other._manager_fn(PlacementNewAction::Move, buffer, other._ptr));
        _manager_fn = other._manager_fn;
        other.cleanup();
      }
    }

    /* Weak exception guarantee for moves */
    LocalPolymorphic &operator=(LocalPolymorphic &&other) noexcept {
      if (this != &other) {
        cleanup();
        if (other._ptr) {
          assert(other._manager_fn);
          _ptr = static_cast<Base *>(
              other._manager_fn(PlacementNewAction::Move, buffer, other._ptr));
          _manager_fn = other._manager_fn;
          other.cleanup();
        }
      }
      return *this;
    }

    Base *ptr() noexcept { return _ptr; }
    const Base *ptr() const noexcept { return _ptr; }

    template <typename T> T *ptr_as() noexcept {
      return dynamic_cast<T *>(_ptr);
    }
    template <typename T> const T *ptr_as() const noexcept {
      return dynamic_cast<const T *>(_ptr);
    }

    ~LocalPolymorphic() { cleanup(); }

  protected:
    void cleanup() {
      if (_ptr) {
        _ptr->~Base();
        _ptr = nullptr;
        _manager_fn = nullptr;
      }
    }
  };

  class BaseRef {
  public:
    virtual async::Awaitable<ObjectT *> resolve() = 0;
    virtual const BaseStateT &state() const = 0;
    virtual ~BaseRef() = default;
  };

  //  NOTE: Must use single, non-virtual inheritance. See notes in Ref and RefTo
  //  implementations.
  template <typename T>
    requires std::derived_from<T, ObjectT>
  class BaseRefTo : public BaseRef {
  public:
    virtual async::Awaitable<T *> resolve_typed() = 0;

    async::Awaitable<ObjectT *> resolve() override {
      T *p = co_await resolve_typed();
      co_return static_cast<ObjectT *>(p);
    }
  };

  // Storage backed by a shared_ptr: copies share the same underlying object
  // and initialization gate.
  template <typename T> class KnownThreadSafeStorage {
  private:
    std::shared_ptr<asyncex::Lazy<T, IsConcurrent>> ptr;

  public:
    explicit KnownThreadSafeStorage(async::Executor exec)
        : ptr(std::make_shared<asyncex::Lazy<T, IsConcurrent>>(exec)) {}

    std::optional<T> &opt() {
      assert(ptr);
      return ptr->opt();
    }
    const std::optional<T> &opt() const {
      assert(ptr);
      return ptr->opt();
    }
    asyncex::InitializationGate<IsConcurrent> &gate() {
      assert(ptr);
      return ptr->gate();
    }
  };

  // Storage backed by a unique_ptr: copies produce independent objects with
  // their own initialization gates.
  template <typename T> class CopyingStorage {
  private:
    std::unique_ptr<asyncex::Lazy<T, IsConcurrent>> ptr;
    async::Executor executor_;

  public:
    explicit CopyingStorage(async::Executor exec)
        : ptr(std::make_unique<asyncex::Lazy<T, IsConcurrent>>(exec)),
          executor_(exec) {}

    CopyingStorage(const CopyingStorage &other) : executor_(other.executor_) {
      if (other.ptr) {
        ptr = std::make_unique<asyncex::Lazy<T, IsConcurrent>>(
            executor_, other.ptr->opt());
      }
    }

    CopyingStorage &operator=(const CopyingStorage &other) {
      if (this != &other) {
        executor_ = other.executor_;
        if (other.ptr) {
          ptr = std::make_unique<asyncex::Lazy<T, IsConcurrent>>(
              executor_, other.ptr->opt());
        } else {
          ptr.reset();
        }
      }
      return *this;
    }

    CopyingStorage(CopyingStorage &&) noexcept = default;
    CopyingStorage &operator=(CopyingStorage &&) noexcept = default;
    ~CopyingStorage() = default;

    std::optional<T> &opt() {
      assert(ptr);
      return ptr->opt();
    }
    const std::optional<T> &opt() const {
      assert(ptr);
      return ptr->opt();
    }
    asyncex::InitializationGate<IsConcurrent> &gate() {
      assert(ptr);
      return ptr->gate();
    }
  };

  static constexpr int CONDITIONAL_BUFFER_MAX_STACK_SIZE = 48;
  static_assert(sizeof(BaseStateT) <= CONDITIONAL_BUFFER_MAX_STACK_SIZE);

  template <typename T, typename StateT, template <typename> class StorageT,
            bool IsSizeMeasurement = false>
    requires std::copyable<StorageT<T>>
  class StorageRef : public BaseRefTo<T> {
    ConditionalBuffer<StateT, CONDITIONAL_BUFFER_MAX_STACK_SIZE,
                      IsSizeMeasurement>
        _state;
    StorageT<T> _storage;

  public:
    StorageRef(StateT state, async::Executor executor)
        : _state(std::move(state)), _storage(executor) {
      // Assertion must be in constructor to avoid failing the assert when
      // evaluating constexpr object sizes with dummy parameters
      static_assert(std::copyable<StateT>, "State class must be copyable!");
      static_assert(
          std::is_base_of_v<BaseStateT, StateT>,
          "State class must inherit from base state template parameter type!");
    }

    async::Awaitable<T *> resolve_typed() override {
      auto maybe_exc =
          co_await _storage.gate().try_init([this]() -> async::Awaitable<void> {
            _state.get().emplace(Emplacer(&_storage.opt()));
            co_return;
          });
      if (maybe_exc) {
        std::rethrow_exception(*maybe_exc);
      }
      co_return _storage.opt() ? &(*_storage.opt()) : nullptr;
    }

    const StateT &state() const override { return _state.get(); }
  };

  template <typename T, typename StateT, bool IsSizeMeasurement = false>
    requires std::copyable<KnownThreadSafeStorage<T>>
  using KnownThreadSafeRef =
      StorageRef<T, StateT, KnownThreadSafeStorage, IsSizeMeasurement>;
  template <typename T, typename StateT, bool IsSizeMeasurement = false>
    requires std::copyable<CopyingStorage<T>>
  using CopyingRef = StorageRef<T, StateT, CopyingStorage, IsSizeMeasurement>;

  template <typename T, typename StateT>
  static constexpr auto known_thread_safe_ref_type =
      std::in_place_type<KnownThreadSafeRef<T, StateT>>;
  template <typename T, typename StateT>
  static constexpr auto copying_ref_type =
      std::in_place_type<CopyingRef<T, StateT>>;

  static constexpr int POLYMORPHIC_REF_BUFFER_SIZE =
      std::max({sizeof(KnownThreadSafeRef<ObjectT, BaseStateT, true>),
                sizeof(CopyingRef<ObjectT, BaseStateT, true>)});

  class Ref;

  template <typename T>
  class RefTo
      : public LocalPolymorphic<BaseRefTo<T>, POLYMORPHIC_REF_BUFFER_SIZE> {
  public:
    using LocalPolymorphic<BaseRefTo<T>,
                           POLYMORPHIC_REF_BUFFER_SIZE>::LocalPolymorphic;
    using LocalPolymorphic<BaseRefTo<T>, POLYMORPHIC_REF_BUFFER_SIZE>::ptr;

    friend class Ref;

    async::Awaitable<T *> resolve() {
      assert(ptr());
      co_return co_await ptr()->resolve_typed();
    }

    const BaseStateT &state() const {
      assert(ptr());
      return ptr()->state();
    }

    Ref decay() const & {
      Ref r;
      if (this->_ptr) {
        assert(this->_manager_fn);
        r._ptr = static_cast<BaseRef *>(
            this->_manager_fn(PlacementNewAction::Copy, r.buffer,
                              static_cast<BaseRef *>(this->_ptr)));
        //  NOTE: Reusing _manager_fn implies a need for single inheritance
        //  (base must be at offset 0)
        r._manager_fn = this->_manager_fn;
      }
      return r;
    }

    Ref decay() && {
      Ref r;
      if (this->_ptr) {
        assert(this->_manager_fn);
        r._ptr = static_cast<BaseRef *>(
            this->_manager_fn(PlacementNewAction::Move, r.buffer,
                              static_cast<BaseRef *>(this->_ptr)));
        //  NOTE: Reusing _manager_fn implies a need for single inheritance
        //  (base must be at offset 0)
        r._manager_fn = this->_manager_fn;
        this->cleanup();
      }
      return r;
    }
  };

  class Ref : public LocalPolymorphic<BaseRef, POLYMORPHIC_REF_BUFFER_SIZE> {
  public:
    using LocalPolymorphic<BaseRef,
                           POLYMORPHIC_REF_BUFFER_SIZE>::LocalPolymorphic;
    using LocalPolymorphic<BaseRef, POLYMORPHIC_REF_BUFFER_SIZE>::ptr;

    template <typename T> friend class RefTo;

    async::Awaitable<ObjectT *> resolve() {
      assert(ptr());
      co_return co_await ptr()->resolve();
    }

    const BaseStateT &state() const {
      assert(ptr());
      return ptr()->state();
    }

    template <typename T> RefTo<T> undecay() const & {
      RefTo<T> r;
      if (this->_ptr) {
        assert(this->_manager_fn);

        BaseRefTo<T> *target = dynamic_cast<BaseRefTo<T> *>(this->_ptr);
        if (!target) {
          throw std::bad_cast();
        }

        r._ptr = static_cast<BaseRefTo<T> *>(
            this->_manager_fn(PlacementNewAction::Copy, r.buffer, this->_ptr));
        //  NOTE: Reusing _manager_fn implies a need for single inheritance
        //  (base must be at offset 0)
        r._manager_fn = this->_manager_fn;
      }
      return r;
    }

    template <typename T> RefTo<T> undecay() && {
      RefTo<T> r;
      if (this->_ptr) {
        assert(this->_manager_fn);

        BaseRefTo<T> *target = dynamic_cast<BaseRefTo<T> *>(this->_ptr);
        if (!target) {
          throw std::bad_cast();
        }

        r._ptr = static_cast<BaseRefTo<T> *>(
            this->_manager_fn(PlacementNewAction::Move, r.buffer, this->_ptr));
        //  NOTE: Reusing _manager_fn implies a need for single inheritance
        //  (base must be at offset 0)
        r._manager_fn = this->_manager_fn;
        this->cleanup();
      }
      return r;
    }
  };
};
