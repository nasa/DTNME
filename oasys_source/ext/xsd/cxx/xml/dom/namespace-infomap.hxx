// file      : xsd/cxx/xml/dom/namespace-infomap.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_XML_DOM_NAMESPACE_INFOMAP_HXX
#define XSD_CXX_XML_DOM_NAMESPACE_INFOMAP_HXX

#include <map>
#include <string>

namespace xsd
{
  namespace cxx
  {
    namespace xml
    {
      namespace dom
      {
        //
        //
        template <typename C>
        struct namespace_info
        {
          typedef std::basic_string<C> string;

          namespace_info ()
          {
          }

          namespace_info (const string& name_, const string& schema_)
              : name (name_),
                schema (schema_)
          {
          }

          std::basic_string<C> name;
          std::basic_string<C> schema;
        };


        // Map of namespace prefix to namespace_info.
        //
        template <typename C>
        struct namespace_infomap:
          public std::map<std::basic_string<C>, namespace_info<C> >
        {
        };
      }
    }
  }
}

#endif  // XSD_CXX_XML_DOM_NAMESPACE_INFOMAP_HXX
