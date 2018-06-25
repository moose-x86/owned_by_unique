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

#include <new>
#include <tuple>
#include <memory>
#include <cassert>
#include <exception>
#include <type_traits>

namespace csp
{

namespace _priv
{
template<typename> class link_ptr;
}

template<typename T>
auto link(const std::unique_ptr<T>& u) noexcept -> _priv::link_ptr<T>;

template<typename R, typename T>
auto link(const std::unique_ptr<T>& u) noexcept -> _priv::link_ptr<R>;

namespace _priv
{

using control_block_type = std::tuple<void*, bool, bool>;

const auto ptr      = +[](control_block_type& cb) -> void* { return std::get<0>(cb); };
const auto deleted  = +[](control_block_type& cb) -> bool& { return std::get<2>(cb); };
const auto acquired = +[](control_block_type& cb) -> bool& { return std::get<1>(cb); };

template<typename T>
struct owned_deleter
{
  void operator()(control_block_type *const cb)
  {
#ifdef OWNED_POINTER_ASSERT_DTOR
    assert(acquired(*cb) && "ASSERT: you created owned_pointer, but unique_ptr was never acquired");
#else
    if(!acquired(*cb))
      delete static_cast<T*>(ptr(*cb));
#endif
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
    if(auto p{control_block.lock()}) deleted(*p) = true;
  }
};

template<typename Base>
struct destruction_notify_object : Base, shared_secret
{
   using Base::Base;
   ~destruction_notify_object() override { delete_event(); }
};

template<typename T>
class link_ptr
{
public:
  template<typename R>
  explicit link_ptr(const std::unique_ptr<R>& u) noexcept : ptr{u.get()} {}

  link_ptr(const link_ptr&) = delete;
  link_ptr(link_ptr&&) noexcept = default;
  link_ptr& operator=(link_ptr&&) = delete;
  link_ptr& operator=(const link_ptr&) = delete;

  auto get() const noexcept -> T* { return ptr; }

private:
  T* const ptr;
};

} // namespace _priv

template<typename Tp>
class owned_pointer : std::shared_ptr<_priv::control_block_type>
{
  static_assert(!std::is_array<Tp>::value && !std::is_pointer<Tp>::value, "no array nor pointer supported");
  using base_type = std::shared_ptr<_priv::control_block_type>;

  template<typename>
  friend class owned_pointer;

public:
  using element_type = Tp;
  using base_type::use_count;
  using uptr_type = std::unique_ptr<element_type>;

  constexpr owned_pointer() noexcept = default;
  constexpr owned_pointer(std::nullptr_t) noexcept {}

  template<typename T>
  owned_pointer(_priv::link_ptr<T>&& p) : owned_pointer(p.get(), true) {}

  template<typename T>
  owned_pointer(std::unique_ptr<T>&& p) : owned_pointer(p.release(), false) {}

  auto get() const -> element_type*;
  explicit operator uptr_type() const;
  auto unique_ptr() const -> uptr_type;
  auto expired() const noexcept -> bool;
  auto raw_ptr() const -> element_type*;
  auto acquired() const noexcept -> bool;
  explicit operator bool() const noexcept;
  auto operator*() const -> element_type&;
  auto operator->() const -> element_type*;
  auto get(std::nothrow_t) const noexcept -> element_type*;

  template<typename T>
  operator owned_pointer<T>() const noexcept;

  template<typename T>
  auto compare(const T& ptr) const noexcept -> std::int8_t;

  template<typename T>
  auto compare(const owned_pointer<T>& p) const noexcept -> std::int8_t;

private:
  owned_pointer(element_type *const p, const bool acquired);

  auto stored_address() const noexcept -> element_type*;
  void throw_when_ptr_expired_and_object_has_virtual_dtor() const;

  template<typename T, typename = typename std::enable_if<std::is_polymorphic<T>::value, void>::type>
  auto get_secret_when_possible(T *const p) noexcept -> _priv::shared_secret*
  {
    if(auto ss{dynamic_cast<_priv::shared_secret*>(p)})
      return base_type::operator=(ss->control_block.lock()), ss;

    return nullptr;
  }

  auto get_secret_when_possible(...) noexcept -> _priv::shared_secret*
  {
    return nullptr;
  }

  void set_shared_secret_when_possible(_priv::shared_secret *const ss) const
  {
    if(ss != nullptr)
      ss->control_block = *this;
  }
};

template<>
struct owned_pointer<void>
{};

#if __cplusplus >= 201703L
template<typename T>
owned_pointer(std::unique_ptr<T>&&) -> owned_pointer<T>;

template<typename T>
owned_pointer(_priv::link_ptr<T>&&) -> owned_pointer<T>;
#endif

/*****************************************************************************************
 *
 * Public Exceptions
 *
 *****************************************************************************************/

struct unique_ptr_already_acquired : public std::runtime_error
{
  unique_ptr_already_acquired() : std::runtime_error("owned_pointer: This pointer is already acquired by unique_ptr") {}
};

struct ptr_is_already_deleted : public std::runtime_error
{
  ptr_is_already_deleted() : std::runtime_error("owned_pointer: This pointer is already deleted") {}
};

/*****************************************************************************************
 *
 * Public member class functions
 *
 *****************************************************************************************/

template<typename T>
inline auto owned_pointer<T>::get() const -> element_type*
{
  throw_when_ptr_expired_and_object_has_virtual_dtor();
  return stored_address();
}

template<typename T>
inline auto owned_pointer<T>::operator->() const -> element_type*
{
  return get();
}

template<typename T>
inline auto owned_pointer<T>::operator*() const -> element_type&
{
  return *get();
}

template<typename T>
inline auto owned_pointer<T>::unique_ptr() const -> uptr_type
{
  if(!get())
    return uptr_type { nullptr };

  if(acquired())
    throw unique_ptr_already_acquired();

  return _priv::acquired(base_type::operator*()) = true, uptr_type{stored_address()};
}

template<typename T>
inline auto owned_pointer<T>::raw_ptr() const -> element_type*
{
  return unique_ptr().release();
}

template<typename T>
inline auto owned_pointer<T>::acquired() const noexcept -> bool
{
  return base_type::operator bool() && _priv::acquired(base_type::operator*());
}

template<typename T>
inline auto owned_pointer<T>::expired() const noexcept -> bool
{
  return base_type::operator bool() && _priv::deleted(base_type::operator*());
}

template<typename T>
inline owned_pointer<T>::operator uptr_type() const
{
  return unique_ptr();
}

template<typename T>
inline owned_pointer<T>::operator bool() const noexcept
{
  return stored_address();
}

template<typename T>
inline auto owned_pointer<T>::get(std::nothrow_t) const noexcept -> element_type*
{
  return expired() ? nullptr : stored_address();
}

template<typename R> template<typename T>
inline owned_pointer<R>::operator owned_pointer<T>() const noexcept
{
  static_assert(std::is_convertible<element_type*, T*>::value,
                "Casting to pointer of different or non-derived type");

  owned_pointer<T> casted{nullptr};
  return(static_cast<base_type&>(casted) = *this, casted);
}

template<typename R> template<typename T>
inline auto owned_pointer<R>::compare(const T& ptr) const noexcept -> std::int8_t
{
  static_assert(std::is_convertible<T, element_type*>::value ||
                std::is_convertible<element_type*, T>::value,
                "Comparing pointer of different or non-derived type");

  const auto addr{stored_address()};
  return addr == ptr ? 0 : (addr < ptr ? -1 : +1);
}

template<typename R> template<typename T>
inline auto owned_pointer<R>::compare(const owned_pointer<T>& p) const noexcept -> std::int8_t
{
  return compare(p.stored_address());
}
/*****************************************************************************************
 *
 * Private member class functions
 *
 *****************************************************************************************/

template<typename T>
auto owned_pointer<T>::stored_address() const noexcept -> element_type*
{
   return base_type::operator bool() ?
       static_cast<element_type*>(_priv::ptr(base_type::operator*())) : nullptr;
}

template<typename T>
void owned_pointer<T>::throw_when_ptr_expired_and_object_has_virtual_dtor() const
{
  if(expired())
    throw ptr_is_already_deleted();
}

template<typename T>
owned_pointer<T>::owned_pointer(element_type *const p, const bool acquired)
{
  if(!p) return;
  const auto ss = get_secret_when_possible(p);

  if(!base_type::operator bool())
  {
    base_type::reset(
      new _priv::control_block_type(p, {}, {}),
      _priv::owned_deleter<element_type>()
    );
    set_shared_secret_when_possible(ss);
  }
  _priv::acquired(base_type::operator*()) = acquired;
}

/*****************************************************************************************
 *
 * Public non-member functions
 *
 *****************************************************************************************/

template<typename T>
struct is_nothrow_dereferencable :
  std::integral_constant<bool, !std::has_virtual_destructor<T>::value
                                #if __cplusplus >= 201402L
                                  || std::is_final_v<T>
                                #endif
                        > {};

template<typename T>
struct is_nothrow_dereferencable<owned_pointer<T>> :
  std::integral_constant<bool, is_nothrow_dereferencable<T>::value> {};

template<typename T>
struct is_nothrow_dereferencable<owned_pointer<T>&> :
  std::integral_constant<bool, is_nothrow_dereferencable<T>::value> {};

template<typename T>
struct is_nothrow_dereferencable<const owned_pointer<T>&> :
  std::integral_constant<bool, is_nothrow_dereferencable<T>::value> {};

template<typename T>
struct is_nothrow_dereferencable<const owned_pointer<T>> :
  std::integral_constant<bool, is_nothrow_dereferencable<T>::value> {};

template<typename T>
struct is_nothrow_dereferencable<owned_pointer<T>&&> :
  std::integral_constant<bool, is_nothrow_dereferencable<T>::value> {};

template<typename T>
struct is_nothrow_dereferencable<const owned_pointer<T>&&> :
  std::integral_constant<bool, is_nothrow_dereferencable<T>::value> {};

#if __cplusplus >= 201402L
template<typename T>
constexpr bool is_nothrow_dereferencable_v{is_nothrow_dereferencable<T>::value};
#endif

template<typename T>
inline bool is_nothrow_dereferencable_f(const owned_pointer<T>& p) noexcept
{
  return is_nothrow_dereferencable<decltype(p)>::value;
}

template<typename Object, typename... Args>
inline auto make_owned(Args&&... args) -> owned_pointer<Object>
{
  using C = std::conditional<
               !is_nothrow_dereferencable<Object>::value,
               _priv::destruction_notify_object<Object> ,
               Object
            >;

  return std::unique_ptr<Object>(
                  new typename C::type{ std::forward<Args>(args)... });
}

template<typename T>
inline auto link(const std::unique_ptr<T>& u) noexcept -> _priv::link_ptr<T>
{
  return _priv::link_ptr<T>{u};
}

template<typename R, typename T>
inline auto link(const std::unique_ptr<T>& u) noexcept -> _priv::link_ptr<R>
{
  return _priv::link_ptr<R>{u};
}

template<typename To, typename From>
inline auto static_pointer_cast(const owned_pointer<From>& from) noexcept -> owned_pointer<To>
{
  return { from };
}

template<typename T, typename F>
inline auto dynamic_pointer_cast(const owned_pointer<F>& from) noexcept -> owned_pointer<T>
{
  static_assert(is_nothrow_dereferencable<F>::value, "Only possible for polymorphic types");

  if (!from.expired())
  {
    if (auto p = dynamic_cast<T*>( from.operator->() ))
    {
      return std::unique_ptr<T>{p};
    }
  }

  return nullptr;
}

/*****************************************************************************************
 *
 * Public compare operators
 *
 *****************************************************************************************/

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

} //namespace csp
