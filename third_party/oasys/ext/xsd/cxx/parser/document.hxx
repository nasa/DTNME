// file      : xsd/cxx/parser/document.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_PARSER_DOCUMENT_HXX
#define XSD_CXX_PARSER_DOCUMENT_HXX

#include <string>
#include <cstdlib> // std::size_t

#include <xsd/cxx/parser/ro-string.hxx>
#include <xsd/cxx/parser/elements.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      // If you want to use a different underlying XML parser, all you
      // need to do is to route events to this interface and then use
      // the document type below.
      //
      template <typename C>
      struct event_consumer
      {
        virtual
        ~event_consumer ();

        virtual void
        start_element (const ro_string<C>& ns,
                       const ro_string<C>& name) = 0;

        virtual void
        end_element (const ro_string<C>& ns,
                     const ro_string<C>& name) = 0;

        virtual void
        attribute (const ro_string<C>& ns,
                   const ro_string<C>& name,
                   const ro_string<C>& value) = 0;

        virtual void
        characters (const ro_string<C>&) = 0;
      };


      //
      //
      template <typename C>
      struct document: event_consumer<C>
      {
        document (parser_base<C>& root,
                  const std::basic_string<C>& ns,
                  const std::basic_string<C>& name);

      public:
        virtual void
        start_element (const ro_string<C>&, const ro_string<C>&);

        virtual void
        end_element (const ro_string<C>&, const ro_string<C>&);

        virtual void
        attribute (const ro_string<C>&,
                   const ro_string<C>&,
                   const ro_string<C>&);

        virtual void
        characters (const ro_string<C>&);

      private:
        parser_base<C>& root_;
        std::basic_string<C> name_;
        std::basic_string<C> ns_;
        std::size_t depth_;
      };
    }
  }
}

#include <xsd/cxx/parser/document.txx>

#endif  // XSD_CXX_PARSER_DOCUMENT_HXX
