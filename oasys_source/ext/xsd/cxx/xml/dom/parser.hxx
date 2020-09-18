// file      : xsd/cxx/xml/dom/parser.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_XML_DOM_PARSER_HXX
#define XSD_CXX_XML_DOM_PARSER_HXX

#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>

namespace xsd
{
  namespace cxx
  {
    namespace xml
    {
      namespace dom
      {
        template <typename C>
        class parser
        {
        public:
          parser (const xercesc::DOMElement& e);

          bool
          more_elements () const
          {
            return next_element_ != 0;
          }

          const xercesc::DOMElement&
          next_element ();

          bool
          more_attributes () const
          {
            return a_->getLength () > ai_;
          }

          const xercesc::DOMAttr&
          next_attribute ();

        private:
          void
          find_next_element ();

        private:
          parser (const parser&);

          parser&
          operator= (const parser&);

        private:
          const xercesc::DOMNode* next_element_;

          const xercesc::DOMNamedNodeMap* a_;
          unsigned long ai_; // Index of the next DOMAttr.
        };
      }
    }
  }
}

#include <xsd/cxx/xml/dom/parser.txx>

#endif // XSD_CXX_XML_DOM_PARSER_HXX
