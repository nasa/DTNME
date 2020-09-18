// file      : xsd/cxx/tree/type-serializer-map.ixx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#if defined(XSD_CXX_TREE_USE_CHAR) || !defined(XSD_CXX_TREE_USE_WCHAR)

#ifndef XSD_CXX_TREE_TYPE_SERIALIZER_MAP_IXX_CHAR
#define XSD_CXX_TREE_TYPE_SERIALIZER_MAP_IXX_CHAR

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      template <>
      inline type_serializer_map<char>::
      type_serializer_map ()
      {
        // Register factories for built-in non-fundamental types.
        //

        std::basic_string<char> xsd ("http://www.w3.org/2001/XMLSchema");


        // anyType and anySimpleType.
        //
        register_type (
          typeid (type),
          qualified_name<char> ("anyType", xsd),
          &bits::serializer<char, type>::impl,
          false);

        typedef simple_type<type> simple_type;
        register_type (
          typeid (simple_type),
          qualified_name<char> ("anySimpleType", xsd),
          &bits::serializer<char, simple_type>::impl,
          false);


        // Strings
        //
        typedef string<char, simple_type> string;
        register_type (
          typeid (string),
          qualified_name<char> ("string", xsd),
          &bits::serializer<char, string>::impl,
          false);

        typedef normalized_string<char, string> normalized_string;
        register_type (
          typeid (normalized_string),
          qualified_name<char> ("normalizedString", xsd),
          &bits::serializer<char, normalized_string>::impl,
          false);

        typedef token<char, normalized_string> token;
        register_type (
          typeid (token),
          qualified_name<char> ("token", xsd),
          &bits::serializer<char, token>::impl,
          false);

        typedef name<char, token> name;
        register_type (
          typeid (name),
          qualified_name<char> ("Name", xsd),
          &bits::serializer<char, name>::impl,
          false);

        typedef nmtoken<char, token> nmtoken;
        register_type (
          typeid (nmtoken),
          qualified_name<char> ("NMTOKEN", xsd),
          &bits::serializer<char, nmtoken>::impl,
          false);

        typedef nmtokens<char, simple_type, nmtoken> nmtokens;
        register_type (
          typeid (nmtokens),
          qualified_name<char> ("NMTOKENS", xsd),
          &bits::serializer<char, nmtokens>::impl,
          false);

        typedef ncname<char, name> ncname;
        register_type (
          typeid (ncname),
          qualified_name<char> ("NCName", xsd),
          &bits::serializer<char, ncname>::impl,
          false);

        typedef language<char, token> language;
        register_type (
          typeid (language),
          qualified_name<char> ("language", xsd),
          &bits::serializer<char, language>::impl,
          false);


        // ID/IDREF.
        //
        typedef id<char, ncname> id;
        register_type (
          typeid (id),
          qualified_name<char> ("ID", xsd),
          &bits::serializer<char, id>::impl,
          false);

        typedef idref<type, char, ncname> idref;
        register_type (
          typeid (idref),
          qualified_name<char> ("IDREF", xsd),
          &bits::serializer<char, idref>::impl,
          false);

        typedef idrefs<char, simple_type, idref> idrefs;
        register_type (
          typeid (idrefs),
          qualified_name<char> ("IDREFS", xsd),
          &bits::serializer<char, idrefs>::impl,
          false);


        // URI.
        //
        typedef uri<char, simple_type> uri;
        register_type (
          typeid (uri),
          qualified_name<char> ("anyURI", xsd),
          &bits::serializer<char, uri>::impl,
          false);


        // Qualified name.
        //
        typedef qname<char, simple_type, uri, ncname> qname;
        register_type (
          typeid (qname),
          qualified_name<char> ("QName", xsd),
          &bits::serializer<char, qname>::impl,
          false);


        // Binary.
        //
        typedef base64_binary<char, simple_type> base64_binary;
        register_type (
          typeid (base64_binary),
          qualified_name<char> ("base64Binary", xsd),
          &bits::serializer<char, base64_binary>::impl,
          false);

        typedef hex_binary<char, simple_type> hex_binary;
        register_type (
          typeid (hex_binary),
          qualified_name<char> ("hexBinary", xsd),
          &bits::serializer<char, hex_binary>::impl,
          false);


        // Date/time.
        //
        typedef date<char, simple_type> date;
        register_type (
          typeid (date),
          qualified_name<char> ("date", xsd),
          &bits::serializer<char, date>::impl,
          false);

        typedef date_time<char, simple_type> date_time;
        register_type (
          typeid (date_time),
          qualified_name<char> ("dateTime", xsd),
          &bits::serializer<char, date_time>::impl,
          false);

        typedef duration<char, simple_type> duration;
        register_type (
          typeid (duration),
          qualified_name<char> ("duration", xsd),
          &bits::serializer<char, duration>::impl,
          false);

        typedef day<char, simple_type> day;
        register_type (
          typeid (day),
          qualified_name<char> ("gDay", xsd),
          &bits::serializer<char, day >::impl,
          false);

        typedef month<char, simple_type> month;
        register_type (
          typeid (month),
          qualified_name<char> ("gMonth", xsd),
          &bits::serializer<char, month>::impl,
          false);

        typedef month_day<char, simple_type> month_day;
        register_type (
          typeid (month_day),
          qualified_name<char> ("gMonthDay", xsd),
          &bits::serializer<char, month_day>::impl,
          false);

        typedef year<char, simple_type> year;
        register_type (
          typeid (year),
          qualified_name<char> ("gYear", xsd),
          &bits::serializer<char, year>::impl,
          false);

        typedef year_month<char, simple_type> year_month;
        register_type (
          typeid (year_month),
          qualified_name<char> ("gYearMonth", xsd),
          &bits::serializer<char, year_month>::impl,
          false);

        typedef time<char, simple_type> time;
        register_type (
          typeid (time),
          qualified_name<char> ("time", xsd),
          &bits::serializer<char, time>::impl,
          false);


        // Entity.
        //
        typedef entity<char, ncname> entity;
        register_type (
          typeid (entity),
          qualified_name<char> ("ENTITY", xsd),
          &bits::serializer<char, entity>::impl,
          false);

        typedef entities<char, simple_type, entity> entities;
        register_type (
          typeid (entities),
          qualified_name<char> ("ENTITIES", xsd),
          &bits::serializer<char, entities>::impl,
          false);
      }
    }
  }
}

#endif // XSD_CXX_TREE_TYPE_SERIALIZER_MAP_IXX_CHAR
#endif // XSD_CXX_TREE_USE_CHAR


#if defined(XSD_CXX_TREE_USE_WCHAR) || !defined(XSD_CXX_TREE_USE_CHAR)

#ifndef XSD_CXX_TREE_TYPE_SERIALIZER_MAP_IXX_WCHAR
#define XSD_CXX_TREE_TYPE_SERIALIZER_MAP_IXX_WCHAR

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      template <>
      inline type_serializer_map<wchar_t>::
      type_serializer_map ()
      {
        // Register factories for built-in non-fundamental types.
        //

        std::basic_string<wchar_t> xsd (L"http://www.w3.org/2001/XMLSchema");


        // anyType and anySimpleType.
        //
        register_type (
          typeid (type),
          qualified_name<wchar_t> (L"anyType", xsd),
          &bits::serializer<wchar_t, type>::impl,
          false);

        typedef simple_type<type> simple_type;
        register_type (
          typeid (simple_type),
          qualified_name<wchar_t> (L"anySimpleType", xsd),
          &bits::serializer<wchar_t, simple_type>::impl,
          false);


        // Strings
        //
        typedef string<wchar_t, simple_type> string;
        register_type (
          typeid (string),
          qualified_name<wchar_t> (L"string", xsd),
          &bits::serializer<wchar_t, string>::impl,
          false);

        typedef normalized_string<wchar_t, string> normalized_string;
        register_type (
          typeid (normalized_string),
          qualified_name<wchar_t> (L"normalizedString", xsd),
          &bits::serializer<wchar_t, normalized_string>::impl,
          false);

        typedef token<wchar_t, normalized_string> token;
        register_type (
          typeid (token),
          qualified_name<wchar_t> (L"token", xsd),
          &bits::serializer<wchar_t, token>::impl,
          false);

        typedef name<wchar_t, token> name;
        register_type (
          typeid (name),
          qualified_name<wchar_t> (L"Name", xsd),
          &bits::serializer<wchar_t, name>::impl,
          false);

        typedef nmtoken<wchar_t, token> nmtoken;
        register_type (
          typeid (nmtoken),
          qualified_name<wchar_t> (L"NMTOKEN", xsd),
          &bits::serializer<wchar_t, nmtoken>::impl,
          false);

        typedef nmtokens<wchar_t, simple_type, nmtoken> nmtokens;
        register_type (
          typeid (nmtokens),
          qualified_name<wchar_t> (L"NMTOKENS", xsd),
          &bits::serializer<wchar_t, nmtokens>::impl,
          false);

        typedef ncname<wchar_t, name> ncname;
        register_type (
          typeid (ncname),
          qualified_name<wchar_t> (L"NCName", xsd),
          &bits::serializer<wchar_t, ncname>::impl,
          false);

        typedef language<wchar_t, token> language;
        register_type (
          typeid (language),
          qualified_name<wchar_t> (L"language", xsd),
          &bits::serializer<wchar_t, language>::impl,
          false);


        // ID/IDREF.
        //
        typedef id<wchar_t, ncname> id;
        register_type (
          typeid (id),
          qualified_name<wchar_t> (L"ID", xsd),
          &bits::serializer<wchar_t, id>::impl,
          false);

        typedef idref<type, wchar_t, ncname> idref;
        register_type (
          typeid (idref),
          qualified_name<wchar_t> (L"IDREF", xsd),
          &bits::serializer<wchar_t, idref>::impl,
          false);

        typedef idrefs<wchar_t, simple_type, idref> idrefs;
        register_type (
          typeid (idrefs),
          qualified_name<wchar_t> (L"IDREFS", xsd),
          &bits::serializer<wchar_t, idrefs>::impl,
          false);


        // URI.
        //
        typedef uri<wchar_t, simple_type> uri;
        register_type (
          typeid (uri),
          qualified_name<wchar_t> (L"anyURI", xsd),
          &bits::serializer<wchar_t, uri>::impl,
          false);


        // Qualified name.
        //
        typedef qname<wchar_t, simple_type, uri, ncname> qname;
        register_type (
          typeid (qname),
          qualified_name<wchar_t> (L"QName", xsd),
          &bits::serializer<wchar_t, qname>::impl,
          false);


        // Binary.
        //
        typedef base64_binary<wchar_t, simple_type> base64_binary;
        register_type (
          typeid (base64_binary),
          qualified_name<wchar_t> (L"base64Binary", xsd),
          &bits::serializer<wchar_t, base64_binary>::impl,
          false);

        typedef hex_binary<wchar_t, simple_type> hex_binary;
        register_type (
          typeid (hex_binary),
          qualified_name<wchar_t> (L"hexBinary", xsd),
          &bits::serializer<wchar_t, hex_binary>::impl,
          false);


        // Date/time.
        //
        typedef date<wchar_t, simple_type> date;
        register_type (
          typeid (date),
          qualified_name<wchar_t> (L"date", xsd),
          &bits::serializer<wchar_t, date>::impl,
          false);

        typedef date_time<wchar_t, simple_type> date_time;
        register_type (
          typeid (date_time),
          qualified_name<wchar_t> (L"dateTime", xsd),
          &bits::serializer<wchar_t, date_time>::impl,
          false);

        typedef duration<wchar_t, simple_type> duration;
        register_type (
          typeid (duration),
          qualified_name<wchar_t> (L"duration", xsd),
          &bits::serializer<wchar_t, duration>::impl,
          false);

        typedef day<wchar_t, simple_type> day;
        register_type (
          typeid (day),
          qualified_name<wchar_t> (L"gDay", xsd),
          &bits::serializer<wchar_t, day >::impl,
          false);

        typedef month<wchar_t, simple_type> month;
        register_type (
          typeid (month),
          qualified_name<wchar_t> (L"gMonth", xsd),
          &bits::serializer<wchar_t, month>::impl,
          false);

        typedef month_day<wchar_t, simple_type> month_day;
        register_type (
          typeid (month_day),
          qualified_name<wchar_t> (L"gMonthDay", xsd),
          &bits::serializer<wchar_t, month_day>::impl,
          false);

        typedef year<wchar_t, simple_type> year;
        register_type (
          typeid (year),
          qualified_name<wchar_t> (L"gYear", xsd),
          &bits::serializer<wchar_t, year>::impl,
          false);

        typedef year_month<wchar_t, simple_type> year_month;
        register_type (
          typeid (year_month),
          qualified_name<wchar_t> (L"gYearMonth", xsd),
          &bits::serializer<wchar_t, year_month>::impl,
          false);

        typedef time<wchar_t, simple_type> time;
        register_type (
          typeid (time),
          qualified_name<wchar_t> (L"time", xsd),
          &bits::serializer<wchar_t, time>::impl,
          false);


        // Entity.
        //
        typedef entity<wchar_t, ncname> entity;
        register_type (
          typeid (entity),
          qualified_name<wchar_t> (L"ENTITY", xsd),
          &bits::serializer<wchar_t, entity>::impl,
          false);

        typedef entities<wchar_t, simple_type, entity> entities;
        register_type (
          typeid (entities),
          qualified_name<wchar_t> (L"ENTITIES", xsd),
          &bits::serializer<wchar_t, entities>::impl,
          false);
      }
    }
  }
}

#endif // XSD_CXX_TREE_TYPE_SERIALIZER_MAP_IXX_WCHAR
#endif // XSD_CXX_TREE_USE_WCHAR
