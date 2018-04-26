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

#include <memory>
#include <exception>
#include <type_traits>
#include <cassert>

namespace pobu
{

namespace detail
{
template<typename> class unique_ptr_link;
}

template<typename T>
detail::unique_ptr_link<T> link(const std::unique_ptr<T>& u) noexcept;

template<typename R, typename T>
detail::unique_ptr_link<R> link(const std::unique_ptr<T>& u) noexcept;

namespace detail
{
constexpr static std::uint8_t _ptr = 0;
constexpr static std::uint8_t _acquired = 1;
constexpr static std::uint8_t _deleted = 2;

using control_block = std::tuple<void*, bool, bool>;

template<typename T>
struct owned_deleter
{
  void operator()(control_block *const cb)
  {
#ifdef OWNED_POINTER_ASSERT_DTOR
    assert((not std::get<_acquired>(*cb)) and "ASSERT: you created owned_pointer, but unique_ptr was never acquired");
#endif

    if(not std::get<_acquired>(*cb))
      delete static_cast<T*>(std::get<_ptr>(*cb));

    delete cb;
  }
};

class shared_secret
{
public:
  std::weak_ptr<control_block> weak_control_block;
  virtual ~shared_secret() = default;
protected:
  void delete_event()
  {
    if(auto p = weak_control_block.lock()) std::get<_deleted>(*p) = true;
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
struct unique_ptr_link
{
  unique_ptr_link(unique_ptr_link&&) = default;
  unique_ptr_link(const unique_ptr_link&) = delete;
  unique_ptr_link& operator=(unique_ptr_link&&) = delete;
  unique_ptr_link& operator=(const unique_ptr_link&) = delete;

  template<typename R>
  explicit unique_ptr_link(const std::unique_ptr<R>& u) : passed_pointer(u.get()) {}

  T* const passed_pointer;
};

} // namespace detail

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
class owned_pointer : private std::shared_ptr<detail::control_block>
{
  static_assert(!std::is_array<Tp>::value, "owned_pointer doesn't support arrays");

  template <typename> friend class owned_pointer;
  using base = std::shared_ptr<detail::control_block>;

public:
  using element_type = Tp;
  using upointer_type = std::unique_ptr<element_type>;
  using base::use_count;

  constexpr owned_pointer() noexcept = default;
  constexpr owned_pointer(std::nullptr_t) noexcept {}

  template<typename T>
  owned_pointer(std::unique_ptr<T>&& p)
  {
    if(not p) return;

    constexpr bool not_acquired = false;
    operator=(owned_pointer<T>(p.release(), not_acquired));
  }

  template<typename T>
  owned_pointer(detail::unique_ptr_link<T>&& p)
  {
    if(not p.passed_pointer) return;

    constexpr bool is_acquired = true;
    operator=(owned_pointer<T>(p.passed_pointer, is_acquired));
  }

  template<typename T>
  owned_pointer(const owned_pointer<T>& op) noexcept
  {
    operator=(op);
  }

  template<typename T>
  owned_pointer(owned_pointer<T>&& op) noexcept : base{std::move(op)}
  {
    static_assert(std::is_same<element_type,T>::value or std::is_base_of<element_type,T>::value,
                  "Assigning pointer of different or non-derived type");
  }

  template<typename T>
  owned_pointer& operator=(const owned_pointer<T>& op) noexcept
  {
    static_assert(std::is_same<element_type,T>::value or std::is_base_of<element_type,T>::value,
                  "Assigning pointer of different or non-derived type");

    base::operator=(op);
    return *this;
  }

  template<typename T>
  owned_pointer& operator=(owned_pointer<T>&& op) noexcept
  {
    static_assert(std::is_same<element_type,T>::value or std::is_base_of<element_type,T>::value,
                  "Assigning pointer of different or non-derived type");

    base::operator=(std::move(op));
    return *this;
  }

  element_type* get() const
  {
    throw_when_ptr_expired_and_virtual_dtor_is_present();
    return get_pointer();
  }

  element_type* operator->() const
  {
    throw_when_ptr_expired_and_virtual_dtor_is_present();
    return get_pointer();
  }

  element_type& operator*() const
  {
    throw_when_ptr_expired_and_virtual_dtor_is_present();
    return *get_pointer();
  }

  upointer_type unique_ptr() const
  {
    if(get_pointer())
    {
      if(not acquired())
      {
        std::get<detail::_acquired>(base::operator*()) = true;
        return upointer_type{get_pointer()};
      }
      throw unique_ptr_already_acquired();
    }
    return nullptr;
  }

  bool acquired() const noexcept
  {
    return base::operator bool() and std::get<detail::_acquired>(base::operator*());
  }

  bool expired() const noexcept
  {
    return base::operator bool() and std::get<detail::_deleted>(base::operator*());
  }

  explicit operator upointer_type() const
  {
    return unique_ptr();
  }

  explicit operator bool() const noexcept
  {
    return get_pointer() != nullptr;
  }

  std::int8_t compare(const void* const ptr) const noexcept
  {
    const void* a = get_pointer();
    return a == ptr ? std::int8_t{0} : (a < ptr ? std::int8_t{-1} : std::int8_t{+1});
  }

  template<typename T>
  std::int8_t compare(const owned_pointer<T>& p) const noexcept
  {
    return compare(p.get_pointer());
  }

private:
  element_type* get_pointer() const noexcept
  {
    if(__builtin_expect(base::operator bool(), true))
      return static_cast<element_type*>(std::get<detail::_ptr>(base::operator*()));

    return nullptr;
  }

  owned_pointer(element_type *const p, const bool acquired)
  {
    auto ss = get_secret_when_possible(p);
    if(!base::operator bool())
    {
      base::reset(
        new detail::control_block(p, {}, {}),
        detail::owned_deleter<element_type>()
      );
      set_shared_secret_when_possible(ss);
    }
    std::get<detail::_acquired>(base::operator*()) = acquired;
  }

  void throw_when_ptr_expired_and_virtual_dtor_is_present() const
  {
    if(__builtin_expect(expired(), false))
      throw ptr_is_already_deleted();
  }

  template<typename T>
  typename std::enable_if<std::is_polymorphic<T>::value, detail::shared_secret*>::type
  get_secret_when_possible(T *const p) noexcept
  {
    if(auto ss = dynamic_cast<detail::shared_secret*>(p))
    {
      base::operator=(ss->weak_control_block.lock());
      return ss;
    }
    return nullptr;
  }

  template<typename T>
  typename std::enable_if<!std::is_polymorphic<T>::value, detail::shared_secret*>::type
  get_secret_when_possible(T *const p) noexcept
  {
    return nullptr;
  }

  void set_shared_secret_when_possible(detail::shared_secret *const p)
  {
    if(p != nullptr)
      p->weak_control_block = *this;
  }
};

template<> struct owned_pointer<void>{};

template<typename T, typename... Args>
inline typename std::enable_if<
  !std::is_array<T>::value,
  owned_pointer<T>
>::type make_owned(Args&&... args)
{
  using ptr_type = typename std::conditional<
    std::has_virtual_destructor<T>::value,
    detail::destruction_notify_object<T>,
    T
  >::type;

  return std::unique_ptr<T>(new ptr_type{std::forward<Args>(args)...});
}

template<typename T>
detail::unique_ptr_link<T> link(const std::unique_ptr<T>& u) noexcept
{
  return detail::unique_ptr_link<T>(u);
}

template<typename R, typename T>
detail::unique_ptr_link<R> link(const std::unique_ptr<T>& u) noexcept
{
  return detail::unique_ptr_link<R>(u);
}

template<typename T1>
inline bool operator==(const owned_pointer<T1>& p1, std::nullptr_t) noexcept
{
  return p1.compare(nullptr) == 0;
}

template<typename T1>
inline bool operator!=(const owned_pointer<T1>& p1, std::nullptr_t) noexcept
{
  return p1.compare(nullptr) != 0;
}

template<typename T1>
inline bool operator==(std::nullptr_t, const owned_pointer<T1>& p1) noexcept
{
  return p1 == nullptr;
}

template<typename T1>
inline bool operator!=(std::nullptr_t, const owned_pointer<T1>& p1) noexcept
{
  return p1 != nullptr;
}

template<typename T1, typename T2>
inline bool operator==(const owned_pointer<T1>& p1, const owned_pointer<T2>& p2) noexcept
{
  return p1.compare(p2) == 0;
}

template<typename T1, typename T2>
inline bool operator!=(const owned_pointer<T1>& p1, const owned_pointer<T2>& p2) noexcept
{
  return not(p1 == p2);
}

template<typename T1, typename T2>
inline bool operator<(const owned_pointer<T1>& p1, const owned_pointer<T2>& p2) noexcept
{
  return p1.compare(p2) < 0;
}

template<typename T1, typename T2>
inline bool operator<=(const owned_pointer<T1>& p1, const owned_pointer<T2>& p2) noexcept
{
  return p1.compare(p2) <= 0;
}

template<typename T1, typename T2>
inline bool operator>(const owned_pointer<T1>& p1, const owned_pointer<T2>& p2) noexcept
{
  return p1.compare(p2) > 0;
}

template<typename T1, typename T2>
inline bool operator>=(const owned_pointer<T1>& p1, const owned_pointer<T2>& p2) noexcept
{
  return p1.compare(p2) >= 0;
}

template<typename T1, typename T2>
inline bool operator==(const owned_pointer<T1>& p1, const T2* p2) noexcept
{
  return p1.compare(p2) == 0;
}

template<typename T1, typename T2>
inline bool operator==(const T1* p1, const owned_pointer<T2>& p2) noexcept
{
  return p2.compare(p1) == 0;
}

template<typename T1, typename T2>
inline bool operator!=(const owned_pointer<T1>& p1, const T2* p2) noexcept
{
  return p1.compare(p2) != 0;
}

template<typename T1, typename T2>
inline bool operator!=(const T1* p1, const owned_pointer<T2>& p2) noexcept
{
  return p2.compare(p1) != 0;
}

template<typename T1, typename T2>
inline bool operator==(const owned_pointer<T1>& p1, const std::unique_ptr<T2>& p2) noexcept
{
  return p1.compare(p2.get()) == 0;
}

template<typename T1, typename T2>
inline bool operator==(const std::unique_ptr<T1>& p1, const owned_pointer<T2>& p2) noexcept
{
  return p2.compare(p1.get()) == 0;
}

template<typename T1, typename T2>
inline bool operator!=(const owned_pointer<T1>& p1, const std::unique_ptr<T2>& p2) noexcept
{
  return p1.compare(p2.get()) != 0;
}

template<typename T1, typename T2>
inline bool operator!=(const std::unique_ptr<T1>& p1, const owned_pointer<T2>& p2) noexcept
{
  return p2.compare(p1.get()) != 0;
}

template<typename R, typename T>
owned_pointer<R> ptr_static_cast(const owned_pointer<T>& p)
{
  return owned_pointer<R>(p);
}

} //namespace pobu
