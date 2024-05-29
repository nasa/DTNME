// file      : xsd/cxx/tree/std-ostream-operators.ixx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#if defined(XSD_CXX_TREE_USE_CHAR) || !defined(XSD_CXX_TREE_USE_WCHAR)

#ifndef XSD_CXX_TREE_STD_OSTREAM_OPERATORS_IXX_CHAR
#define XSD_CXX_TREE_STD_OSTREAM_OPERATORS_IXX_CHAR

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      // optional
      //

      template <typename X, bool fund>
      std::basic_ostream<char>&
      operator<< (std::basic_ostream<char>& os, const optional<X, fund>& x)
      {
        if (x)
          os << *x;
        else
          os << "<not present>";

        return os;
      }
    }
  }
}

#endif // XSD_CXX_TREE_STD_OSTREAM_OPERATORS_IXX_CHAR
#endif // XSD_CXX_TREE_USE_CHAR


#if defined(XSD_CXX_TREE_USE_WCHAR) || !defined(XSD_CXX_TREE_USE_CHAR)

#ifndef XSD_CXX_TREE_STD_OSTREAM_OPERATORS_IXX_WCHAR
#define XSD_CXX_TREE_STD_OSTREAM_OPERATORS_IXX_WCHAR

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      // optional
      //

      template <typename X, bool fund>
      std::basic_ostream<wchar_t>&
      operator<< (std::basic_ostream<wchar_t>& os, const optional<X, fund>& x)
      {
        if (x)
          os << *x;
        else
          os << L"<not present>";

        return os;
      }
    }
  }
}

#endif // XSD_CXX_TREE_STD_OSTREAM_OPERATORS_IXX_WCHAR
#endif // XSD_CXX_TREE_USE_WCHAR
