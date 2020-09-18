// file      : xsd/cxx/xml/dom/parser.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <cassert>

namespace xsd
{
  namespace cxx
  {
    namespace xml
    {
      namespace dom
      {
        template <typename C>
        parser<C>::
        parser (const xercesc::DOMElement& e)
            : next_element_ (e.getFirstChild ()),
              a_ (e.getAttributes ()), ai_ (0)
        {
          find_next_element ();
        }

        template <typename C>
        const xercesc::DOMElement& parser<C>::
        next_element ()
        {
          using xercesc::DOMNode;

          const DOMNode* n (next_element_);
          assert (n->getNodeType () == DOMNode::ELEMENT_NODE);

          next_element_ = next_element_->getNextSibling ();
          find_next_element ();

          return *static_cast<const xercesc::DOMElement*> (n);
        }

        template <typename C>
        const xercesc::DOMAttr& parser<C>::
        next_attribute ()
        {
          using xercesc::DOMNode;

          const DOMNode* n (a_->item (ai_++));
          assert (n->getNodeType () == DOMNode::ATTRIBUTE_NODE);

          return *static_cast<const xercesc::DOMAttr*> (n);
        }

        template <typename C>
        void parser<C>::
        find_next_element ()
        {
          using xercesc::DOMNode;

          for (; next_element_ != 0 &&
                 next_element_->getNodeType () != DOMNode::ELEMENT_NODE;
               next_element_ = next_element_->getNextSibling ())
	       ;
        }
      }
    }
  }
}

