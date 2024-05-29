// file      : xsd/cxx/tree/containers.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      // optional
      //
      template <typename X>
      optional<X, false>::
      ~optional ()
      {
        delete x_;
      }

      template <typename X>
      optional<X, false>::
      optional (flags f, type* container)
          : x_ (0), flags_ (f), container_ (container)
      {
      }

      template <typename X>
      optional<X, false>::
      optional (const X& x, flags f, type* container)
          : x_ (0), flags_ (f), container_ (container)
      {
        set (x);
      }

      template <typename X>
      optional<X, false>::
      optional (const optional<X, false>& x, flags f, type* container)
          : x_ (0), flags_ (f), container_ (container)
      {
        if (x)
          set (*x);
      }

      template <typename X>
      optional<X, false>& optional<X, false>::
      operator= (const X& x)
      {
        if (x_ == &x)
          return *this;

        set (x);

        return *this;
      }

      template <typename X>
      optional<X, false>& optional<X, false>::
      operator= (const optional<X, false>& x)
      {
        if (this == &x)
          return *this;

        if (x)
          set (*x);
        else
          reset ();

        return *this;
      }

      template <typename X>
      void optional<X, false>::
      set (const X& x)
      {
        // We always do a fresh copy because X may not be x's
        // dynamic type.
        //
        X* r (x._clone (flags_, container_));

        delete x_;
        x_ = r;
      }

      template <typename X>
      void optional<X, false>::
      set (std::auto_ptr<X> x)
      {
        X* r (0);

        if (x.get () != 0)
        {
          if (x->_container () == container_)
            r = x.release ();
          else
            r = x->_clone (flags_, container_);
        }

        delete x_;
        x_ = r;
      }

      template <typename X>
      void optional<X, false>::
      reset ()
      {
        delete x_;
        x_ = 0;
      }

      template <typename X>
      void optional<X, false>::
      true_ ()
      {
      }


      // optional
      //
      template <typename X>
      optional<X, true>::
      optional (const X& y, flags, type*)
          : present_ (false)
      {
        set (y);
      }

      template <typename X>
      optional<X, true>::
      optional (const optional<X, true>& y, flags, type*)
          : present_ (false)
      {
        if (y)
          set (*y);
      }

      template <typename X>
      optional<X, true>& optional<X, true>::
      operator= (const X& y)
      {
        if (&x_ == &y)
          return *this;

        set (y);

        return *this;
      }

      template <typename X>
      optional<X, true>& optional<X, true>::
      operator= (const optional<X, true>& y)
      {
        if (this == &y)
          return *this;

        if (y)
          set (*y);
        else
          reset ();

        return *this;
      }

      template <typename X>
      void optional<X, true>::
      true_ ()
      {
      }


      // one
      //
      template<typename X>
      one<X, false>::
      ~one ()
      {
        delete x_;
      }

      template<typename X>
      one<X, false>::
      one (flags f, type* container)
          : x_ (0), flags_ (f), container_ (container)
      {
      }

      template<typename X>
      one<X, false>::
      one (const X& x, flags f, type* container)
          : x_ (0), flags_ (f), container_ (container)
      {
        set (x);
      }

      template<typename X>
      one<X, false>::
      one (std::auto_ptr<X> x, flags f, type* container)
          : x_ (0), flags_ (f), container_ (container)
      {
        set (x);
      }

      template<typename X>
      one<X, false>::
      one (const one<X, false>& x, flags f, type* container)
          : x_ (0), flags_ (f), container_ (container)
      {
        if (x.present ())
          set (x.get ());
      }

      template<typename X>
      one<X, false>& one<X, false>::
      operator= (const one<X, false>& x)
      {
        if (this == &x)
          return *this;

        if (x.present ())
          set (x.get ());
        else
        {
          delete x_;
          x_ = 0;
        }

        return *this;
      }

      template<typename X>
      void one<X, false>::
      set (const X& x)
      {
        // We always do a fresh copy because X may not be x's
        // dynamic type.
        //
        X* r (x._clone (flags_, container_));

        delete x_;
        x_ = r;
      }

      template<typename X>
      void one<X, false>::
      set (std::auto_ptr<X> x)
      {
        X* r (0);

        if (x.get () != 0)
        {
          if (x->_container () == container_)
            r = x.release ();
          else
            r = x->_clone (flags_, container_);
        }

        delete x_;
        x_ = r;
      }
    }
  }
}


