// file      : xsd/cxx/xml/dom/elements.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_XML_DOM_ELEMENTS_HXX
#define XSD_CXX_XML_DOM_ELEMENTS_HXX

#include <string>

#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMInputSource.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>

#include <xsd/cxx/xml/elements.hxx>
#include <xsd/cxx/xml/error-handler.hxx>

#include <xsd/cxx/xml/dom/auto-ptr.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace xml
    {
      namespace dom
      {
        template <typename C>
        class element;

        template <typename C>
        std::basic_string<C>
        prefix (const std::basic_string<C>& ns, const element<C>& e);

        using xml::prefix;


        //
        //
        template <typename C>
        class element
        {
          typedef std::basic_string<C> string_type;

        public:
          element (const xercesc::DOMElement&);

          element (xercesc::DOMElement&);

          element (const string_type& name,
                   xercesc::DOMElement& parent);

          element (const string_type& name,
                   const string_type& ns,
                   xercesc::DOMElement& parent);

        public:
          const string_type&
          name () const
          {
            return name_;
          }

          const string_type&
          namespace_ () const
          {
            return namespace__;
          }

        public:
          string_type
          operator[] (const string_type& name) const;

          string_type
          attribute (const string_type& name, const string_type& ns) const;

        public:
          const xercesc::DOMElement&
          dom_element () const
          {
            return *ce_;
          }

          xercesc::DOMElement&
          dom_element ()
          {
            return *e_;
          }

        private:
          xercesc::DOMElement* e_;
          const xercesc::DOMElement* ce_;

          string_type name_;
          string_type namespace__;
        };


        //
        //
        template <typename C>
        class attribute
        {
          typedef std::basic_string<C> string_type;

        public:
          attribute (const xercesc::DOMAttr&);

          attribute (xercesc::DOMAttr&);

          attribute (const string_type& name,
                     xercesc::DOMElement& parent,
                     const string_type& value = string_type ());

          attribute (const string_type& name,
                     const string_type& ns,
                     xercesc::DOMElement& parent,
                     const string_type& value = string_type ());

        public:
          const string_type&
          name () const
          {
            return name_;
          }

          const string_type&
          namespace_ () const
          {
            return namespace__;
          }

          const string_type&
          value () const
          {
            return value_;
          }

          void
          value (const string_type& v)
          {
            value_ = v;
            a_->setValue (string (v).c_str ());
          }

          const xercesc::DOMElement&
          element () const;

          xercesc::DOMElement&
          element ();

        public:
          const xercesc::DOMAttr&
          dom_attribute () const
          {
            return *ca_;
          }

          xercesc::DOMAttr&
          dom_attribute ()
          {
            return *a_;
          }

        private:
          xercesc::DOMAttr* a_;
          const xercesc::DOMAttr* ca_;

          string_type name_;
          string_type namespace__;
          string_type value_;
        };


        //
        //

        struct no_mapping {};

        template <typename C>
        std::basic_string<C>
        ns_name (const element<C>& e, const std::basic_string<C>& name);

        template <typename C>
        std::basic_string<C>
        fq_name (const element<C>& e, const std::basic_string<C>& name);

        class no_prefix {};

        template <typename C>
        std::basic_string<C>
        prefix (const std::basic_string<C>& ns, const xercesc::DOMElement& e);


        // Parsing flags.
        //
        const unsigned long dont_validate = 0x00000200UL;

        template <typename C>
        xml::dom::auto_ptr<xercesc::DOMDocument>
        parse (const xercesc::DOMInputSource&,
               error_handler<C>&,
               const properties<C>&,
               unsigned long flags);

        template <typename C>
        xml::dom::auto_ptr<xercesc::DOMDocument>
        parse (const xercesc::DOMInputSource&,
               xercesc::DOMErrorHandler&,
               const properties<C>&,
               unsigned long flags);

        template <typename C>
        xml::dom::auto_ptr<xercesc::DOMDocument>
        parse (const std::basic_string<C>& uri,
               error_handler<C>&,
               const properties<C>&,
               unsigned long flags);

        template <typename C>
        xml::dom::auto_ptr<xercesc::DOMDocument>
        parse (const std::basic_string<C>& uri,
               xercesc::DOMErrorHandler&,
               const properties<C>&,
               unsigned long flags);
      }
    }
  }
}

#include <xsd/cxx/xml/dom/elements.txx>

#endif // XSD_CXX_XML_DOM_ELEMENTS_HXX
