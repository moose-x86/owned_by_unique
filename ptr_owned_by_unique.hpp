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
detail::unique_ptr_link<T> link(const std::unique_ptr<T>& u);

template<typename R, typename T>
detail::unique_ptr_link<R> link(const std::unique_ptr<T>& u);

namespace detail
{
struct empty_base {};

template<typename _Base = empty_base>
struct dtor_notify_enabled : public _Base
{
  using _Base::_Base;
  virtual ~dtor_notify_enabled() { *is_destroyed = true; }

  const std::shared_ptr<bool> is_destroyed{std::make_shared<bool>(false)};
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
  unique_ptr_already_acquired()
      : std::runtime_error("ptr_owned_by_unique: This pointer is already acquired by unique_ptr")
  {}
};

struct ptr_is_already_deleted : public std::runtime_error
{
  ptr_is_already_deleted() : std::runtime_error("ptr_owned_by_unique: This pointer is already deleted")
  {}
};

template<typename _Tp1>
class ptr_owned_by_unique : private std::shared_ptr<_Tp1>
{
  using shared_ptr = std::shared_ptr<_Tp1>;
  template <typename> friend class ptr_owned_by_unique;

  constexpr static const bool is_acquired_true = true;

  static_assert(not std::is_array<_Tp1>::value, "ptr_owned_by_unique doesn't support arrays");

public:
  using element_type = _Tp1;
  using unique_ptr_t = std::unique_ptr<element_type>;
  using shared_ptr::operator bool;
  using shared_ptr::use_count;

  ptr_owned_by_unique(std::nullptr_t) : ptr_owned_by_unique() {}
  ptr_owned_by_unique() : ptr_owned_by_unique(nullptr, not is_acquired_true) {}

  template<typename _Tp2>
  ptr_owned_by_unique(detail::unique_ptr_link<_Tp2>&& pointee)
  {
    *this = ptr_owned_by_unique<_Tp2>(pointee._ptr, is_acquired_true);
  }

  template<typename _Tp2>
  ptr_owned_by_unique(std::unique_ptr<_Tp2>&& pointee)
  {
    *this = ptr_owned_by_unique<_Tp2>(pointee.release(), not is_acquired_true);
  }

  template<typename _Tp2>
  ptr_owned_by_unique(const ptr_owned_by_unique<_Tp2> &pointee)
  {
    *this = pointee;
  }

  template<typename _Tp2>
  ptr_owned_by_unique& operator=(const ptr_owned_by_unique<_Tp2> &pointee)
  {
    static_assert(std::is_same   <element_type, _Tp2>::value or
                  std::is_base_of<element_type, _Tp2>::value,
                  "Assigning pointer of different or non-derived type");

    acquire_share_resource(pointee);
    is_destroyed = pointee.is_destroyed;
    acquired_by_unique_ptr = pointee.acquired_by_unique_ptr;
    return *this;
  }

  element_type* get() const
  {
    throw_if_is_destroyed_and_has_virtual_dtor();
    return shared_ptr::get();
  }

  element_type* operator->() const
  {
    throw_if_is_destroyed_and_has_virtual_dtor();
    return shared_ptr::operator->();
  }

  element_type& operator*() const
  {
    throw_if_is_destroyed_and_has_virtual_dtor();
    return shared_ptr::operator*();
  }

  unique_ptr_t unique_ptr() const
  {
    if(shared_ptr::get())
    {
      if(not is_acquired())
      {
        *acquired_by_unique_ptr = true;
        return unique_ptr_t(get());
      }
      throw unique_ptr_already_acquired{};
    }
    return nullptr;
  }

  bool is_acquired() const { return *acquired_by_unique_ptr; }
  explicit operator unique_ptr_t() const { return unique_ptr(); }

  std::int8_t compare_ptr(const void* const ptr) const
  {
    const std::ptrdiff_t diff = shared_ptr::get() - reinterpret_cast<const _Tp1*>(ptr);
    return diff ? ( diff > 0 ? 1 : -1 ) : 0;
  }

  template<typename _Tp2>
  std::int8_t compare_ptr(const ptr_owned_by_unique<_Tp2>& p) const
  {
    return compare_ptr(p.std::template shared_ptr<_Tp2>::get());
  }

private:
  std::shared_ptr<const bool> is_destroyed;
  std::shared_ptr<bool> acquired_by_unique_ptr;

  struct deleter
  {
    deleter(std::shared_ptr<bool> a) : acquired(a){}
    void operator()(element_type *const pointee)
    {
      #ifdef OWNED_BY_UNIQUE_ASSERT_DTOR
        assert(*acquired and "ASSERT: you created ptr_owned_by_unique, but unique_ptr was never acquired");
      #endif
      if (not *acquired) delete pointee;
    }

  private:
    std::shared_ptr<const bool> acquired;
  };

  ptr_owned_by_unique(element_type *const pointee, const bool acquired)
  {
    acquired_by_unique_ptr = std::make_shared<bool>(acquired);
    shared_ptr::operator=(shared_ptr{pointee, deleter(acquired_by_unique_ptr)});

    acquire_is_destroyed_flag_if_possible(pointee);
  }

  void throw_if_is_destroyed_and_has_virtual_dtor() const
  {
    if(is_destroyed and *is_destroyed) throw ptr_is_already_deleted{};
  }

  template<typename _Tp2>
  void acquire_share_resource(const ptr_owned_by_unique<_Tp2> &pointee)
  {
    shared_ptr::operator=(pointee);
  }

  template<typename _Tp2>
  typename std::enable_if<std::is_polymorphic<_Tp2>::value,void>::type
  acquire_is_destroyed_flag_if_possible(_Tp2 *const p)
  {
    acquire_is_destroyed_flag_if_possible(dynamic_cast<detail::dtor_notify_enabled<_Tp2>*>(p));
  }

  template<typename _Tp2>
  void acquire_is_destroyed_flag_if_possible(detail::dtor_notify_enabled<_Tp2>* const pointee)
  {
    if(pointee)
        is_destroyed = pointee->is_destroyed;
  }

  template<typename _Tp2>
  typename std::enable_if<not std::is_polymorphic<_Tp2>::value, void>::type
  acquire_is_destroyed_flag_if_possible(_Tp2 *const p) {}
};

template< typename _PT, typename... Args >
inline typename std::enable_if<not std::is_array<_PT>::value, ptr_owned_by_unique<_PT> >::type
make_owned_by_unique(Args&&... args)
{
  using pointee_t = typename std::conditional <
    std::has_virtual_destructor<_PT>::value,
    detail::dtor_notify_enabled<_PT>,
    _PT
  >::type;

  return ptr_owned_by_unique<_PT> {
      std::unique_ptr<pointee_t>(new pointee_t{std::forward<Args>(args)...})
  };
}

template<> struct ptr_owned_by_unique<void> {};

template<typename T>
detail::unique_ptr_link<T> link(const std::unique_ptr<T>& u)
{
  return detail::unique_ptr_link<T>(u);
}

template<typename R, typename T>
detail::unique_ptr_link<R> link(const std::unique_ptr<T>& u)
{
  return detail::unique_ptr_link<R>(u);
}

template<typename T1>
inline bool operator==(const ptr_owned_by_unique<T1>& p1, std::nullptr_t)
{
  return p1.compare_ptr(nullptr) == 0;
}

template<typename T1>
inline bool operator!=(const ptr_owned_by_unique<T1>& p1, std::nullptr_t)
{
  return p1.compare_ptr(nullptr) != 0;
}

template<typename T1>
inline bool operator==(std::nullptr_t, const ptr_owned_by_unique<T1>& p1)
{
  return p1 == nullptr;
}

template<typename T1>
inline bool operator!=(std::nullptr_t, const ptr_owned_by_unique<T1>& p1)
{
  return p1 != nullptr;
}

template<typename T1, typename T2>
inline bool operator==(const ptr_owned_by_unique<T1>& p1, const ptr_owned_by_unique<T2>& p2)
{
  return p1.compare_ptr(p2) == 0;
}

template<typename T1, typename T2>
inline bool operator!=(const ptr_owned_by_unique<T1>& p1, const ptr_owned_by_unique<T2>& p2)
{
  return not(p1 == p2);
}

template<typename T1, typename T2>
inline bool operator<(const ptr_owned_by_unique<T1>& p1, const ptr_owned_by_unique<T2>& p2)
{
  return p1.compare_ptr(p2) < 0;
}

template<typename T1, typename T2>
inline bool operator<=(const ptr_owned_by_unique<T1>& p1, const ptr_owned_by_unique<T2>& p2)
{
  return p1.compare_ptr(p2) <= 0;
}

template<typename T1, typename T2>
inline bool operator>(const ptr_owned_by_unique<T1>& p1, const ptr_owned_by_unique<T2>& p2)
{
  return p1.compare_ptr(p2) > 0;
}

template<typename T1, typename T2>
inline bool operator>=(const ptr_owned_by_unique<T1>& p1, const ptr_owned_by_unique<T2>& p2)
{
  return p1.compare_ptr(p2) >= 0;
}

} //namespace pobu
