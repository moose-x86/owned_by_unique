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
struct deleter
{
  void operator()(control_block *const cb)
  {
    #ifdef OWNED_BY_UNIQUE_ASSERT_DTOR
    assert((not std::get<_acquired>(*cb)) and "ASSERT: you created owned_pointer, but unique_ptr was never acquired");
    #endif

    std::unique_ptr<control_block> u{cb};

    if(not std::get<_acquired>(*u))
      delete static_cast<T*>(std::get<_ptr>(*u));
  }
};

struct shared_secret
{
  virtual ~shared_secret() = default;
  std::weak_ptr<control_block> weak_control_block;
};

template<typename _base>
struct dtor_notify_enabled : public _base, public shared_secret
{
  using _base::_base;
  ~dtor_notify_enabled() override
  {
    if(auto p = weak_control_block.lock()) std::get<_deleted>(*p) = true;
  }
};

template<typename _Tp1>
class unique_ptr_link
{
  unique_ptr_link(const unique_ptr_link&) = delete;
  unique_ptr_link& operator=(unique_ptr_link&&) = delete;
  unique_ptr_link& operator=(const unique_ptr_link&) = delete;

public:
  _Tp1* const _ptr;

  template<typename _Tp2>
  explicit unique_ptr_link(const std::unique_ptr<_Tp2>& u) : _ptr(u.get()) {}

  unique_ptr_link(unique_ptr_link&&) = default;
};
} // namespace detail


struct unique_ptr_already_acquired : public std::runtime_error
{
  unique_ptr_already_acquired() :
        std::runtime_error("owned_pointer: This pointer is already acquired by unique_ptr")
  {}
};

struct ptr_is_already_deleted : public std::runtime_error
{
  ptr_is_already_deleted() :
        std::runtime_error("owned_pointer: This pointer is already deleted")
  {}
};

template<typename _Tp1>
class owned_pointer : private std::shared_ptr<detail::control_block>
{
  static_assert(not std::is_array<_Tp1>::value, "owned_pointer doesn't support arrays");

  constexpr static const bool is_acquired_true = true;
  template <typename> friend class owned_pointer;

  using base = std::shared_ptr<detail::control_block>;

public:
  using element_type = _Tp1;
  using unique_ptr_t = std::unique_ptr<element_type>;
  using base::use_count;

  owned_pointer(std::nullptr_t) : owned_pointer{} {}
  owned_pointer() : owned_pointer{nullptr, not is_acquired_true} {}

  template<typename _Tp2>
  owned_pointer(detail::unique_ptr_link<_Tp2>&& pointee)
  {
    *this = owned_pointer<_Tp2>{pointee._ptr, is_acquired_true};
  }

  template<typename _Tp2>
  owned_pointer(std::unique_ptr<_Tp2>&& pointee)
  {
    *this = owned_pointer<_Tp2>{pointee.release(), not is_acquired_true};
  }

  template<typename _Tp2>
  owned_pointer(const owned_pointer<_Tp2>& pointee) noexcept
  {
    *this = pointee;
  }

  template<typename _Tp2>
  owned_pointer& operator=(const owned_pointer<_Tp2>& pointee) noexcept
  {
    static_assert(std::is_same   <element_type, _Tp2>::value or
                  std::is_base_of<element_type, _Tp2>::value,
                  "Assigning pointer of different or non-derived type");

    base::operator=(pointee);
    return *this;
  }

  element_type* get() const
  {
    throw_if_is_destroyed_and_has_virtual_dtor();
    return get_pointer();
  }

  element_type* operator->() const
  {
    throw_if_is_destroyed_and_has_virtual_dtor();
    return get_pointer();
  }

  element_type& operator*() const
  {
    throw_if_is_destroyed_and_has_virtual_dtor();
    return *get_pointer();
  }

  unique_ptr_t unique_ptr() const
  {
    if(get_pointer())
    {
      if(not acquired())
      {
        std::get<detail::_acquired>(base::operator*()) = true;
        return unique_ptr_t{get_pointer()};
      }
      throw unique_ptr_already_acquired{};
    }
    return nullptr;
  }

  bool acquired() const noexcept
  {
      return std::get<detail::_acquired>(base::operator*());
  }

  bool expired() const noexcept
  {
      return std::get<detail::_deleted>(base::operator*());
  }

  explicit operator unique_ptr_t() const
  {
      return unique_ptr();
  }

  explicit operator bool() const noexcept
  {
      return get_pointer() != nullptr;
  }

  std::int8_t compare(const void* const ptr) const noexcept
  {
    const std::ptrdiff_t diff = get_pointer() - static_cast<const element_type*>(ptr);
    return diff ? ( diff > 0 ? 1 : -1 ) : 0;
  }

  template<typename _Tp2>
  std::int8_t compare(const owned_pointer<_Tp2>& p) const noexcept
  {
      return compare(p.get_pointer());
  }

private:
  element_type* get_pointer() const { return static_cast<element_type*>(std::get<detail::_ptr>(base::operator*())); }

  owned_pointer(element_type *const pointee, const bool acquired)
  {
    auto ss = acquire_is_destroyed_flag_if_possible(pointee);
    if(not base::operator bool())
    {
       auto cb = new detail::control_block{pointee, {}, {}};
       base::operator=(base{cb, detail::deleter<element_type>{}});
       set_shared_secret_when_possible(ss);
    }

    std::get<detail::_acquired>(base::operator*()) = acquired;
  }

  void throw_if_is_destroyed_and_has_virtual_dtor() const
  {
     if(expired())
       throw ptr_is_already_deleted{};
  }

  template<typename _Tp2>
  typename std::enable_if<std::is_polymorphic<_Tp2>::value, detail::shared_secret*>::type
  acquire_is_destroyed_flag_if_possible(_Tp2 *const p)
  {
    auto ss = dynamic_cast<detail::shared_secret*>(p);
    if(ss) base::operator=(ss->weak_control_block.lock());

    return ss;
  }

  template<typename _Tp2>
  typename std::enable_if<!std::is_polymorphic<_Tp2>::value, detail::shared_secret*>::type
  acquire_is_destroyed_flag_if_possible(_Tp2 *const p) { return nullptr; }

  void set_shared_secret_when_possible(detail::shared_secret *const p)
  {
    if(p) p->weak_control_block = *this;
  }
};

template< typename _PT, typename... Args >
inline typename std::enable_if<not std::is_array<_PT>::value, owned_pointer<_PT> >::type
make_owned(Args&&... args)
{
  using pointee_t = typename std::conditional
  <
    std::has_virtual_destructor<_PT>::value,
    detail::dtor_notify_enabled<_PT>,
    _PT
  >::type;

  return { std::unique_ptr<pointee_t>(new pointee_t{std::forward<Args>(args)...}) };
}

template<> struct owned_pointer<void> {};

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
inline bool operator!=(const std::unique_ptr<T1> p1, const owned_pointer<T2>& p2) noexcept
{
  return p2.compare(p1.get()) != 0;
}

} //namespace pobu
