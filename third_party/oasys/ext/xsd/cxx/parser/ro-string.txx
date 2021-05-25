// file      : xsd/cxx/parser/ro-string.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      template <typename C>
      typename ro_string<C>::size_type ro_string<C>::
      find (C c, size_type pos) const
      {
        size_type r (npos);

        if (pos < size_)
	{
          if (const C* p = traits_type::find(data_ + pos, size_ - pos, c))
	    r = p - data_;
	}

        return r;
      }

      template<typename C>
      void
      trim (ro_string<C>& s)
      {
        typename ro_string<C>::size_type size (s.size ());

        if (size != 0)
        {
          const C* f (s.data ());
          const C* l (f + size);

          while (f < l &&
                 (*f == C (0x20) ||
                  *f == C (0x0D) ||
                  *f == C (0x09) ||
                  *f == C (0x0A)))
            ++f;

          --l;

          while (l > f &&
                 (*l == C (0x20) ||
                  *l == C (0x0D) ||
                  *l == C (0x09) ||
                  *l == C (0x0A)))
            --l;

          s.assign ((f <= l ? f : 0), (f <= l ? l - f + 1 : 0));
        }
      }
    }
  }
}
