#ifndef __FUNCTORS_H__
#define __FUNCTORS_H__

#include <functional>
#include <algorithm>

namespace oasys {

/*! \file These are various adaptors for performing common operations
 * with the STL algorithms (and their friends). It is possible to
 * construct these operations with the STL -- however, the syntax is
 * awkward and difficult to dicipher (can be said for all of the
 * internals of the STL).
 *
 * The lower case function should be used instead of the classes
 * directly (see below). 
 */

//! See below for the function you should use
template<typename _Value, 
         typename _Class, 
         typename _Comp,
         typename _Ret>
struct CompFunctor {
    CompFunctor(_Value value, 
                _Ret (_Class::*m_fcn_ptr)() const,
                const _Comp& comparator) 
        : value_(value), 
          m_fcn_ptr_(m_fcn_ptr),
          comparator_(comparator) {}
    
    bool operator()(const _Class& other) const {
        const _Value& other_val = (other.*m_fcn_ptr_)();
        return comparator_(value_, other_val);
    }
    
    _Value value_;
    _Ret (_Class::*m_fcn_ptr_)() const;
    const _Comp& comparator_;
};

/*!
 * This function takes an object t of type _T, a member accessor
 * function f and a comparison function and returns the functor:
 *
 *      bool F(const _T& other) { 
 *          return other.f() -OP- t.f(); 
 *      }
 */
template<typename _Value, typename _Class, typename _Comp, typename _Ret>
CompFunctor<_Value, _Class, _Comp, _Ret> 
comp_functor(const _Class& t, 
             _Ret (_Class::*m_fcn_ptr)() const,
             _Comp comparator)
{
    return CompFunctor<_Value, _Class, _Comp, _Ret>(t, m_fcn_ptr, comparator);
}

/*!
 * @{ This next set of functions implement the above for the standard
 * operator overloads. There need to be several different overloadings
 * because of issues with constness.
 */
#define MAKE_FUNCTOR(_name, _operator)                          \
                                                                \
template<typename _Value, typename _Class, typename _Ret>       \
inline CompFunctor<_Value, _Class, _operator<_Value>,  _Ret>    \
_name(_Value value, _Ret (_Class::*m_fcn_ptr)() const)          \
{                                                               \
    _operator<_Value> comp;                                     \
    return CompFunctor<_Value, _Class, _operator<_Value>, _Ret> \
        (value, m_fcn_ptr, comp);                               \
}                                                               \
                                                                \
template<typename _Value, typename _Class, typename _Ret>       \
inline CompFunctor<_Value, _Class, _operator<_Value>, _Ret>     \
_name(_Value value, _Ret (_Class::*m_fcn_ptr)())                \
{                                                               \
    _operator<_Value> comp;                                     \
    return CompFunctor<_Value, _Class, _operator<_Value>, _Ret> \
        (value, m_fcn_ptr, comp);                               \
}
//! @}

MAKE_FUNCTOR(eq_functor,  std::equal_to);
MAKE_FUNCTOR(neq_functor, std::not_equal_to);
MAKE_FUNCTOR(gt_functor,  std::greater);
MAKE_FUNCTOR(lt_functor,  std::less);
MAKE_FUNCTOR(gte_functor, std::greater_equal);
MAKE_FUNCTOR(lte_functor, std::less_equal);


#undef MAKE_FUNCTOR

//! @}

/*!
 * @return True if elt exists in the container cont.
 */
template<typename _ContType, typename _EltType>
bool
element_of(const _ContType& cont, const _EltType& elt)
{
    return std::find(cont.begin(), cont.end(), elt) != cont.end();
}

/*!
 * @return True if elt exists in the container cont->mem_fcn.
 */
template
<
    typename _ContType, 
    typename _EltType, 
    typename _EltType2, 
    typename _Ret
>
bool
element_of(const _ContType& cont, 
           const _EltType& value, 
           _Ret (_EltType2::*m_fcn_ptr)() const)
{
    typename _ContType::const_iterator begin = cont.begin();
    typename _ContType::const_iterator end   = cont.end();
    typename _ContType::const_iterator itr;

    itr = std::find_if(begin, end, eq_functor(value, m_fcn_ptr));

    return itr != end;
}

/*!
 * @return True if elt exists in the container cont->mem_fcn, for
 * non-const mem_fcn.
 */
template
<
    typename _ContType, 
    typename _EltType, 
    typename _EltType2, 
    typename _Ret
>
bool
element_of(const _ContType& cont, 
           const _EltType& value, 
           _Ret (_EltType2::*m_fcn_ptr)())
{
    typename _ContType::const_iterator begin = cont.begin();
    typename _ContType::const_iterator end   = cont.end();
    typename _ContType::const_iterator itr;

    itr = std::find_if(begin, end, eq_functor(value, m_fcn_ptr));

    return itr != end;
}

} // namespace oasys

#endif /* __FUNCTORS_H__ */
