// file      : xsd/cxx/tree/containers.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_TREE_CONTAINERS_HXX
#define XSD_CXX_TREE_CONTAINERS_HXX


#include <cstddef>   // std::ptrdiff_t
#include <string>
#include <vector>
#include <memory>    // std::auto_ptr
#include <iterator>  // std::iterator_traits
#include <algorithm> // std::equal, std::lexicographical_compare

#include <xsd/cxx/tree/istream-fwd.hxx> // tree::istream
#include <xsd/cxx/tree/elements.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      // Test whether X is a fundamental C++ type.
      //

      template <typename X>
      struct fundamental_p
      {
        static const bool r = false;
      };

      // byte
      //
      template <>
      struct fundamental_p<signed char>
      {
        static const bool r = true;
      };

      template <>
      struct fundamental_p<unsigned char>
      {
        static const bool r = true;
      };

      // short
      //
      template <>
      struct fundamental_p<short>
      {
        static const bool r = true;
      };

      template <>
      struct fundamental_p<unsigned short>
      {
        static const bool r = true;
      };

      // int
      //
      template <>
      struct fundamental_p<int>
      {
        static const bool r = true;
      };

      template <>
      struct fundamental_p<unsigned int>
      {
        static const bool r = true;
      };

      // long
      //
      template <>
      struct fundamental_p<long>
      {
        static const bool r = true;
      };

      template <>
      struct fundamental_p<unsigned long>
      {
        static const bool r = true;
      };

      template <>
      struct fundamental_p<long long>
      {
        static const bool r = true;
      };

      template <>
      struct fundamental_p<unsigned long long>
      {
        static const bool r = true;
      };

      // bool
      //
      template <>
      struct fundamental_p<bool>
      {
        static const bool r = true;
      };

      // float
      //
      template <>
      struct fundamental_p<float>
      {
        static const bool r = true;
      };

      template <>
      struct fundamental_p<double>
      {
        static const bool r = true;
      };

      template <>
      struct fundamental_p<long double>
      {
        static const bool r = true;
      };


      // Note that I cannot get rid of fund because of HP aCC3.
      //
      template <typename X, bool fund = fundamental_p<X>::r>
      class optional;


      //
      //
      template <typename X>
      class optional<X, false>
      {
      public:
        ~optional ();

        explicit
        optional (flags = 0, type* container = 0);

        explicit
        optional (const X&, flags = 0, type* container = 0);

        optional (const optional&, flags = 0, type* container = 0);

        template <typename S>
        optional (istream<S>&, flags = 0, type* container = 0);

        optional&
        operator= (const X&);

        optional&
        operator= (const optional&);

        // Pointer-like interface.
        //
      public:
        const X*
        operator-> () const
        {
          return x_;
        }

        X*
        operator-> ()
        {
          return x_;
        }

        const X&
        operator* () const
        {
          return *x_;
        }

        X&
        operator* ()
        {
          return *x_;
        }

        typedef optional self_; // Simplifier for Sun C++ 5.7.
        typedef void (self_::*bool_convertible) ();

        operator bool_convertible () const
        {
          return x_ != 0 ? &self_::true_ : 0;
        }

        // Get/set interface.
        //
      public:
        bool
        present () const
        {
          return x_ != 0;
        }

        const X&
        get () const
        {
          return *x_;
        }

        X&
        get ()
        {
          return *x_;
        }

        void
        set (const X&);

        void
        set (std::auto_ptr<X>);

        void
        reset ();

      private:
        void
        true_ ();

      private:
        X* x_;
        flags flags_;
        type* container_;
      };


      //
      //
      template <typename X>
      class optional<X, true>
      {
      public:
        explicit
        optional (flags  = 0, type* = 0)
            : present_ (false)
        {
        }

        explicit
        optional (const X&, flags = 0, type* = 0);

        optional (const optional&, flags = 0, type* = 0);

        template <typename S>
        optional (istream<S>&, flags = 0, type* container = 0);

        optional&
        operator= (const X&);

        optional&
        operator= (const optional&);

        // Pointer-like interface.
        //
      public:
        const X*
        operator-> () const
        {
          return &x_;
        }

        X*
        operator-> ()
        {
          return &x_;
        }

        const X&
        operator* () const
        {
          return get ();
        }

        X&
        operator* ()
        {
          return get ();
        }

        typedef optional self_; // Simplifier for Sun C++ 5.7.
        typedef void (self_::*bool_convertible) ();

        operator bool_convertible () const
        {
          return present () ? &self_::true_ : 0;
        }

        // Get/set interface.
        //
      public:
        bool
        present () const
        {
          return present_;
        }

        const X&
        get () const
        {
          return x_;
        }

        X&
        get ()
        {
          return x_;
        }

        void
        set (const X& y)
        {
          x_ = y;
          present_ = true;
        }

        void
        reset ()
        {
          present_ = false;
        }

      private:
        void
        true_ ();

      private:
        bool present_;
        X x_;
      };

      // Comparison operators.
      //

      template <typename X, bool fund>
      inline bool
      operator== (const optional<X, fund>& a, const optional<X, fund>& b)
      {
        return !a || !b ? a == b : *a == *b;
      }

      template <typename X, bool fund>
      inline bool
      operator!= (const optional<X, fund>& a, const optional<X, fund>& b)
      {
        return !(a == b);
      }

      template <typename X, bool fund>
      inline bool
      operator< (const optional<X, fund>& a, const optional<X, fund>& b)
      {
        return a && (!b || *a < *b);
      }

      template <typename X, bool fund>
      inline bool
      operator> (const optional<X, fund>& a, const optional<X, fund>& b)
      {
        return b < a;
      }

      template <typename X, bool fund>
      inline bool
      operator<= (const optional<X, fund>& a, const optional<X, fund>& b)
      {
        return !(a > b);
      }

      template <typename X, bool fund>
      inline bool
      operator>= (const optional<X, fund>& a, const optional<X, fund>& b)
      {
        return !(a < b);
      }


      // Sequence.
      //

      // Note that I cannot get rid of 'fund' because HP aCC3 likes it
      // this way.
      //
      template <typename X, bool fund = fundamental_p<X>::r>
      class sequence;


      // Sun CC's <iterator> does not have iterator_traits. To overcome
      // this, we will wrap std::iterator_traits into our own and also
      // specialize it for pointer types. Since Sun CC uses pointer
      // for vector::iterator, it will use the specialization and won't
      // notice the std::iterator_traits.
      //
      template <typename I>
      struct iterator_traits
      {
        typedef
        typename std::iterator_traits<I>::iterator_category
        iterator_category;

        typedef
        typename std::iterator_traits<I>::value_type
        value_type;

        typedef
        typename std::iterator_traits<I>::difference_type
        difference_type;
      };

      template <typename X>
      struct iterator_traits<X*>
      {
        typedef std::random_access_iterator_tag iterator_category;
        typedef X value_type;
        typedef std::ptrdiff_t difference_type;
      };


      // This is a dangerously destructive automatic pointer. We are going
      // to use it in a controlled environment to save us a lot of coding.
      //
      struct ptr
      {
        ~ptr ()
        {
          reset ();
        }

        explicit
        ptr (type* x = 0)
            : x_ (x)
        {
        }

        ptr (const ptr& y)
            : x_ (y.x_)
        {
          // Yes, hostile takeover.
          //
          y.x_ = 0;
        }

        ptr&
        operator= (const ptr& y)
        {
          if (this != &y)
          {
            // Yes, hostile takeover.
            //
            reset (y.x_);
            y.x_ = 0;
          }

          return *this;
        }

      public:
        type&
        operator* () const
        {
          return *get ();
        }

        type*
        get () const
        {
          return x_;
        }

      public:
        void
        reset (type* x = 0)
        {
	  if (x_ != x)
	  {
            delete x_;
            x_ = x;
	  }
        }

      private:
        mutable type* x_;
      };


      // Specialization for complex types.
      //

      template <typename I, typename X>
      struct iterator_adapter
      {
        typedef X value_type;
        typedef value_type& reference;
        typedef value_type* pointer;

        typedef
        typename iterator_traits<I>::iterator_category
        iterator_category;

        typedef
        typename iterator_traits<I>::difference_type
        difference_type;


      public:
        iterator_adapter ()
            : i_ () // i_ can be of a pointer type.
        {
        }

        // Allow iterator to const_iterator conversion.
        //
        template <typename J, typename Y>
        iterator_adapter (const iterator_adapter<J, Y>& j)
            : i_ (j.base ())
        {
        }

        explicit
        iterator_adapter (const I& i)
            : i_ (i)
        {
        }

      public:
        // Forward iterator requirements.
        //
        reference
        operator* () const
        {
          return static_cast<reference> (**i_);
        }

        pointer
        operator-> () const
        {
          return static_cast<pointer> (i_->get ());
        }

        iterator_adapter&
        operator++ ()
        {
          ++i_;
          return *this;
        }

        iterator_adapter
        operator++ (int)
        {
          iterator_adapter r (*this);
          ++i_;
          return r;
        }

        // Bidirectional iterator requirements.
        //
        iterator_adapter&
        operator-- ()
        {
          --i_;
          return *this;
        }

        iterator_adapter
        operator-- (int)
        {
          iterator_adapter r (*this);
          --i_;
          return r;
        }

        // Random access iterator requirements.
        //
        reference
        operator[] (difference_type n) const
        {
          return static_cast<reference> (i_[n]);
        }

        iterator_adapter&
        operator+= (difference_type n)
        {
          i_ += n;
          return *this;
        }

        iterator_adapter
        operator+ (difference_type n) const
        {
          return iterator_adapter (i_ + n);
        }

        iterator_adapter&
        operator-= (difference_type n)
        {
          i_ -= n;
          return *this;
        }

        iterator_adapter
        operator- (difference_type n) const
        {
          return iterator_adapter (i_ - n);
        }

      public:
        const I&
        base () const
        {
          return i_;
        }

      private:
        I i_;
      };

      // Note: We use different types for left- and right-hand-side
      // arguments to allow comparison between iterator and const_iterator.
      //

      // Forward iterator requirements.
      //
      template <typename I, typename J, typename X, typename Y>
      inline bool
      operator== (const iterator_adapter<I, X>& i,
                  const iterator_adapter<J, Y>& j)
      {
        return i.base () == j.base ();
      }

      template <typename I, typename J, typename X, typename Y>
      inline bool
      operator!= (const iterator_adapter<I, X>& i,
                  const iterator_adapter<J, Y>& j)
      {
        return i.base () != j.base ();
      }

      // Random access iterator requirements
      //
      template <typename I, typename J, typename X, typename Y>
      inline bool
      operator< (const iterator_adapter<I, X>& i,
                 const iterator_adapter<J, Y>& j)
      {
        return i.base() < j.base();
      }

      template <typename I, typename J, typename X, typename Y>
      inline bool
      operator> (const iterator_adapter<I, X>& i,
                 const iterator_adapter<J, Y>& j)
      {
        return i.base() > j.base();
      }

      template <typename I, typename J, typename X, typename Y>
      inline bool
      operator<= (const iterator_adapter<I, X>& i,
                  const iterator_adapter<J, Y>& j)
      {
        return i.base() <= j.base();
      }

      template <typename I, typename J, typename X, typename Y>
      inline bool
      operator>= (const iterator_adapter<I, X>& i,
                  const iterator_adapter<J, Y>& j)
      {
        return i.base() >= j.base();
      }

      template <typename I, typename J, typename X, typename Y>
      inline typename iterator_adapter<I, X>::difference_type
      operator- (const iterator_adapter<I, X>& i,
                 const iterator_adapter<J, Y>& j)
      {
        return i.base () - j.base ();
      }

      template <typename I, typename X>
      inline iterator_adapter<I, X>
      operator+ (typename iterator_adapter<I, X>::difference_type n,
                 const iterator_adapter<I, X>& i)
      {
        return iterator_adapter<I, X> (i.base() + n);
      }

      //
      //
      class sequence_common
      {
      protected:
        typedef std::vector<ptr> base_sequence;
        typedef base_sequence::iterator base_iterator;
        typedef base_sequence::const_iterator base_const_iterator;

        typedef base_sequence::size_type       size_type;
        typedef base_sequence::difference_type difference_type;
        typedef base_sequence::allocator_type  allocator_type;

      protected:
        sequence_common (flags f, type* c)
            : flags_ (f | flags::not_root), container_ (c)
        {
        }

        sequence_common (size_type n, const type& x)
            : flags_ (0), container_ (0)
        {
          assign (n, x);
        }

        template<typename I>
        sequence_common (const I& b, const I& e)
            : flags_ (0), container_ (0)
        {
          assign (b, e);
        }

        sequence_common (const sequence_common& v, flags f, type* c)
            : flags_ (f | flags::not_root), container_ (c), v_ (v.size ())
        {
          base_iterator di (v_.begin ()), de (v_.end ());
          base_const_iterator si (v.v_.begin ()), se (v.v_.end ());

          for (; si != se && di != de; ++si, ++di)
          {
            // We have no ptr_ref.
            //
            ptr p ((**si)._clone (flags_, container_));
            *di = p;
          }
        }

      public:
        sequence_common&
        operator= (const sequence_common& v)
        {
          if (this == &v)
            return *this;

          v_.assign (v.v_.size (), ptr ());

          base_iterator di (v_.begin ()), de (v_.end ());
          base_const_iterator si (v.v_.begin ()), se (v.v_.end ());

          for (; si != se && di != de; ++si, ++di)
          {
            // We have no ptr_ref.
            //
            ptr p ((**si)._clone (flags_, container_));
            *di = p;
          }

          return *this;
        }

      public:
        size_type
        size () const
        {
          return v_.size ();
        }

        size_type
        max_size () const
        {
          return v_.max_size ();
        }

        size_type
        capacity () const
        {
          return v_.capacity ();
        }

        bool
        empty () const
        {
          return v_.empty ();
        }

        void
        reserve (size_type n)
        {
          v_.reserve (n);
        }

        void
        clear ()
        {
          v_.clear ();
        }

      protected:
        void
        assign (size_type n, const type& x)
        {
          v_.assign (n, ptr ());

          for (base_iterator i (v_.begin ()), e (v_.end ()); i != e; ++i)
          {
            ptr p (x._clone (flags_, container_));
            *i = p;
          }
        }

        template<typename I>
        void
        assign (const I& b, const I& e)
        {
          // This is not the fastest way to do it. Also I's type may not
          // have _clone.
          //

          v_.clear ();

          for (I i (b); i != e; ++i)
          {
            ptr p (i->_clone (flags_, container_));
            v_.push_back (p);
          }
        }

        void
        resize (size_type n, const type& x)
        {
          size_type old (v_.size ());
          v_.resize (n, ptr ());

          if (old < n)
            for (base_iterator i (v_.begin () + old), e (v_.end ());
                 i != e; ++i)
            {
              ptr p (x._clone (flags_, container_));
              *i = p;
            }
        }

        void
        insert (base_iterator p, size_type n, const type& x)
        {
          difference_type d (v_.end () - p);
          v_.insert (p, n, ptr ());

          for (base_iterator i (v_.end () - d); n != 0; --n)
          {
            ptr r (x._clone (flags_, container_));
            *(--i) = r;
          }
        }

        template<typename I>
        void
        insert (base_iterator p, const I& b, const I& e)
        {
          // This is not the fastest way to do it. Also I's type may not
          // have _clone.
          //
          if (b != e)
          {
            for (I i (e);;)
            {
              --i;
              ptr r (i->_clone (flags_, container_));
              p = v_.insert (p, r);

              if (i == b)
                break;
            }
          }
        }

      protected:
        flags flags_;
        type* container_;
        base_sequence v_;
      };

      //
      //
      template <typename X>
      class sequence<X, false>: public sequence_common
      {
      public:
        typedef X        value_type;
        typedef X*       pointer;
        typedef const X* const_pointer;
        typedef X&       reference;
        typedef const X& const_reference;

        typedef
        iterator_adapter<base_sequence::iterator, X>
        iterator;

        typedef
        iterator_adapter<base_sequence::const_iterator, const X>
        const_iterator;

        typedef
        iterator_adapter<base_sequence::reverse_iterator, X>
        reverse_iterator;

        typedef
        iterator_adapter<base_sequence::const_reverse_iterator, const X>
        const_reverse_iterator;

        typedef sequence_common::size_type       size_type;
        typedef sequence_common::difference_type difference_type;
        typedef sequence_common::allocator_type  allocator_type;

      public:
        explicit
        sequence (flags f = 0, type* container = 0)
            : sequence_common (f, container)
        {
        }

        explicit
        sequence (size_type n)
            : sequence_common (n, X ())
        {
        }

        sequence (size_type n, const X& x)
            : sequence_common (n, x)
        {
        }

        template<typename I>
        sequence (const I& b, const I& e)
            : sequence_common (b, e)
        {
        }

        template <typename S>
        sequence (istream<S>&, flags = 0, type* container = 0);

        sequence (const sequence<X, false>& v,
                  flags f = 0,
                  type* container = 0)
            : sequence_common (v, f, container)
        {
        }

      public:
        void
        assign (size_type n, const X& x)
        {
          sequence_common::assign (n, x);
        }

        template<typename I>
        void
        assign (const I& b, const I& e)
        {
          sequence_common::assign (b, e);
        }

      public:
        void
        resize (size_type n)
        {
          sequence_common::resize (n, X ());
        }

        void
        resize (size_type n, const X& x)
        {
          sequence_common::resize (n, x);
        }

      public:
        const_iterator
        begin () const
        {
          return const_iterator (v_.begin ());
        }

        const_iterator
        end () const
        {
          return const_iterator (v_.end ());
        }

        iterator
        begin ()
        {
          return iterator (v_.begin ());
        }

        iterator
        end ()
        {
          return iterator (v_.end ());
        }

        // reverse
        //

        const_reverse_iterator
        rbegin () const
        {
          return const_reverse_iterator (v_.rbegin ());
        }

        const_reverse_iterator
        rend () const
        {
          return const_reverse_iterator (v_.rend ());
        }

        reverse_iterator
        rbegin ()
        {
          return reverse_iterator (v_.rbegin ());
        }

        reverse_iterator
        rend ()
        {
          return reverse_iterator (v_.rend ());
        }

      public:
        X&
        operator[] (size_type n)
        {
          return static_cast<X&> (*(v_[n]));
        }

        const X&
        operator[] (size_type n) const
        {
          return static_cast<const X&> (*(v_[n]));
        }

        X&
        at (size_type n)
        {
          return static_cast<X&> (*(v_.at (n)));
        }

        const X&
        at (size_type n) const
        {
          return static_cast<const X&> (*(v_.at (n)));
        }

        X&
        front ()
        {
          return static_cast<X&> (*(v_.front ()));
        }

        const X&
        front () const
        {
          return static_cast<const X&> (*(v_.front ()));
        }

        X&
        back ()
        {
          return static_cast<X&> (*(v_.back ()));
        }

        const X&
        back () const
        {
          return static_cast<const X&> (*(v_.back ()));
        }

      public:
        void
        push_back (const X& x)
        {
          v_.push_back (ptr (x._clone (flags_, container_)));
        }

        void
        push_back (std::auto_ptr<X> x)
        {
          if (x->_container () == container_)
            v_.push_back (ptr (x.release ()));
          else
            push_back (*x);
        }


        void
        pop_back ()
        {
          v_.pop_back ();
        }

        iterator
        insert (iterator position, const X& x)
        {
          return iterator (
            v_.insert (
              position.base (), ptr (x._clone (flags_, container_))));
        }

        void
        insert (iterator position, size_type n, const X& x)
        {
          sequence_common::insert (position.base (), n, x);
        }

        template<typename I>
        void
        insert (iterator position, const I& b, const I& e)
        {
          sequence_common::insert (position.base (), b, e);
        }

        iterator
        erase (iterator position)
        {
          return iterator (v_.erase (position.base ()));
        }

        iterator
        erase (iterator b, iterator e)
        {
          return iterator (v_.erase (b.base (), e.base ()));
        }

        void
        swap (sequence<X, false>& x)
        {
          v_.swap (x.v_);
        }
      };


      // Specialization for fundamental types.
      //
      template <typename X>
      class sequence<X, true>: public std::vector<X>
      {
        typedef std::vector<X> base_sequence;

      public:
        explicit
        sequence (flags = 0, type* = 0)
        {
        }

        explicit
        sequence (typename base_sequence::size_type n, const X& x = X ())
            : base_sequence (n, x)
        {
        }

        template<typename I>
        sequence (const I& b, const I& e)
            : base_sequence (b, e)
        {
        }

        template <typename S>
        sequence (istream<S>&, flags = 0, type* container = 0);

        sequence (const sequence<X, true>& s, flags = 0, type* = 0)
            : base_sequence (s)
        {
        }
      };


      // Comparison operators.
      //

      template <typename X, bool fund>
      inline bool
      operator== (const sequence<X, fund>& a, const sequence<X, fund>& b)
      {
        return (a.size () == b.size ()
                && std::equal (a.begin (), a.end (), b.begin ()));
      }

      template <typename X, bool fund>
      inline bool
      operator!= (const sequence<X, fund>& a, const sequence<X, fund>& b)
      {
        return !(a == b);
      }

      template <typename X, bool fund>
      inline bool
      operator< (const sequence<X, fund>& a, const sequence<X, fund>& b)
      {
        return std::lexicographical_compare (a.begin (), a.end (),
                                             b.begin (), b.end ());
      }

      template <typename X, bool fund>
      inline bool
      operator> (const sequence<X, fund>& a, const sequence<X, fund>& b)
      {
        return b < a;
      }

      template <typename X, bool fund>
      inline bool
      operator<= (const sequence<X, fund>& a, const sequence<X, fund>& b)
      {
        return !(a > b);
      }

      template <typename X, bool fund>
      inline bool
      operator>= (const sequence<X, fund>& a, const sequence<X, fund>& b)
      {
        return !(a < b);
      }

      template <typename X, bool fund>
      inline void
      swap (sequence<X, fund>& x, sequence<X, fund>& y)
      {
        x.swap (y);
      }

      // one (for internal use only)
      //

      template <typename X, bool fund = fundamental_p<X>::r>
      class one;

      template<typename X>
      class one<X, false>
      {
      public:
        ~one ();

        one (flags, type* container);

        one (const X&, flags, type* container);

        one (std::auto_ptr<X>, flags, type* container);

        template <typename S>
        one (istream<S>&, flags, type* container);

        one (const one&, flags, type* container);

        one&
        operator= (const one&);

      public:
        const X&
        get () const
        {
          return *x_;
        }

        X&
        get ()
        {
          return *x_;
        }

        void
        set (const X&);

        void
        set (std::auto_ptr<X>);

        bool
        present () const
        {
          return x_ != 0;
        }

      protected:
        X* x_;
        flags flags_;
        type* container_;
      };


      template<typename X>
      class one<X, true>
      {
      public:
        one (flags, type*)
            : present_ (false)
        {
        }

        one (const X& x, flags, type*)
            : x_ (x), present_ (true)
        {
        }

        template <typename S>
        one (istream<S>&, flags, type* container);

        one (const one& x, flags, type*)
            : x_ (x.x_), present_ (x.present_)
        {
        }

        one&
        operator= (const one& x)
        {
          if (this == &x)
            return *this;

          x_ = x.x_;
          present_ = x.present_;

          return *this;
        }

      public:
        const X&
        get () const
        {
          return x_;
        }

        X&
        get ()
        {
          return x_;
        }

        void
        set (const X& x)
        {
          x_ = x;
          present_ = true;
        }

        bool
        present () const
        {
          return present_;
        }

      protected:
        X x_;
        bool present_;
      };
    }
  }
}

#include <xsd/cxx/tree/containers.txx>

#endif  // XSD_CXX_TREE_CONTAINERS_HXX

