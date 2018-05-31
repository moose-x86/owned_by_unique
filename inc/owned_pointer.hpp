/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Przemyslaw Wos
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
**/
#pragma once

#include <tuple>
#include <memory>
#include <cassert>
#include <exception>
#include <new>
#include <type_traits>

namespace pobu
{

namespace __priv
{
template<typename> struct uptr_link;
}

template<typename T>
__priv::uptr_link<T> link(const std::unique_ptr<T>& u) noexcept;

template<typename R, typename T>
__priv::uptr_link<R> link(const std::unique_ptr<T>& u) noexcept;

namespace __priv
{
constexpr const std::uint8_t _ptr{0};
constexpr const std::uint8_t _acquired{1};
constexpr const std::uint8_t _deleted{2};

using control_block_type = std::tuple<void*, bool, bool>;

template<typename T>
struct owned_deleter
{
  void operator()(control_block_type *const cb)
  {
#ifdef OWNED_POINTER_ASSERT_DTOR
    assert((!std::get<_acquired>(*cb)) && "ASSERT: you created owned_pointer, but unique_ptr was never acquired");
#endif

    if(!std::get<_acquired>(*cb))
      delete static_cast<T*>(std::get<_ptr>(*cb));

    delete cb;
  }
};

class shared_secret
{
public:
  std::weak_ptr<control_block_type> control_block;
  virtual ~shared_secret() = default;

protected:
  void delete_event() noexcept
  {
    if(const auto& p = control_block.lock()) std::get<_deleted>(*p) = true;
  }
};

template<typename base>
struct destruction_notify_object : base, shared_secret
{
   using base::base;
   ~destruction_notify_object() override
   {
     delete_event();
   }
};

template<typename T>
struct uptr_link
{
  uptr_link(uptr_link&&) = default;
  uptr_link(const uptr_link&) = delete;
  uptr_link& operator=(uptr_link&&) = delete;
  uptr_link& operator=(const uptr_link&) = delete;

  template<typename R>
  explicit uptr_link(const std::unique_ptr<R>& u) noexcept : naked_pointer(u.get()) {}

  T* const naked_pointer;
};

} // namespace __priv

struct unique_ptr_already_acquired : public std::runtime_error
{
  unique_ptr_already_acquired() : std::runtime_error("owned_pointer: This pointer is already acquired by unique_ptr")
  {}
};

struct ptr_is_already_deleted : public std::runtime_error
{
  ptr_is_already_deleted() : std::runtime_error("owned_pointer: This pointer is already deleted")
  {}
};

template<typename Tp>
class owned_pointer : private std::shared_ptr<__priv::control_block_type>
{
  static_assert(!std::is_array<Tp>::value && !std::is_pointer<Tp>::value, "no array nor pointer supported");
  using base = std::shared_ptr<__priv::control_block_type>;

  template <typename>
  friend class owned_pointer;

public:
  using element_type = Tp;
  using upointer_type = std::unique_ptr<element_type>;
  using base::use_count;

  constexpr owned_pointer() noexcept = default;
  constexpr owned_pointer(std::nullptr_t) noexcept {}

  template<typename T>
  owned_pointer(std::unique_ptr<T>&& p)
  {
    if(!p) return;

    constexpr bool not_acquired = false;
    operator=(owned_pointer<T>(p.release(), not_acquired));
  }

  template<typename T>
  owned_pointer(__priv::uptr_link<T>&& p)
  {
    if(!p.naked_pointer) return;

    constexpr bool is_acquired = true;
    operator=(owned_pointer<T>(p.naked_pointer, is_acquired));
  }

  template<typename T>
  owned_pointer(const owned_pointer<T>& op) noexcept
  {
    operator=(op);
  }

  template<typename T>
  owned_pointer(owned_pointer<T>&& op) noexcept
  {
    operator=(std::move(op));
  }

  template<typename T>
  owned_pointer& operator=(const owned_pointer<T>& op) noexcept
  {
    static_assert(std::is_convertible<T*, element_type*>::value, "Assigning pointer of different or non-derived type");

    base::operator=(op);
    return *this;
  }

  template<typename T>
  owned_pointer& operator=(owned_pointer<T>&& op) noexcept
  {
    static_assert(std::is_convertible<T*, element_type*>::value, "Assigning pointer of different or non-derived type");

    base::operator=(std::move(op));
    return *this;
  }

  element_type* get() const
  {
    throw_when_ptr_expired_and_object_has_virtual_dtor();
    return stored_address();
  }

  element_type* operator->() const
  {
    return get();
  }

  element_type& operator*() const
  {
    return *get();
  }

  upointer_type unique_ptr() const
  {
    if(get() != nullptr)
    {
      if(!acquired())
      {
        std::get<__priv::_acquired>(base::operator*()) = true;
        return upointer_type{ stored_address () };
      }
      throw unique_ptr_already_acquired();
    }
    return nullptr;
  }

  bool acquired() const noexcept
  {
    return base::operator bool() && std::get<__priv::_acquired>(base::operator*());
  }

  bool expired() const noexcept
  {
    return base::operator bool() && std::get<__priv::_deleted>(base::operator*());
  }

  explicit operator upointer_type() const
  {
    return unique_ptr();
  }

  explicit operator bool() const noexcept
  {
    return stored_address() != nullptr;
  }
  template  <typename T>
  std::int8_t compare(const T* ptr) const noexcept
  {
    static_assert(std::is_convertible<T*, element_type*>::value || std::is_convertible<element_type*, T*>::value
    , "Comparing pointer of different or non-derived type");

    const void* this_ptr = stored_address();
    return this_ptr == (void*)ptr ? std::int8_t{0} : (this_ptr < (void*)ptr ? std::int8_t{-1} : std::int8_t{+1});
  }
  std::int8_t compare (std::nullptr_t) const noexcept
  {
    return compare<void>(nullptr);
  }
  
  template<typename T>
  std::int8_t compare(const owned_pointer<T>& p) const noexcept
  {
    return compare(p.stored_address());
  }
  
  element_type* get(std::nothrow_t) const noexcept
  {
    return !expired ()  ? stored_address() : nullptr;
  }
  
private:
  element_type* stored_address() const noexcept
  {
    return base::operator bool() ? static_cast<element_type*>(std::get<__priv::_ptr>(base::operator*())) : nullptr;
  }
  owned_pointer(element_type *const p, const bool acquired)
  {
    const auto ss = get_secret_when_possible(p);

    if(!base::operator bool())
    {
      constexpr bool is_not_aquired{false};
      constexpr bool is_not_deleted{false};

      base::reset(
        new __priv::control_block_type(p, is_not_aquired, is_not_deleted),
        __priv::owned_deleter<element_type>()
      );
      set_shared_secret_when_possible(ss);
    }
    std::get<__priv::_acquired>(base::operator*()) = acquired;
  }

  void throw_when_ptr_expired_and_object_has_virtual_dtor() const
  {
    if(expired())
      throw ptr_is_already_deleted();
  }

  template<typename T, typename = typename std::enable_if<std::is_polymorphic<T>::value, void>::type>
  __priv::shared_secret* get_secret_when_possible(T *const p) noexcept
  {
    if(const auto ss = dynamic_cast<__priv::shared_secret*>(p))
    {
      base::operator=(ss->control_block.lock());
      return ss;
    }
    return nullptr;
  }

  __priv::shared_secret* get_secret_when_possible(...) noexcept
  {
    return nullptr;
  }

  void set_shared_secret_when_possible(__priv::shared_secret *const ss)
  {
    if(ss != nullptr)
      ss->control_block = *this;
  }
};

template<> struct owned_pointer<void>{};

template<typename Object, typename... Args>
inline owned_pointer<Object> make_owned(Args&&... args)
{
  constexpr bool has_vdtor = std::has_virtual_destructor<Object>::value;
  using ptr_type = typename std::conditional<has_vdtor, __priv::destruction_notify_object<Object>, Object>::type;

  return std::unique_ptr<Object>(new ptr_type{std::forward<Args>(args)...});
}

template<typename T>
__priv::uptr_link<T> link(const std::unique_ptr<T>& u) noexcept
{
  return __priv::uptr_link<T>(u);
}

template<typename R, typename T>
__priv::uptr_link<R> link(const std::unique_ptr<T>& u) noexcept
{
  return __priv::uptr_link<R>(u);
}

template<typename A>
inline bool operator==(const owned_pointer<A>& p1, std::nullptr_t) noexcept
{
  return p1.compare(nullptr) == 0;
}

template<typename A>
inline bool operator!=(const owned_pointer<A>& p1, std::nullptr_t) noexcept
{
  return p1.compare(nullptr) != 0;
}

template<typename A>
inline bool operator==(std::nullptr_t, const owned_pointer<A>& p1) noexcept
{
  return p1 == nullptr;
}

template<typename A>
inline bool operator!=(std::nullptr_t, const owned_pointer<A>& p1) noexcept
{
  return p1 != nullptr;
}

template<typename A, typename B>
inline bool operator==(const owned_pointer<A>& p1, const owned_pointer<B>& p2) noexcept
{
  return p1.compare(p2) == 0;
}

template<typename A, typename B>
inline bool operator!=(const owned_pointer<A>& p1, const owned_pointer<B>& p2) noexcept
{
  return !(p1 == p2);
}

template<typename A, typename B>
inline bool operator<(const owned_pointer<A>& p1, const owned_pointer<B>& p2) noexcept
{
  return p1.compare(p2) < 0;
}

template<typename A, typename B>
inline bool operator<=(const owned_pointer<A>& p1, const owned_pointer<B>& p2) noexcept
{
  return p1.compare(p2) <= 0;
}

template<typename A, typename B>
inline bool operator>(const owned_pointer<A>& p1, const owned_pointer<B>& p2) noexcept
{
  return p1.compare(p2) > 0;
}

template<typename A, typename B>
inline bool operator>=(const owned_pointer<A>& p1, const owned_pointer<B>& p2) noexcept
{
  return p1.compare(p2) >= 0;
}

template<typename A, typename B>
inline bool operator==(const owned_pointer<A>& p1, const B* p2) noexcept
{
  return p1.compare(p2) == 0;
}

template<typename A, typename B>
inline bool operator==(const A* p1, const owned_pointer<B>& p2) noexcept
{
  return p2.compare(p1) == 0;
}

template<typename A, typename B>
inline bool operator!=(const owned_pointer<A>& p1, const B* p2) noexcept
{
  return p1.compare(p2) != 0;
}

template<typename A, typename B>
inline bool operator!=(const A* p1, const owned_pointer<B>& p2) noexcept
{
  return p2.compare(p1) != 0;
}

template<typename A, typename B>
inline bool operator==(const owned_pointer<A>& p1, const std::unique_ptr<B>& p2) noexcept
{
  return p1.compare(p2.get()) == 0;
}

template<typename A, typename B>
inline bool operator==(const std::unique_ptr<A>& p1, const owned_pointer<B>& p2) noexcept
{
  return p2.compare(p1.get()) == 0;
}

template<typename A, typename B>
inline bool operator!=(const owned_pointer<A>& p1, const std::unique_ptr<B>& p2) noexcept
{
  return p1.compare(p2.get()) != 0;
}

template<typename A, typename B>
inline bool operator!=(const std::unique_ptr<A>& p1, const owned_pointer<B>& p2) noexcept
{
  return p2.compare(p1.get()) != 0;
}

template<typename R, typename T>
owned_pointer<R> ptr_static_cast(const owned_pointer<T>& p)
{
  return owned_pointer<R>(p);
}

} //namespace pobu
