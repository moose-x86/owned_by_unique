#pragma once

#include <memory>
#include <exception>
#include <type_traits>

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
    virtual ~dtor_notify_enabled() { *isDestroyed = true; }

    const std::shared_ptr<bool> isDestroyed{std::make_shared<bool>(false)};
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

struct unique_ptr_already_acquired : public std::runtime_error
{
    unique_ptr_already_acquired()
        : std::runtime_error("owned_by_unique: This pointer is already acquired by unique_ptr")
    {
    }
};

struct ptr_is_already_deleted : public std::runtime_error
{
    ptr_is_already_deleted() : std::runtime_error("owned_by_unique: This pointer is already deleted")
    {
    }
};

} // namespace detail

template<typename _Tp1> class owned_by_unique : private std::shared_ptr<_Tp1>
{
    using shared_ptr = std::shared_ptr<_Tp1>;
    template <typename> friend class owned_by_unique;

    constexpr static const bool isAcquired = true;

public:
    using element_type = _Tp1;
    using UniquePtr = std::unique_ptr<element_type>;
    using shared_ptr::operator bool;
    using shared_ptr::use_count;

    owned_by_unique() : owned_by_unique(nullptr, not isAcquired)
    {}

    owned_by_unique(std::nullptr_t) : owned_by_unique()
    {}

    template<typename _Tp2>
    owned_by_unique(detail::unique_ptr_link<_Tp2>&& pointee)
    {
        *this = owned_by_unique<_Tp2>(pointee._ptr, isAcquired);
    }

    template<typename _Tp2>
    owned_by_unique(std::unique_ptr<_Tp2>&& pointee)
    {
        *this = owned_by_unique<_Tp2>(pointee.release(), not isAcquired);
    }

    template<typename _Tp2>
    owned_by_unique(const owned_by_unique<_Tp2> &pointee)
    {
        *this = pointee;
    }

    template<typename _Tp2>
    owned_by_unique& operator=(const owned_by_unique<_Tp2> &pointee)
    {
        static_assert(std::is_same   <element_type, _Tp2>::value or
                      std::is_base_of<element_type, _Tp2>::value,
                      "Assigning pointer of different or non-derived type");

        acquireSharedResource(pointee);
        isDestroyed = pointee.isDestroyed;
        acquiredByUniquePtr = pointee.acquiredByUniquePtr;
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

    UniquePtr unique_ptr() const
    {
        if(shared_ptr::get())
        {
            if(not is_acquired())
            {
                *acquiredByUniquePtr = true;
                return UniquePtr(get());
            }
            throw detail::unique_ptr_already_acquired();
        }
        return nullptr;
    }

    explicit operator UniquePtr() const
    {
        return unique_ptr();
    }

    bool is_acquired() const
    {
        return *acquiredByUniquePtr;
    }

private:
    std::shared_ptr<const bool> isDestroyed;
    std::shared_ptr<bool> acquiredByUniquePtr;

    struct Deleter
    {
        Deleter(std::shared_ptr<bool> a) : acquired(a){}
        void operator()(element_type *const pointee)
        {
            if (not *acquired) deletePointee(pointee);
        }

    private:
        std::shared_ptr<const bool> acquired;

        void deletePointee(element_type *const pointee)
        {
            if (std::is_array<element_type>::value)
                delete[] pointee;
            else
                delete pointee;
        }
    };

    void throw_if_is_destroyed_and_has_virtual_dtor() const
    {
        if(isDestroyed and *isDestroyed)
            throw detail::ptr_is_already_deleted();
    }

    template<typename _Tp2>
    void acquireSharedResource(const owned_by_unique<_Tp2> &pointee)
    {
        shared_ptr::operator=(pointee);
    }

    template<typename _Tp2> typename std::enable_if
    <
        std::has_virtual_destructor<_Tp2>::value, void
    >::type acquireIsDestroyedFlagIfPossible(_Tp2 *const p)
    {
        const auto dtor_enabled = dynamic_cast<detail::dtor_notify_enabled<_Tp2>*>(p);

        if(dtor_enabled)
            isDestroyed = dtor_enabled->isDestroyed;
    }

    template<typename _Tp2>
    void acquireIsDestroyedFlagIfPossible(detail::dtor_notify_enabled<_Tp2>* const pointee)
    {
        if(pointee)
            isDestroyed = pointee->isDestroyed;
    }

    template<typename _Tp2> typename std::enable_if
    <
        not std::has_virtual_destructor<_Tp2>::value, void
    >::type acquireIsDestroyedFlagIfPossible(_Tp2 *const p)
    {}

    owned_by_unique(element_type *const pointee, const bool acquired)
    {
        acquiredByUniquePtr = std::make_shared<bool>(acquired);
        shared_ptr::operator=(shared_ptr(pointee, Deleter(acquiredByUniquePtr)));

        acquireIsDestroyedFlagIfPossible(pointee);
    }
};

template<> struct owned_by_unique<void> {};

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

template<typename T1, typename T2>
inline bool operator==(const owned_by_unique<T1> &p1, const owned_by_unique<T2> &p2)
{
    return (p1.get() == p2.get());
}

template<typename T1, typename T2>
inline bool operator!=(const owned_by_unique<T1> &p1, const owned_by_unique<T2> &p2)
{
    return not(p1 == p2);
}

template<typename T1, typename T2>
inline bool operator<(const owned_by_unique<T1> &p1, const owned_by_unique<T2> &p2)
{
    return p1.get() < p2.get();
}

template< typename _PT, typename... Args >
inline typename std::enable_if
<
    not std::is_array<_PT>::value, owned_by_unique<_PT>
>::type make_owned_by_unique(Args&&... args)
{
    using pointee_t =
       typename std::conditional
       <
           std::has_virtual_destructor<_PT>::value,
           detail::dtor_notify_enabled<_PT>,
           _PT
       >::type;

    std::unique_ptr<pointee_t> tmp(new pointee_t(std::forward<Args>(args)...));
    return owned_by_unique<_PT>(std::move(tmp));
}

template< typename _PT >
inline typename std::enable_if
<
    std::is_array<_PT>::value and std::extent<_PT>::value, owned_by_unique<_PT>
>::type make_owned_by_unique()
{
    std::unique_ptr<_PT> tmp(new _PT[1]);
    return owned_by_unique<_PT>(std::move(tmp));
}

}
