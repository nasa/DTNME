// file      : xsd/cxx/tree/type-factory-map.ixx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#if defined(XSD_CXX_TREE_USE_CHAR) || !defined(XSD_CXX_TREE_USE_WCHAR)

#ifndef XSD_CXX_TREE_TYPE_FACTORY_MAP_IXX_CHAR
#define XSD_CXX_TREE_TYPE_FACTORY_MAP_IXX_CHAR

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      template <>
      inline type_factory_map<char>::
      type_factory_map ()
      {
        // Register factories for default instantiations of built-in,
        // non-fundamental types.
        //

        std::basic_string<char> xsd ("http://www.w3.org/2001/XMLSchema");


        // anyType and anySimpleType.
        //
        register_type (
          "anyType " + xsd,
          &factory_impl<char, type>,
          false);

        typedef simple_type<type> simple_type;
        register_type (
          "anySimpleType " + xsd,
          &factory_impl<char, simple_type>,
          false);


        // Strings
        //
        typedef string<char, simple_type> string;
        register_type (
          "string " + xsd,
          &factory_impl<char, string>,
          false);

        typedef normalized_string<char, string> normalized_string;
        register_type (
          "normalizedString " + xsd,
          &factory_impl<char, normalized_string>,
          false);

        typedef token<char, normalized_string> token;
        register_type (
          "token " + xsd,
          &factory_impl<char, token>,
          false);

        typedef name<char, token> name;
        register_type (
          "Name " + xsd,
          &factory_impl<char, name>,
          false);

        typedef nmtoken<char, token> nmtoken;
        register_type (
          "NMTOKEN " + xsd,
          &factory_impl<char, nmtoken>,
          false);

        register_type (
          "NMTOKENS " + xsd,
          &factory_impl<char, nmtokens<char, simple_type, nmtoken> >,
          false);

        typedef ncname<char, name> ncname;
        register_type (
          "NCName " + xsd,
          &factory_impl<char, ncname>,
          false);

        register_type (
          "language " + xsd,
          &factory_impl<char, language<char, token> >,
          false);


        // ID/IDREF.
        //
        register_type (
          "ID " + xsd,
          &factory_impl<char, id<char, ncname> >,
          false);

        typedef idref<type, char, ncname> idref;
        register_type (
          "IDREF " + xsd,
          &factory_impl<char, idref>,
          false);

        register_type (
          "IDREFS " + xsd,
          &factory_impl<char, idrefs<char, simple_type, idref> >,
          false);


        // URI.
        //
        typedef uri<char, simple_type> uri;
        register_type (
          "anyURI " + xsd,
          &factory_impl<char, uri>,
          false);


        // Qualified name.
        //
        register_type (
          "QName " + xsd,
          &factory_impl<char, qname<char, simple_type, uri, ncname> >,
          false);


        // Binary.
        //
        register_type (
          "base64Binary " + xsd,
          &factory_impl<char, base64_binary<char, simple_type> >,
          false);

        register_type (
          "hexBinary " + xsd,
          &factory_impl<char, hex_binary<char, simple_type> >,
          false);


        // Date/time.
        //
        register_type (
          "date " + xsd,
          &factory_impl<char, date<char, simple_type> >,
          false);

        register_type (
          "dateTime " + xsd,
          &factory_impl<char, date_time<char, simple_type> >,
          false);

        register_type (
          "duration " + xsd,
          &factory_impl<char, duration<char, simple_type> >,
          false);

        register_type (
          "gDay " + xsd,
          &factory_impl<char, day<char, simple_type> >,
          false);

        register_type (
          "gMonth " + xsd,
          &factory_impl<char, month<char, simple_type> >,
          false);

        register_type (
          "gMonthDay " + xsd,
          &factory_impl<char, month_day<char, simple_type> >,
          false);

        register_type (
          "gYear " + xsd,
          &factory_impl<char, year<char, simple_type> >,
          false);

        register_type (
          "gYearMonth " + xsd,
          &factory_impl<char, year_month<char, simple_type> >,
          false);

        register_type (
          "time " + xsd,
          &factory_impl<char, time<char, simple_type> >,
          false);


        // Entity.
        //
        typedef entity<char, ncname> entity;
        register_type (
          "ENTITY " + xsd,
          &factory_impl<char, entity>,
          false);

        register_type (
          "ENTITIES " + xsd,
          &factory_impl<char, entities<char, simple_type, entity> >,
          false);
      }
    }
  }
}

#endif // XSD_CXX_TREE_TYPE_FACTORY_MAP_IXX_CHAR
#endif // XSD_CXX_TREE_USE_CHAR


#if defined(XSD_CXX_TREE_USE_WCHAR) || !defined(XSD_CXX_TREE_USE_CHAR)

#ifndef XSD_CXX_TREE_TYPE_FACTORY_MAP_IXX_WCHAR
#define XSD_CXX_TREE_TYPE_FACTORY_MAP_IXX_WCHAR

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      template <>
      inline type_factory_map<wchar_t>::
      type_factory_map ()
      {
        // Register factories for built-in non-fundamental types.
        //

        std::basic_string<wchar_t> xsd (L"http://www.w3.org/2001/XMLSchema");


        // anyType and anySimpleType.
        //
        register_type (
          L"anyType " + xsd,
          &factory_impl<wchar_t, type>,
          false);

        typedef simple_type<type> simple_type;

        register_type (
          L"anySimpleType " + xsd,
          &factory_impl<wchar_t, simple_type>,
          false);


        // Strings
        //
        typedef string<wchar_t, simple_type> string;
        register_type (
          L"string " + xsd,
          &factory_impl<wchar_t, string>,
          false);

        typedef normalized_string<wchar_t, string> normalized_string;
        register_type (
          L"normalizedString " + xsd,
          &factory_impl<wchar_t, normalized_string>,
          false);

        typedef token<wchar_t, normalized_string> token;
        register_type (
          L"token " + xsd,
          &factory_impl<wchar_t, token>,
          false);

        typedef name<wchar_t, token> name;
        register_type (
          L"Name " + xsd,
          &factory_impl<wchar_t, name>,
          false);

        typedef nmtoken<wchar_t, token> nmtoken;
        register_type (
          L"NMTOKEN " + xsd,
          &factory_impl<wchar_t, nmtoken>,
          false);

        register_type (
          L"NMTOKENS " + xsd,
          &factory_impl<wchar_t, nmtokens<wchar_t, simple_type, nmtoken> >,
          false);

        typedef ncname<wchar_t, name> ncname;
        register_type (
          L"NCName " + xsd,
          &factory_impl<wchar_t, ncname>,
          false);

        register_type (
          L"language " + xsd,
          &factory_impl<wchar_t, language<wchar_t, token> >,
          false);


        // ID/IDREF.
        //
        register_type (
          L"ID " + xsd,
          &factory_impl<wchar_t, id<wchar_t, ncname> >,
          false);

        typedef idref<type, wchar_t, ncname> idref;
        register_type (
          L"IDREF " + xsd,
          &factory_impl<wchar_t, idref>,
          false);

        register_type (
          L"IDREFS " + xsd,
          &factory_impl<wchar_t, idrefs<wchar_t, simple_type, idref> >,
          false);


        // URI.
        //
        typedef uri<wchar_t, simple_type> uri;
        register_type (
          L"anyURI " + xsd,
          &factory_impl<wchar_t, uri>,
          false);


        // Qualified name.
        //
        register_type (
          L"QName " + xsd,
          &factory_impl<wchar_t, qname<wchar_t, simple_type, uri, ncname> >,
          false);


        // Binary.
        //
        register_type (
          L"base64Binary " + xsd,
          &factory_impl<wchar_t, base64_binary<wchar_t, simple_type> >,
          false);

        register_type (
          L"hexBinary " + xsd,
          &factory_impl<wchar_t, hex_binary<wchar_t, simple_type> >,
          false);


        // Date/time.
        //
        register_type (
          L"date " + xsd,
          &factory_impl<wchar_t, date<wchar_t, simple_type> >,
          false);

        register_type (
          L"dateTime " + xsd,
          &factory_impl<wchar_t, date_time<wchar_t, simple_type> >,
          false);

        register_type (
          L"duration " + xsd,
          &factory_impl<wchar_t, duration<wchar_t, simple_type> >,
          false);

        register_type (
          L"gDay " + xsd,
          &factory_impl<wchar_t, day<wchar_t, simple_type> >,
          false);

        register_type (
          L"gMonth " + xsd,
          &factory_impl<wchar_t, month<wchar_t, simple_type> >,
          false);

        register_type (
          L"gMonthDay " + xsd,
          &factory_impl<wchar_t, month_day<wchar_t, simple_type> >,
          false);

        register_type (
          L"gYear " + xsd,
          &factory_impl<wchar_t, year<wchar_t, simple_type> >,
          false);

        register_type (
          L"gYearMonth " + xsd,
          &factory_impl<wchar_t, year_month<wchar_t, simple_type> >,
          false);

        register_type (
          L"time " + xsd,
          &factory_impl<wchar_t, time<wchar_t, simple_type> >,
          false);


        // Entity.
        //
        typedef entity<wchar_t, ncname> entity;
        register_type (
          L"ENTITY " + xsd,
          &factory_impl<wchar_t, entity>,
          false);

        register_type (
          L"ENTITIES " + xsd,
          &factory_impl<wchar_t, entities<wchar_t, simple_type, entity> >,
          false);
      }
    }
  }
}

#endif // XSD_CXX_TREE_TYPE_FACTORY_MAP_IXX_WCHAR
#endif // XSD_CXX_TREE_USE_WCHAR
