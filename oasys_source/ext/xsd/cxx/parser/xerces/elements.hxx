// file      : xsd/cxx/parser/xerces/elements.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_PARSER_XERCES_ELEMENTS_HXX
#define XSD_CXX_PARSER_XERCES_ELEMENTS_HXX

#include <memory>  // std::auto_ptr
#include <string>
#include <iosfwd>

#include <xercesc/sax/InputSource.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>

#include <xsd/cxx/xml/elements.hxx>
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
      namespace xerces
      {
        //
        //
        struct flags
        {
          // Use the following flags to modify the default behavior
          // of the parsing functions.
          //

          // Do not try to validate instance documents. Note that
          // the xsd runtime assumes instance documents are valid so
          // you had better make sure they are if you specify this flag.
          //
          //
          static const unsigned long dont_validate = 0x00000001;

          // Do not initialize the Xerces-C++ runtime.
          //
          static const unsigned long dont_initialize = 0x00000002;

        public:
          flags (unsigned long x = 0)
              : x_ (x)
          {
          }

          operator unsigned long () const
          {
            return x_;
          }

        private:
          unsigned long x_;
        };


        // Parsing properties. Refer to xsd/cxx/xml/elements.hxx for
        // XML-related properties.
        //
        template <typename C>
        class properties: public xml::properties<C>
        {
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

          // Parse URI or a local file.
          //
        protected:
          void
          parse_ (const std::basic_string<C>& uri,
                  flags,
                  const properties<C>&);

          // With error_handler.
          //

          void
          parse_ (const std::basic_string<C>& uri,
                  xml::error_handler<C>&,
                  flags,
                  const properties<C>&);

          // With ErrorHandler.
          //

          void
          parse_ (const std::basic_string<C>& uri,
                  xercesc::ErrorHandler&,
                  flags,
                  const properties<C>&);

          // Using SAX2XMLReader.
          //

          void
          parse_ (const std::basic_string<C>& uri,
                  xercesc::SAX2XMLReader&,
                  flags,
                  const properties<C>&);


          // Parse std::istream.
          //
        protected:
          void
          parse_ (std::istream&,
                  flags,
                  const properties<C>&);

          // With error_handler.
          //

          void
          parse_ (std::istream&,
                  xml::error_handler<C>&,
                  flags,
                  const properties<C>&);

          // With ErrorHandler.
          //

          void
          parse_ (std::istream&,
                  xercesc::ErrorHandler&,
                  flags,
                  const properties<C>&);

          // Using SAX2XMLReader.
          //

          void
          parse_ (std::istream&,
                  xercesc::SAX2XMLReader&,
                  flags,
                  const properties<C>&);


          // Parse std::istream with a system id.
          //
        protected:
          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  flags,
                  const properties<C>&);

          // With error_handler.
          //

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  xml::error_handler<C>&,
                  flags,
                  const properties<C>&);

          // With ErrorHandler.
          //

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  xercesc::ErrorHandler&,
                  flags,
                  const properties<C>&);

          // Using SAX2XMLReader.
          //

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  xercesc::SAX2XMLReader&,
                  flags,
                  const properties<C>&);



          // Parse std::istream with system and public ids.
          //
        protected:
          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  const std::basic_string<C>& public_id,
                  flags,
                  const properties<C>&);

          // With error_handler.
          //

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  const std::basic_string<C>& public_id,
                  xml::error_handler<C>&,
                  flags,
                  const properties<C>&);

          // With ErrorHandler.
          //

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  const std::basic_string<C>& public_id,
                  xercesc::ErrorHandler&,
                  flags,
                  const properties<C>&);

          // Using SAX2XMLReader.
          //

          void
          parse_ (std::istream&,
                  const std::basic_string<C>& system_id,
                  const std::basic_string<C>& public_id,
                  xercesc::SAX2XMLReader&,
                  flags,
                  const properties<C>&);


          // Parse InputSource.
          //
        protected:
          void
          parse_ (const xercesc::InputSource&,
                  flags,
                  const properties<C>&);

          // With error_handler.
          //

          void
          parse_ (const xercesc::InputSource&,
                  xml::error_handler<C>&,
                  flags,
                  const properties<C>&);

          // With ErrorHandler.
          //

          void
          parse_ (const xercesc::InputSource&,
                  xercesc::ErrorHandler&,
                  flags,
                  const properties<C>&);

          // Using SAX2XMLReader.
          //

          void
          parse_ (const xercesc::InputSource&,
                  xercesc::SAX2XMLReader&,
                  flags,
                  const properties<C>&);


        private:
          void
          parse_ (const std::basic_string<C>& uri,
                  xercesc::ErrorHandler&,
                  xercesc::SAX2XMLReader&,
                  flags,
                  const properties<C>&);

          void
          parse_ (const xercesc::InputSource&,
                  xercesc::ErrorHandler&,
                  xercesc::SAX2XMLReader&,
                  flags,
                  const properties<C>&);

        private:
          std::auto_ptr<xercesc::SAX2XMLReader>
          create_sax_ (flags, const properties<C>&);

        private:
          cxx::parser::document<C> doc_; // VC 7.1 likes it qualified.
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
          // Parse URI or a local file. We have to overload it for const C*
          // bacause xercesc::InputSource has an implicit constructor that
          // takes const char*.
          //
          X
          parse (const std::basic_string<C>& uri,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          X
          parse (const C* uri,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse URI or a local file with a user-provided error_handler
          // object.
          //
          X
          parse (const std::basic_string<C>& uri,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          X
          parse (const C* uri,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse URI or a local file with a user-provided ErrorHandler
          // object. Note that you must initialize the Xerces-C++ runtime
          // before calling these functions.
          //
          X
          parse (const std::basic_string<C>& uri,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          X
          parse (const C* uri,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse URI or a local file using a user-provided SAX2XMLReader
          // object. Note that you must initialize the Xerces-C++ runtime
          // before calling these functions.
          //
          X
          parse (const std::basic_string<C>& uri,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          X
          parse (const C* uri,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


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
          parse (std::istream&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse std::istream with user-provided error_handler object.
          //
          X
          parse (std::istream&,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse std::istream with user-provided ErrorHandler object.
          // Note that you must initialize the Xerces-C++ runtime before
          // calling this function.
          //
          X
          parse (std::istream&,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse std::istream using user-provided SAX2XMLReader object.
          // Note that you must initialize the Xerces-C++ runtime before
          // calling this function.
          //
          X
          parse (std::istream&,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


        public:
          // Parse std::istream with a system id.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse std::istream with a system id and a user-provided
          // error_handler object.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse std::istream with a system id and a user-provided
          // ErrorHandler object. Note that you must initialize the
          // Xerces-C++ runtime before calling this function.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse std::istream with a system id using a user-provided
          // SAX2XMLReader object. Note that you must initialize the
          // Xerces-C++ runtime before calling this function.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());



        public:
          // Parse std::istream with system and public ids.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse std::istream with system and public ids and a user-provided
          // error_handler object.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse std::istream with system and public ids and a user-provided
          // ErrorHandler object. Note that you must initialize the Xerces-C++
          // runtime before calling this function.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse std::istream with system and public ids using a user-
          // provided SAX2XMLReader object. Note that you must initialize
          // the Xerces-C++ runtime before calling this function.
          //
          X
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


        public:
          // Parse InputSource. Note that you must initialize the Xerces-C++
          // runtime before calling this function.
          //
          X
          parse (const xercesc::InputSource&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse InputSource with a user-provided error_handler object.
          // Note that you must initialize the Xerces-C++ runtime before
          // calling this function.
          //
          X
          parse (const xercesc::InputSource&,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse InputSource with a user-provided ErrorHandler object.
          // Note that you must initialize the Xerces-C++ runtime before
          // calling this function.
          //
          X
          parse (const xercesc::InputSource&,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


          // Parse InputSource using a user-provided SAX2XMLReader object.
          // Note that you must initialize the Xerces-C++ runtime before
          // calling this function.
          //
          X
          parse (const xercesc::InputSource&,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

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
          // Parse URI or a local file. We have to overload it for const C*
          // bacause xercesc::InputSource has an implicit constructor that
          // takes const char*.
          //
          void
          parse (const std::basic_string<C>& uri,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const C* uri,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const std::basic_string<C>& uri,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const C* uri,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const std::basic_string<C>& uri,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const C* uri,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const std::basic_string<C>& uri,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const C* uri,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


        public:
          // Parse std::istream.
          //
          void
          parse (std::istream&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (std::istream&,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (std::istream&,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (std::istream&,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


        public:
          // Parse std::istream with a system id.
          //
          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

        public:
          // Parse std::istream with system and public ids.
          //
          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (std::istream&,
                 const std::basic_string<C>& system_id,
                 const std::basic_string<C>& public_id,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());


        public:
          // Parse InputSource.
          //
          void
          parse (const xercesc::InputSource&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const xercesc::InputSource&,
                 xml::error_handler<C>&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const xercesc::InputSource&,
                 xercesc::ErrorHandler&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

          void
          parse (const xercesc::InputSource&,
                 xercesc::SAX2XMLReader&,
                 flags = 0,
                 const properties<C>& = properties<C> ());

        private:
          parser<void, C>& parser_;
        };


        //
        //
        template <typename C>
        struct event_router: xercesc::DefaultHandler
        {
          event_router (event_consumer<C>&);

          // I know, some of those consts are stupid. But that's what
          // Xerces folks put into their interfaces and VC-7.1 thinks
          // there are different signatures if one strips this fluff off.
          //

          virtual void
          startElement(const XMLCh* const uri,
                       const XMLCh* const lname,
                       const XMLCh* const qname,
                       const xercesc::Attributes& attributes);

          virtual void
          endElement(const XMLCh* const uri,
                     const XMLCh* const lname,
                     const XMLCh* const qname);

          virtual void
          characters (const XMLCh* const s, const unsigned int length);

        private:
          event_consumer<C>& consumer_;
        };
      }
    }
  }
}

#include <xsd/cxx/parser/xerces/elements.txx>

#endif  // XSD_CXX_PARSER_XERCES_ELEMENTS_HXX
