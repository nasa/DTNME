// file      : xsd/cxx/xml/dom/serialization.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_XML_DOM_SERIALIZATION_HXX
#define XSD_CXX_XML_DOM_SERIALIZATION_HXX

#include <string>
#include <ostream>
#include <sstream>

#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#include <xercesc/framework/XMLFormatter.hpp> // XMLFormatTarget, XMLFormatter

#include <xsd/cxx/xml/error-handler.hxx>
#include <xsd/cxx/xml/dom/auto-ptr.hxx>
#include <xsd/cxx/xml/dom/namespace-infomap.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace xml
    {
      namespace dom
      {
        // No mapping provided for a namespace.
        //
        template <typename C>
        struct mapping
        {
          mapping (const std::basic_string<C>& name)
              : name_ (name)
          {
          }

          const std::basic_string<C>&
          name () const
          {
            return name_;
          }

        private:
          std::basic_string<C> name_;
        };

        // Serialization flags.
        //
        const unsigned long no_xml_declaration = 0x00010000UL;


        // 'xsi' prefix is already in use and no user-defined mapping has
        //  been provided.
        //
        struct xsi_already_in_use {};


        template <typename C>
        xml::dom::auto_ptr<xercesc::DOMDocument>
        serialize (const std::basic_string<C>& root_element,
                   const std::basic_string<C>& root_element_namespace,
                   const namespace_infomap<C>& map,
                   unsigned long flags);

        // This one helps Sun C++ to overcome its fears.
        //
        template <typename C>
        inline xml::dom::auto_ptr<xercesc::DOMDocument>
        serialize (const C* root_element,
                   const C* root_element_namespace,
                   const namespace_infomap<C>& map,
                   unsigned long flags)
        {
          return serialize (std::basic_string<C> (root_element),
                            std::basic_string<C> (root_element_namespace),
                            map,
                            flags);
        }

        //
        //
        template <typename C>
        bool
        serialize (xercesc::XMLFormatTarget& target,
                   const xercesc::DOMDocument& doc,
                   const std::basic_string<C>& enconding,
                   error_handler<C>& eh,
                   unsigned long flags);

        template <typename C>
        bool
        serialize (xercesc::XMLFormatTarget& target,
                   const xercesc::DOMDocument& doc,
                   const std::basic_string<C>& enconding,
                   xercesc::DOMErrorHandler& eh,
                   unsigned long flags);

        //
        //
        class ostream_format_target: public xercesc::XMLFormatTarget
        {
        public:
          ostream_format_target (std::ostream& os)
              : os_ (os)
          {
          }


        public:
          // I know, some of those consts are stupid. But that's what
          // Xerces folks put into their interfaces and VC-7.1 thinks
          // there are different signatures if one strips this fluff off.
          //
          virtual void
          writeChars (const XMLByte* const buf,
                      const unsigned int size,
                      xercesc::XMLFormatter* const)
          {
            os_.write (reinterpret_cast<const char*> (buf),
                       static_cast<std::streamsize> (size));
          }


          virtual void
          flush ()
          {
            os_.flush ();
          }

        private:
          std::ostream& os_;
        };
      }
    }
  }
}

#include <xsd/cxx/xml/dom/serialization.txx>

#endif  // XSD_CXX_XML_DOM_SERIALIZATION_HXX
