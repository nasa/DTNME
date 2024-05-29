// file      : xsd/cxx/parser/exceptions.ixx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#if defined(XSD_CXX_PARSER_USE_CHAR) || !defined(XSD_CXX_PARSER_USE_WCHAR)

#ifndef XSD_CXX_PARSER_EXCEPTIONS_IXX_CHAR
#define XSD_CXX_PARSER_EXCEPTIONS_IXX_CHAR

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      // error
      //
      inline
      std::basic_ostream<char>&
      operator<< (std::basic_ostream<char>& os, const error<char>& e)
      {
        return os << e.id () << ':' << e.line () << ':' << e.column ()
                  << " error: " << e.message ();
      }

      // errors
      //
      inline
      std::basic_ostream<char>&
      operator<< (std::basic_ostream<char>& os, const errors<char>& e)
      {
        for (errors<char>::const_iterator b (e.begin ()), i (b);
             i != e.end ();
             ++i)
        {
          if (i != b)
            os << "\n";

          os << *i;
        }

        return os;
      }

      // parsing
      //
      template<>
      inline
      void parsing<char>::
      print (std::basic_ostream<char>& os) const
      {
        if (errors_.empty ())
          os << "instance document parsing failed";
        else
          os << errors_;
      }

      // expected_element
      //
      template<>
      inline
      void expected_element<char>::
      print (std::basic_ostream<char>& os) const
      {
        os << "expected element '"
           << expected_namespace ()
           << (expected_namespace ().empty () ? "" : "#")
           << expected_name () << "'";

        if (!encountered_name ().empty ())
          os << " instead of '"
             << encountered_namespace ()
             << (encountered_namespace ().empty () ? "" : "#")
             << encountered_name () << "'";
      }

      // unexpected_element
      //
      template<>
      inline
      void unexpected_element<char>::
      print (std::basic_ostream<char>& os) const
      {
        os << "unexpected element '"
           << encountered_namespace ()
           << (encountered_namespace ().empty () ? "" : "#")
           << encountered_name () << "'";
      }

      // expected_attribute
      //
      template<>
      inline
      void expected_attribute<char>::
      print (std::basic_ostream<char>& os) const
      {
        os << "expected attribute '"
           << expected_namespace ()
           << (expected_namespace ().empty () ? "" : "#")
           << expected_name () << "'";
      }

      // unexpected_attribute
      //
      template<>
      inline
      void unexpected_attribute<char>::
      print (std::basic_ostream<char>& os) const
      {
        os << "unexpected attribute '"
           << encountered_namespace ()
           << (encountered_namespace ().empty () ? "" : "#")
           << encountered_name () << "'";
      }

      // unexpected_characters
      //
      template<>
      inline
      void unexpected_characters<char>::
      print (std::basic_ostream<char>& os) const
      {
        os << "unexpected characters '" << characters () << "'";
      }

      // invalid_value
      //
      template<>
      inline
      void invalid_value<char>::
      print (std::basic_ostream<char>& os) const
      {
        os << "'" << value () << "' is not a valid value representation "
           << "for type '" << type () << "'";
      }
    }
  }
}

#endif // XSD_CXX_PARSER_EXCEPTIONS_IXX_CHAR
#endif // XSD_CXX_PARSER_USE_CHAR


#if defined(XSD_CXX_PARSER_USE_WCHAR) || !defined(XSD_CXX_PARSER_USE_CHAR)

#ifndef XSD_CXX_PARSER_EXCEPTIONS_IXX_WCHAR
#define XSD_CXX_PARSER_EXCEPTIONS_IXX_WCHAR

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      // error
      //
      inline
      std::basic_ostream<wchar_t>&
      operator<< (std::basic_ostream<wchar_t>& os, const error<wchar_t>& e)
      {
        return os << e.id () << L':' << e.line () << L':' << e.column ()
                  << L" error: " << e.message ();
      }

      // errors
      //
      inline
      std::basic_ostream<wchar_t>&
      operator<< (std::basic_ostream<wchar_t>& os, const errors<wchar_t>& e)
      {
        for (errors<wchar_t>::const_iterator b (e.begin ()), i (b);
             i != e.end ();
             ++i)
        {
          if (i != b)
            os << L"\n";

          os << *i;
        }

        return os;
      }

      // parsing
      //
      template<>
      inline
      void parsing<wchar_t>::
      print (std::basic_ostream<wchar_t>& os) const
      {
        if (errors_.empty ())
          os << L"instance document parsing failed";
        else
          os << errors_;
      }

      // expected_element
      //
      template<>
      inline
      void expected_element<wchar_t>::
      print (std::basic_ostream<wchar_t>& os) const
      {
        os << L"expected element '"
           << expected_namespace ()
           << (expected_namespace ().empty () ? L"" : L"#")
           << expected_name () << L"'";

        if (!encountered_name ().empty ())
          os << L" instead of '"
             << encountered_namespace ()
             << (encountered_namespace ().empty () ? L"" : L"#")
             << encountered_name () << L"'";
      }

      // unexpected_element
      //
      template<>
      inline
      void unexpected_element<wchar_t>::
      print (std::basic_ostream<wchar_t>& os) const
      {
        os << L"unexpected element '"
           << encountered_namespace ()
           << (encountered_namespace ().empty () ? L"" : L"#")
           << encountered_name () << L"'";
      }

      // expected_attribute
      //
      template<>
      inline
      void expected_attribute<wchar_t>::
      print (std::basic_ostream<wchar_t>& os) const
      {
        os << L"expected attribute '"
           << expected_namespace ()
           << (expected_namespace ().empty () ? L"" : L"#")
           << expected_name () << L"'";
      }

      // unexpected_attribute
      //
      template<>
      inline
      void unexpected_attribute<wchar_t>::
      print (std::basic_ostream<wchar_t>& os) const
      {
        os << L"unexpected attribute '"
           << encountered_namespace ()
           << (encountered_namespace ().empty () ? L"" : L"#")
           << encountered_name () << L"'";
      }

      // unexpected_characters
      //
      template<>
      inline
      void unexpected_characters<wchar_t>::
      print (std::basic_ostream<wchar_t>& os) const
      {
        os << L"unexpected characters '" <<  characters () << L"'";
      }

      // invalid_value
      //
      template<>
      inline
      void invalid_value<wchar_t>::
      print (std::basic_ostream<wchar_t>& os) const
      {
        os << L"'" << value () << L"' is not a valid value representation "
           << L"for type '" << type () << L"'";
      }
    }
  }
}

#endif // XSD_CXX_PARSER_EXCEPTIONS_IXX_WCHAR
#endif // XSD_CXX_PARSER_USE_WCHAR
