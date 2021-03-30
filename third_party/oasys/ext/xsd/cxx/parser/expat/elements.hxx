// file      : xsd/cxx/parser/expat/elements.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_PARSER_EXPAT_ELEMENTS_HXX
#define XSD_CXX_PARSER_EXPAT_ELEMENTS_HXX

#include <string>
#include <iosfwd>

#include <expat.h>

#include <xsd/cxx/xml/error-handler.hxx>

#include <xsd/cxx/parser/exceptions.hxx>
#include <xsd/cxx/parser/elements.hxx>
#include <xsd/cxx/parser/document.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      namespace expat
      {

        // Simple auto pointer for Expat's XML_Parser object.
        //
        struct parser_auto_ptr
        {
          ~parser_auto_ptr ()
          {
            if (parser_ != 0)
              XML_ParserFree (parser_);
          }

          parser_auto_ptr (XML_Parser parser)
              : parser_ (parser)
          {
          }

        public:
          operator XML_Parser ()
          {
            return parser_;
          }

        private:
          parser_auto_ptr (const parser_auto_ptr&);
          parser_auto_ptr&
          operator= (const parser_auto_ptr&);

        private:
          XML_Parser parser_;
        };


        //
        //
        template <typename C>
        struct event_router
        {
        public:
          event_router (event_consumer<C>& consumer);

        public:
          static void XMLCALL
          start_element (void*, const XML_Char*, const XML_Char**);

          static void XMLCALL
          end_element (void*, const XML_Char*);

          static void XMLCALL
          characters (void*, const XML_Char*, int);

        private:
          void
          start_element_ (const XML_Char* ns_name, const XML_Char** atts);

          void
          end_element_ (const XML_Char* ns_name);

          void
          characters_ (const XML_Char* s, std::size_t n);

        private:
          XML_Parser parser_;
          event_consumer<C>& consumer_;
        };


        // Common code before generic and void-specialization split.
        //
        template <typename C>
        struct document_common
        {
        protected:
          document_common (parser_base<C>&,
                           const std::basic_string<C>& root_namespace,
                           const std::basic_string<C>& root_name);

        protected:
          void
          parse_ (const std::basic_string<C>& file);

          void
          parse_ (const std::basic_string<C>& file,
                  xml::error_handler<C>&);

        protected:
          void
          parse_ (std::istream&);

          void
          parse_ (std::istream&,
                  xml::error_handler<C>&);

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id);

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  xml::error_handler<C>&);

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  const std::basic_string<C>& public_id);

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  const std::basic_string<C>& public_id,
                  xml::error_handler<C>&);

        protected:
          void
          parse_begin_ (XML_Parser);

          void
          parse_end_ ();

        private:
          bool
          parse_ (std::istream&,
                  const std::basic_string<C>* system_id,
                  const std::basic_string<C>* public_id,
                  xml::error_handler<C>&);

        private:
          void
          set ();

          void
          clear ();

        private:
          cxx::parser::document<C> doc_; // VC 7.1 likes it qualified.
          event_router<C> router_;

        protected:
          XML_Parser xml_parser_;
        };


        //
        //
        template <typename X, typename C>
        struct document: document_common<C>
        {
        public:
          document (parser<X, C>&,
                    const std::basic_string<C>& root_element_name);

          document (parser<X, C>&,
                    const std::basic_string<C>& root_element_namespace,
                    const std::basic_string<C>& root_element_name);

        public:
          // Parse a local file. The file is accessed with std::ifstream
          // in binary mode. std::ios_base::failure exception is used to
          // report io errors (badbit and failbit).
          X
          parse (const std::basic_string<C>& file);

          // Parse a local file with a user-provided error_handler
          // object. The file is accessed with std::ifstream in binary
          // mode. std::ios_base::failure exception is used to report
          // io errors (badbit and failbit).
          //
          X
          parse (const std::basic_string<C>& file, xml::error_handler<C>&);

        public:
          // System id is a "system" identifier of the resources (e.g.,
          // URI or a full file path). Public id is a "public" identifier
          // of the resource (e.g., application-specific name or relative
          // file path). System id is used to resolve relative paths.
          // In diagnostics messages system id is used if public id is
          // not available. Otherwise public id is used.
          //
          // Note that these sematics are open for interpretation. Both
          // system and public ids are optional and you can view them as
          // a way to associate two strings with the stream being parsed
          // which are made available to you in error handlers.
          //


          // Parse std::istream.
          //
          X
          parse (std::istream&);

          // Parse std::istream with user-provided error_handler object.
          //
          X
          parse (std::istream&, xml::error_handler<C>&);

          // Parse std::istream with a system id.
          //
          X
          parse (std::istream&, const std::basic_string<C>& system_id);

          // Parse std::istream with a system id and a user-provided
          // error_handler object.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 xml::error_handler<C>&);

          // Parse std::istream with system and public ids.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id);

          // Parse std::istream with system and public ids and a user-provided
          // error_handler object.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 xml::error_handler<C>&);

        public:
          // Low-level Expat-specific parsing API. A typical use case
          // would look like this (pseudo-code):
          //
          // XML_Parser xml_parser (XML_ParserCreateNS (0, ' '));
          // doc.parse_begin (xml_parser);
          //
          // while (more_stuff_to_parse)
          // {
          //    // Call XML_Parse or XML_ParseBuffer
          //    // Handle errors if any.
          // }
          //
          // result_type result (doc.parse_end ());
          //
          // Notes:
          //
          // 1. If your XML instances use XML namespaces, XML_ParserCreateNS
          //    functions should be used to create the XML parser. Space
          //    (XML_Char (' ')) should be used as a separator (the second
          //    argument to XML_ParserCreateNS).
          //
          void
          parse_begin (XML_Parser);

          X
          parse_end ();

        private:
          parser<X, C>& parser_;
        };


        // Specialization for void.
        //
        template <typename C>
        struct document<void, C>: document_common<C>
        {
        public:
          document (parser<void, C>&,
                    const std::basic_string<C>& root_element_name);

          document (parser<void, C>&,
                    const std::basic_string<C>& root_element_namespace,
                    const std::basic_string<C>& root_element_name);

        public:
          void
          parse (const std::basic_string<C>& file);

          void
          parse (const std::basic_string<C>& file, xml::error_handler<C>&);

        public:
          void
          parse (std::istream&);

          void
          parse (std::istream&, xml::error_handler<C>&);

          void
          parse (std::istream&, const std::basic_string<C>& system_id);

          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 xml::error_handler<C>&);

          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id);

          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 xml::error_handler<C>&);

        public:
          void
          parse_begin (XML_Parser);

          void
          parse_end ();

        private:
          parser<void, C>& parser_;
        };
      }
    }
  }
}

#include <xsd/cxx/parser/expat/elements.txx>

#endif  // XSD_CXX_PARSER_EXPAT_ELEMENTS_HXX
