#ifndef __STATEPTR_H__
#define __STATEPTR_H__

#include "../debug/DebugUtils.h"

/*!
 * This template is used in cases where it is useful to express the
 * transition of an object from one to another. For example, consider
 * a class Foo which exists in two states (1) and (2). Functions which
 * operate on Foo in state (2) are mutually exclusive from (1) and
 * illegal (but this invariant is easy to screw up because references
 * to Foo are all over the codebase).
 *
 * Now ideally we would express the two states as two different
 * classes, but in practice this may be a pain because of the need to
 * duplicate class hierarchies and other complexities.
 *
 * Instead of we do the following:
 *
 * 1) Make StatePtr a member of Foo. 
 * 2) When Foo changes state, call set_state(this).
 * 2) Functions in state (1) have type signature Foo* (or Foo& what have you)
 * 3) Functions in state (2) take StatePtr<Foo>, which is a) a
 * reminder of what state the object is in and b) does a debug sanity
 * check for statehood.
 *
 * Alternatives: 
 *
 * 1) Add an assertion in every usage case that Foo is in state
 * (2). This is annoying and tedious. Also, it's hard to remember to
 * do.
 * 
 * Note: This adds a level of indirection and pointer storage so
 * shouldn't be put into constrained code, but developer time is not
 * on Moore's Law.
 */
template<typename _Class>
class StatePtr {
public:
    StatePtr() : self_(0) {}

    /*!
     * Set the object to be in this state.
     */
    inline void set_state(_Class* self) { 
        ASSERT(self_ == 0);
        self_ = self; 
        ASSERT(self_ != 0);
    }

    //! @{ Standard pointer class overrides
    inline _Class& operator*() const { 
        ASSERTF(ptr_ != 0, "Invalid use of pointer -- state variable not set!");
        return *self_; 
    }

    inline _Class* operator->() const { 
        ASSERTF(ptr_ != 0, "Invalid use of pointer -- state variable not set!");
        return self_; 
    }
    //! @}
    
    inline _Class* ptr() const {
        return self_;
    }

private:
    _Class* self_;
};

#endif /* __STATEPTR_H__ */
