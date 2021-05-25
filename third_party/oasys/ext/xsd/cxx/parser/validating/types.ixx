// file      : xsd/cxx/parser/validating/types.ixx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#if defined(XSD_CXX_PARSER_USE_CHAR) || !defined(XSD_CXX_PARSER_USE_WCHAR)

#ifndef XSD_CXX_PARSER_VALIDATING_TYPES_IXX_CHAR
#define XSD_CXX_PARSER_VALIDATING_TYPES_IXX_CHAR

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      namespace validating
      {
        namespace bits
        {
          template<>
          inline const char*
          byte<char> ()
          {
            return "byte";
          }

          template<>
          inline const char*
          unsigned_byte<char> ()
          {
            return "unsignedByte";
          }

          template<>
          inline const char*
          short_<char> ()
          {
            return "short";
          }

          template<>
          inline const char*
          unsigned_short<char> ()
          {
            return "unsignedShort";
          }

          template<>
          inline const char*
          int_<char> ()
          {
            return "int";
          }

          template<>
          inline const char*
          unsigned_int<char> ()
          {
            return "unsignedInt";
          }

          template<>
          inline const char*
          long_<char> ()
          {
            return "long";
          }

          template<>
          inline const char*
          unsigned_long<char> ()
          {
            return "unsignedLong";
          }

          template<>
          inline const char*
          boolean<char> ()
          {
            return "boolean";
          }

          template<>
          inline const char*
          float_<char> ()
          {
            return "float";
          }

          template<>
          inline const char*
          double_<char> ()
          {
            return "double";
          }

          template<>
          inline const char*
          decimal<char> ()
          {
            return "decimal";
          }

          //
          //
          template<>
          inline const char*
          positive_inf<char> ()
          {
            return "INF";
          }

          template<>
          inline const char*
          negative_inf<char> ()
          {
            return "-INF";
          }

          template<>
          inline const char*
          nan<char> ()
          {
            return "NaN";
          }

          //
          //
          template<>
          inline const char*
          true_<char> ()
          {
            return "true";
          }

          template<>
          inline const char*
          false_<char> ()
          {
            return "false";
          }

          template<>
          inline const char*
          one<char> ()
          {
            return "1";
          }

          template<>
          inline const char*
          zero<char> ()
          {
            return "0";
          }
        }
      }
    }
  }
}

#endif // XSD_CXX_PARSER_VALIDATING_TYPES_IXX_CHAR
#endif // XSD_CXX_PARSER_USE_CHAR


#if defined(XSD_CXX_PARSER_USE_WCHAR) || !defined(XSD_CXX_PARSER_USE_CHAR)

#ifndef XSD_CXX_PARSER_VALIDATING_TYPES_IXX_WCHAR
#define XSD_CXX_PARSER_VALIDATING_TYPES_IXX_WCHAR

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      namespace validating
      {
        namespace bits
        {
          template<>
          inline const wchar_t*
          byte<wchar_t> ()
          {
            return L"byte";
          }

          template<>
          inline const wchar_t*
          unsigned_byte<wchar_t> ()
          {
            return L"unsignedByte";
          }

          template<>
          inline const wchar_t*
          short_<wchar_t> ()
          {
            return L"short";
          }

          template<>
          inline const wchar_t*
          unsigned_short<wchar_t> ()
          {
            return L"unsignedShort";
          }

          template<>
          inline const wchar_t*
          int_<wchar_t> ()
          {
            return L"int";
          }

          template<>
          inline const wchar_t*
          unsigned_int<wchar_t> ()
          {
            return L"unsignedInt";
          }

          template<>
          inline const wchar_t*
          long_<wchar_t> ()
          {
            return L"long";
          }

          template<>
          inline const wchar_t*
          unsigned_long<wchar_t> ()
          {
            return L"unsignedLong";
          }

          template<>
          inline const wchar_t*
          boolean<wchar_t> ()
          {
            return L"boolean";
          }

          template<>
          inline const wchar_t*
          float_<wchar_t> ()
          {
            return L"float";
          }

          template<>
          inline const wchar_t*
          double_<wchar_t> ()
          {
            return L"double";
          }

          template<>
          inline const wchar_t*
          decimal<wchar_t> ()
          {
            return L"decimal";
          }

          //
          //
          template<>
          inline const wchar_t*
          positive_inf<wchar_t> ()
          {
            return L"INF";
          }

          template<>
          inline const wchar_t*
          negative_inf<wchar_t> ()
          {
            return L"-INF";
          }

          template<>
          inline const wchar_t*
          nan<wchar_t> ()
          {
            return L"NaN";
          }

          //
          //
          template<>
          inline const wchar_t*
          true_<wchar_t> ()
          {
            return L"true";
          }

          template<>
          inline const wchar_t*
          false_<wchar_t> ()
          {
            return L"false";
          }

          template<>
          inline const wchar_t*
          one<wchar_t> ()
          {
            return L"1";
          }

          template<>
          inline const wchar_t*
          zero<wchar_t> ()
          {
            return L"0";
          }
        }
      }
    }
  }
}

#endif // XSD_CXX_PARSER_VALIDATING_TYPES_IXX_WCHAR
#endif // XSD_CXX_PARSER_USE_WCHAR
