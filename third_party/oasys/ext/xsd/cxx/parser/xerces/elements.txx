// file      : xsd/cxx/parser/xerces/elements.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <istream>

#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>

#include <xsd/cxx/xml/string.hxx>
#include <xsd/cxx/xml/sax/std-input-source.hxx>
#include <xsd/cxx/xml/sax/bits/error-handler-proxy.hxx>

#include <xsd/cxx/parser/error-handler.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      namespace xerces
      {

        // document_common
        //

        template <typename C>
        document_common<C>::
        document_common (parser_base<C>& parser,
                         const std::basic_string<C>& ns,
                         const std::basic_string<C>& name)
            : doc_ (parser, ns, name)
        {
        }

        // parse (uri)
        //
        template <typename C>
        void document_common<C>::
        parse_ (const std::basic_string<C>& uri,
                flags f,
                const properties<C>& p)
        {
          xml::auto_initializer init ((f & flags::dont_initialize) == 0);

          error_handler<C> eh;
          xml::sax::bits::error_handler_proxy<C> eh_proxy (eh);
          std::auto_ptr<xercesc::SAX2XMLReader> sax (create_sax_ (f, p));

          parse_ (uri, eh_proxy, *sax, f, p);

          eh.throw_if_failed ();
        }

        // error_handler
        //

        template <typename C>
        void document_common<C>::
        parse_ (const std::basic_string<C>& uri,
                xml::error_handler<C>& eh,
                flags f,
                const properties<C>& p)
        {
          xml::auto_initializer init ((f & flags::dont_initialize) == 0);

          xml::sax::bits::error_handler_proxy<C> eh_proxy (eh);
          std::auto_ptr<xercesc::SAX2XMLReader> sax (create_sax_ (f, p));

          parse_ (uri, eh_proxy, *sax, f, p);

          if (eh_proxy.failed ())
            throw parsing<C> ();
        }

        // ErrorHandler
        //

        template <typename C>
        void document_common<C>::
        parse_ (const std::basic_string<C>& uri,
                xercesc::ErrorHandler& eh,
                flags f,
                const properties<C>& p)
        {
          xml::sax::bits::error_handler_proxy<C> eh_proxy (eh);
          std::auto_ptr<xercesc::SAX2XMLReader> sax (create_sax_ (f, p));

          parse_ (uri, eh_proxy, *sax, f, p);

          if (eh_proxy.failed ())
            throw parsing<C> ();
        }

        // SAX2XMLReader
        //

        template <typename C>
        void document_common<C>::
        parse_ (const std::basic_string<C>& uri,
                xercesc::SAX2XMLReader& sax,
                flags f,
                const properties<C>& p)
        {
          // If there is no error handler, then fall back on the default
          // implementation.
          //
          xercesc::ErrorHandler* eh (sax.getErrorHandler ());

          if (eh)
          {
            xml::sax::bits::error_handler_proxy<C> eh_proxy (*eh);

            parse_ (uri, eh_proxy, sax, f, p);

            if (eh_proxy.failed ())
              throw parsing<C> ();
          }
          else
          {
            error_handler<C> fallback_eh;
            xml::sax::bits::error_handler_proxy<C> eh_proxy (fallback_eh);

            parse_ (uri, eh_proxy, sax, f, p);

            fallback_eh.throw_if_failed ();
          }
        }


        // parse (istream)
        //

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                flags f,
                const properties<C>& p)
        {
          xml::auto_initializer init ((f & flags::dont_initialize) == 0);

          xml::sax::std_input_source isrc (is);

          parse_ (isrc, f, p);
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                xml::error_handler<C>& eh,
                flags f,
                const properties<C>& p)
        {
          xml::auto_initializer init ((f & flags::dont_initialize) == 0);

          xml::sax::std_input_source isrc (is);

          parse_ (isrc, eh, f, p);
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                xercesc::ErrorHandler& eh,
                flags f,
                const properties<C>& p)
        {
          xml::sax::std_input_source isrc (is);

          parse_ (isrc, eh, f, p);
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                xercesc::SAX2XMLReader& sax,
                flags f,
                const properties<C>& p)
        {
          xml::sax::std_input_source isrc (is);

          parse_ (isrc, sax, f, p);
        }


        // parse (istream, system_id)
        //


        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                flags f,
                const properties<C>& p)
        {
          xml::auto_initializer init ((f & flags::dont_initialize) == 0);

          xml::sax::std_input_source isrc (is, system_id);

          parse_ (isrc, f, p);
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                xml::error_handler<C>& eh,
                flags f,
                const properties<C>& p)
        {
          xml::auto_initializer init ((f & flags::dont_initialize) == 0);

          xml::sax::std_input_source isrc (is, system_id);

          parse_ (isrc, eh, f, p);
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                xercesc::ErrorHandler& eh,
                flags f,
                const properties<C>& p)
        {
          xml::sax::std_input_source isrc (is, system_id);

          parse_ (isrc, eh, f, p);
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                xercesc::SAX2XMLReader& sax,
                flags f,
                const properties<C>& p)
        {
          xml::sax::std_input_source isrc (is, system_id);

          parse_ (isrc, sax, f, p);
        }


        // parse (istream, system_id, public_id)
        //

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                const std::basic_string<C>& public_id,
                flags f,
                const properties<C>& p)
        {
          xml::auto_initializer init ((f & flags::dont_initialize) == 0);

          xml::sax::std_input_source isrc (is, system_id, public_id);

          parse_ (isrc, f, p);
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                const std::basic_string<C>& public_id,
                xml::error_handler<C>& eh,
                flags f,
                const properties<C>& p)
        {
          xml::auto_initializer init ((f & flags::dont_initialize) == 0);

          xml::sax::std_input_source isrc (is, system_id, public_id);

          parse_ (isrc, eh, f, p);
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                const std::basic_string<C>& public_id,
                xercesc::ErrorHandler& eh,
                flags f,
                const properties<C>& p)
        {
          xml::sax::std_input_source isrc (is, system_id, public_id);

          parse_ (isrc, eh, f, p);
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                const std::basic_string<C>& public_id,
                xercesc::SAX2XMLReader& sax,
                flags f,
                const properties<C>& p)
        {
          xml::sax::std_input_source isrc (is, system_id, public_id);

          parse_ (isrc, sax, f, p);
        }


        // parse (InputSource)
        //


        template <typename C>
        void document_common<C>::
        parse_ (const xercesc::InputSource& is,
                flags f,
                const properties<C>& p)
        {
          error_handler<C> eh;
          xml::sax::bits::error_handler_proxy<C> eh_proxy (eh);
          std::auto_ptr<xercesc::SAX2XMLReader> sax (create_sax_ (f, p));

          parse_ (is, eh_proxy, *sax, f, p);

          eh.throw_if_failed ();
        }

        template <typename C>
        void document_common<C>::
        parse_ (const xercesc::InputSource& is,
                xml::error_handler<C>& eh,
                flags f,
                const properties<C>& p)
        {
          xml::sax::bits::error_handler_proxy<C> eh_proxy (eh);
          std::auto_ptr<xercesc::SAX2XMLReader> sax (create_sax_ (f, p));

          parse_ (is, eh_proxy, *sax, f, p);

          if (eh_proxy.failed ())
            throw parsing<C> ();
        }

        template <typename C>
        void document_common<C>::
        parse_ (const xercesc::InputSource& is,
                xercesc::ErrorHandler& eh,
                flags f,
                const properties<C>& p)
        {
          xml::sax::bits::error_handler_proxy<C> eh_proxy (eh);
          std::auto_ptr<xercesc::SAX2XMLReader> sax (create_sax_ (f, p));

          parse_ (is, eh_proxy, *sax, f, p);

          if (eh_proxy.failed ())
            throw parsing<C> ();
        }


        template <typename C>
        void document_common<C>::
        parse_ (const xercesc::InputSource& is,
                xercesc::SAX2XMLReader& sax,
                flags f,
                const properties<C>& p)
        {
          // If there is no error handler, then fall back on the default
          // implementation.
          //
          xercesc::ErrorHandler* eh (sax.getErrorHandler ());

          if (eh)
          {
            xml::sax::bits::error_handler_proxy<C> eh_proxy (*eh);

            parse_ (is, eh_proxy, sax, f, p);

            if (eh_proxy.failed ())
              throw parsing<C> ();
          }
          else
          {
            error_handler<C> fallback_eh;
            xml::sax::bits::error_handler_proxy<C> eh_proxy (fallback_eh);

            parse_ (is, eh_proxy, sax, f, p);

            fallback_eh.throw_if_failed ();
          }
        }

        namespace Bits
        {
          struct ErrorHandlingController
          {
            ErrorHandlingController (xercesc::SAX2XMLReader& sax,
                                     xercesc::ErrorHandler& eh)
                : sax_ (sax), eh_ (sax_.getErrorHandler ())
            {
              sax_.setErrorHandler (&eh);
            }

            ~ErrorHandlingController ()
            {
              sax_.setErrorHandler (eh_);
            }

          private:
            xercesc::SAX2XMLReader& sax_;
            xercesc::ErrorHandler* eh_;
          };

          struct ContentHandlingController
          {
            ContentHandlingController (xercesc::SAX2XMLReader& sax,
                                       xercesc::ContentHandler& ch)
                : sax_ (sax), ch_ (sax_.getContentHandler ())
            {
              sax_.setContentHandler (&ch);
            }

            ~ContentHandlingController ()
            {
              sax_.setContentHandler (ch_);
            }

          private:
            xercesc::SAX2XMLReader& sax_;
            xercesc::ContentHandler* ch_;
          };
        };

        template <typename C>
        void document_common<C>::
        parse_ (const std::basic_string<C>& uri,
                xercesc::ErrorHandler& eh,
                xercesc::SAX2XMLReader& sax,
                flags,
                const properties<C>&)
        {
          event_router<C> router (doc_);

          Bits::ErrorHandlingController ehc (sax, eh);
          Bits::ContentHandlingController chc (sax, router);

          sax.parse (xml::string (uri).c_str ());
        }

        template <typename C>
        void document_common<C>::
        parse_ (const xercesc::InputSource& is,
                xercesc::ErrorHandler& eh,
                xercesc::SAX2XMLReader& sax,
                flags,
                const properties<C>&)
        {
          event_router<C> router (doc_);

          Bits::ErrorHandlingController controller (sax, eh);
          Bits::ContentHandlingController chc (sax, router);

          sax.parse (is);
        }


        template <typename C>
        std::auto_ptr<xercesc::SAX2XMLReader> document_common<C>::
        create_sax_ (flags f, const properties<C>& p)
        {
          // HP aCC cannot handle using namespace xercesc;
          //
          using xercesc::SAX2XMLReader;
          using xercesc::XMLReaderFactory;
          using xercesc::XMLUni;

          std::auto_ptr<SAX2XMLReader> sax (
            XMLReaderFactory::createXMLReader ());

          sax->setFeature (XMLUni::fgSAX2CoreNameSpaces, true);
          sax->setFeature (XMLUni::fgSAX2CoreNameSpacePrefixes, true);
          sax->setFeature (XMLUni::fgXercesValidationErrorAsFatal, true);

          if (f & flags::dont_validate)
          {
            sax->setFeature (XMLUni::fgSAX2CoreValidation, false);
            sax->setFeature (XMLUni::fgXercesSchema, false);
            sax->setFeature (XMLUni::fgXercesSchemaFullChecking, false);
          }
          else
          {
            sax->setFeature (XMLUni::fgSAX2CoreValidation, true);
            sax->setFeature (XMLUni::fgXercesSchema, true);

            // This feature checks the schema grammar for additional
            // errors. We most likely do not need it when validating
            // instances (assuming the schema is valid).
            //
            sax->setFeature (XMLUni::fgXercesSchemaFullChecking, false);
          }

          // Transfer properies if any.
          //

          if (!p.schema_location ().empty ())
          {
            xml::string sl (p.schema_location ());
            const void* v (sl.c_str ());

            sax->setProperty (
              XMLUni::fgXercesSchemaExternalSchemaLocation,
              const_cast<void*> (v));
          }

          if (!p.no_namespace_schema_location ().empty ())
          {
            xml::string sl (p.no_namespace_schema_location ());
            const void* v (sl.c_str ());

            sax->setProperty (
              XMLUni::fgXercesSchemaExternalNoNameSpaceSchemaLocation,
              const_cast<void*> (v));
          }

          return sax;
        }


        // document
        //

        template <typename X, typename C>
        document<X, C>::
        document (parser<X, C>& p, const std::basic_string<C>& name)
            : document_common<C> (p, std::basic_string<C> (), name),
              parser_ (p)
        {
        }

        template <typename X, typename C>
        document<X, C>::
        document (parser<X, C>& p,
                  const std::basic_string<C>& ns,
                  const std::basic_string<C>& name)
            : document_common<C> (p, ns, name), parser_ (p)
        {
        }

        // parse (uri)
        //
        template <typename X, typename C>
        X document<X, C>::
        parse (const std::basic_string<C>& uri,
               flags f,
               const properties<C>& p)
        {
          parse_ (uri, f, p);
          return parser_.post ();
        }


        template <typename X, typename C>
        X document<X, C>::
        parse (const C* uri,
               flags f,
               const properties<C>& p)
        {
          parse_ (
            std::basic_string<C> (uri), f, p);
          return parser_.post ();
        }

        // error_handler
        //

        template <typename X, typename C>
        X document<X, C>::
        parse (const std::basic_string<C>& uri,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (uri, eh, f, p);
          return parser_.post ();
        }


        template <typename X, typename C>
        X document<X, C>::
        parse (const C* uri,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (std::basic_string<C> (uri), eh, f, p);

          return parser_.post ();
        }

        // ErrorHandler
        //

        template <typename X, typename C>
        X document<X, C>::
        parse (const std::basic_string<C>& uri,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (uri, eh, f, p);
          return parser_.post ();
        }


        template <typename X, typename C>
        X document<X, C>::
        parse (const C* uri,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (std::basic_string<C> (uri), eh, f, p);
          return parser_.post ();
        }

        // SAX2XMLReader
        //

        template <typename X, typename C>
        X document<X, C>::
        parse (const std::basic_string<C>& uri,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (uri, sax, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (const C* uri,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (std::basic_string<C> (uri), sax, f, p);
          return parser_.post ();
        }


        // parse (istream)
        //

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, eh, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, eh, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sax, f, p);
          return parser_.post ();
        }


        // parse (istream, system_id)
        //


        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, eh, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, eh, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, sax, f, p);
          return parser_.post ();
        }


        // parse (istream, system_id, public_id)
        //

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               const std::basic_string<C>& pub_id,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, pub_id, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               const std::basic_string<C>& pub_id,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, pub_id, eh, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               const std::basic_string<C>& pub_id,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, pub_id, eh, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               const std::basic_string<C>& pub_id,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, pub_id, sax, f, p);
          return parser_.post ();
        }


        // parse (InputSource)
        //


        template <typename X, typename C>
        X document<X, C>::
        parse (const xercesc::InputSource& is,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (const xercesc::InputSource& is,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, eh, f, p);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (const xercesc::InputSource& is,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, eh, f, p);
          return parser_.post ();
        }


        template <typename X, typename C>
        X document<X, C>::
        parse (const xercesc::InputSource& is,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sax, f, p);
          return parser_.post ();
        }


        // document<void>
        //

        template <typename C>
        document<void, C>::
        document (parser<void, C>& p, const std::basic_string<C>& name)
            : document_common<C> (p, std::basic_string<C> (), name),
              parser_ (p)
        {
        }

        template <typename C>
        document<void, C>::
        document (parser<void, C>& p,
                  const std::basic_string<C>& ns,
                  const std::basic_string<C>& name)
            : document_common<C> (p, ns, name), parser_ (p)
        {
        }


        // parse (uri)
        //

        template <typename C>
        void document<void, C>::
        parse (const std::basic_string<C>& uri,
               flags f,
               const properties<C>& p)
        {
          parse_ (uri, f, p);
          parser_.post ();
        }


        template <typename C>
        void document<void, C>::
        parse (const C* uri,
               flags f,
               const properties<C>& p)
        {
          parse_ (std::basic_string<C> (uri), f, p);
          parser_.post ();
        }

        // error_handler
        //

        template <typename C>
        void document<void, C>::
        parse (const std::basic_string<C>& uri,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (uri, eh, f, p);
          parser_.post ();
        }


        template <typename C>
        void document<void, C>::
        parse (const C* uri,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (std::basic_string<C> (uri), eh, f, p);
          parser_.post ();
        }

        // ErrorHandler
        //

        template <typename C>
        void document<void, C>::
        parse (const std::basic_string<C>& uri,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (uri, eh, f, p);
          parser_.post ();
        }


        template <typename C>
        void document<void, C>::
        parse (const C* uri,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (std::basic_string<C> (uri), eh, f, p);
          parser_.post ();
        }

        // SAX2XMLReader
        //

        template <typename C>
        void document<void, C>::
        parse (const std::basic_string<C>& uri,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (uri, sax, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (const C* uri,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (std::basic_string<C> (uri), sax, f, p);
          parser_.post ();
        }


        // parse (istream)
        //

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, eh, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, eh, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sax, f, p);
          parser_.post ();
        }


        // parse (istream, system_id)
        //


        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, eh, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, eh, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, sax, f, p);
          parser_.post ();
        }


        // parse (istream, system_id, public_id)
        //

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               const std::basic_string<C>& pub_id,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, pub_id, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               const std::basic_string<C>& pub_id,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, pub_id, eh, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               const std::basic_string<C>& pub_id,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, pub_id, eh, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& sys_id,
               const std::basic_string<C>& pub_id,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sys_id, pub_id, sax, f, p);
          parser_.post ();
        }


        // parse (InputSource)
        //


        template <typename C>
        void document<void, C>::
        parse (const xercesc::InputSource& is,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (const xercesc::InputSource& is,
               xml::error_handler<C>& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, eh, f, p);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (const xercesc::InputSource& is,
               xercesc::ErrorHandler& eh,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, eh, f, p);
          parser_.post ();
        }


        template <typename C>
        void document<void, C>::
        parse (const xercesc::InputSource& is,
               xercesc::SAX2XMLReader& sax,
               flags f,
               const properties<C>& p)
        {
          parse_ (is, sax, f, p);
          parser_.post ();
        }


        // event_router
        //
        template <typename C>
        event_router<C>::
        event_router (event_consumer<C>& consumer)
            : consumer_ (consumer)
        {
        }

        template <typename C>
        void event_router<C>::
        startElement(const XMLCh* const uri,
                     const XMLCh* const lname,
                     const XMLCh* const /*qname*/,
                     const xercesc::Attributes& attributes)
        {
          typedef std::basic_string<C> string;

          {
            string ns (xml::transcode<C> (uri));
            string name (xml::transcode<C> (lname));

            // Without this explicit construction IBM XL C++ complains
            // about ro_string's copy ctor being private even though the
            // temporary has been eliminated. Note that we cannot
            // eliminate ns and name since ro_string does not make a copy.
            //
            ro_string<C> ro_ns (ns);
            ro_string<C> ro_name (name);

            consumer_.start_element (ro_ns, ro_name);
          }


          for (unsigned long i (0); i < attributes.getLength(); ++i)
          {
            string ns (xml::transcode<C> (attributes.getURI (i)));
            string name (xml::transcode<C> (attributes.getLocalName (i)));
            string value (xml::transcode<C> (attributes.getValue (i)));

            // Without this explicit construction IBM XL C++ complains
            // about ro_string's copy ctor being private even though the
            // temporary has been eliminated. Note that we cannot
            // eliminate ns, name and value since ro_string does not make
            // a copy.
            //
            ro_string<C> ro_ns (ns);
            ro_string<C> ro_name (name);
            ro_string<C> ro_value (value);

            consumer_.attribute (ro_ns, ro_name, ro_value);
          }
        }

        template <typename C>
        void event_router<C>::
        endElement(const XMLCh* const uri,
                   const XMLCh* const lname,
                   const XMLCh* const /*qname*/)
        {
          typedef std::basic_string<C> string;

          string ns (xml::transcode<C> (uri));
          string name (xml::transcode<C> (lname));

          // Without this explicit construction IBM XL C++ complains
          // about ro_string's copy ctor being private even though the
          // temporary has been eliminated. Note that we cannot
          // eliminate ns and name since ro_string does not make a copy.
          //
          ro_string<C> ro_ns (ns);
          ro_string<C> ro_name (name);

          consumer_.end_element (ro_ns, ro_name);
        }

        template <typename C>
        void event_router<C>::
        characters (const XMLCh* const s, const unsigned int n)
        {
          typedef std::basic_string<C> string;

          if (n != 0)
          {
            string str (xml::transcode<C> (s, n));

            // Without this explicit construction IBM XL C++ complains
            // about ro_string's copy ctor being private even though the
            // temporary has been eliminated. Note that we cannot
            // eliminate str since ro_string does not make a copy.
            //
            ro_string<C> ro_str (str);

            consumer_.characters (ro_str);
          }
        }
      }
    }
  }
}
