// file      : xsd/cxx/auto-array.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_AUTO_ARRAY_HXX
#define XSD_CXX_AUTO_ARRAY_HXX

#include <cstddef> // std::size_t

namespace xsd
{
  namespace cxx
  {
    // Simple automatic array.
    //
    template <typename X>
    struct auto_array
    {
      auto_array (X a[])
          : a_ (a)
      {
      }

      ~auto_array ()
      {
        delete[] a_;
      }

      X&
      operator[] (std::size_t index) const
      {
        return a_[index];
      }

      X*
      get () const
      {
        return a_;
      }

      X*
      release ()
      {
        X* tmp (a_);
        a_ = 0;
        return tmp;
      }

      void
      reset (X a[] = 0)
      {
        if (a_ != a)
        {
          delete[] a_;
          a_ = a;
        }
      }

      typedef void (auto_array::*bool_convertible)();

      operator bool_convertible () const
      {
        return a_ ? &auto_array<X>::true_ : 0;
      }

    private:
      void
      true_ ();

    private:
      X* a_;
    };

    template <typename X>
    void auto_array<X>::
    true_ ()
    {
    }
  }
}

#endif  // XSD_CXX_AUTO_ARRAY_HXX
