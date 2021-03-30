// file      : xsd/cxx/xml/dom/elements.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <xercesc/util/XMLUniDefs.hpp> // chLatin_L, etc

#include <xercesc/dom/DOMBuilder.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>

#include <xsd/cxx/xml/string.hxx>
#include <xsd/cxx/xml/bits/literals.hxx>
#include <xsd/cxx/xml/dom/bits/error-handler-proxy.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace xml
    {
      namespace dom
      {
        // element
        //

        template <typename C>
        element<C>::
        element (const xercesc::DOMElement& e)
            : e_ (0), ce_ (&e)
        {
          const XMLCh* n (e.getLocalName ());

          // If this DOM does not support namespaces then use getTagName.
          //
          if (n == 0)
            n = e.getTagName ();
          else
            namespace__ = transcode<C> (e.getNamespaceURI ());

          name_ = transcode<C> (n);
        }

        template <typename C>
        element<C>::
        element (xercesc::DOMElement& e)
            : e_ (&e), ce_ (&e)
        {
          const XMLCh* n (e.getLocalName ());

          // If this DOM does not support namespaces then use getTagName.
          //
          if (n == 0)
            n = e.getTagName ();
          else
            namespace__ = transcode<C> (e.getNamespaceURI ());

          name_ = transcode<C> (n);
        }

        template <typename C>
        element<C>::
        element (const string_type& name, xercesc::DOMElement& parent)
            : e_ (0),
              ce_ (0),
              name_ (name)
        {
          xercesc::DOMDocument* doc (parent.getOwnerDocument ());

          e_ = doc->createElement (string (name).c_str ());

          parent.appendChild (e_);

          ce_ = e_;
        }

        template <typename C>
        element<C>::
        element (const string_type& name,
                 const string_type& ns,
                 xercesc::DOMElement& parent)
            : e_ (0),
              ce_ (0),
              name_ (name),
              namespace__ (ns)
        {
          xercesc::DOMDocument* doc (parent.getOwnerDocument ());

          if (!ns.empty ())
          {
            string_type p (prefix (ns, parent));

            e_ = doc->createElementNS (
              string (ns).c_str (),
              string (p.empty ()
                      ? name
                      : p + string_type (1, ':') + name).c_str ());
          }
          else
            e_ = doc->createElement (string (name).c_str ());

          parent.appendChild (e_);

          ce_ = e_;
        }

        template <typename C>
        typename element<C>::string_type element<C>::
        operator[] (const string_type& name) const
        {
          const XMLCh* value (ce_->getAttribute (string (name).c_str ()));
          return transcode<C> (value);
        }

        template <typename C>
        typename element<C>::string_type element<C>::
        attribute (const string_type& name, const string_type& ns) const
        {
          const XMLCh* value (
            ce_->getAttributeNS (string (ns).c_str (),
                                 string (name).c_str ()));
          return transcode<C> (value);
        }


        // attribute
        //

        template <typename C>
        attribute<C>::
        attribute (const xercesc::DOMAttr& a)
            : a_ (0),
              ca_ (&a),
              value_ (transcode<C> (a.getValue ()))
        {
          const XMLCh* n (a.getLocalName ());

          // If this DOM does not support namespaces then use getName.
          //
          if (n == 0)
            n = a.getName ();
          else
            namespace__ = transcode<C> (a.getNamespaceURI ());

          name_ = transcode<C> (n);
        }

        template <typename C>
        attribute<C>::
        attribute (xercesc::DOMAttr& a)
            : a_ (&a),
              ca_ (&a),
              value_ (transcode<C> (a.getValue ()))
        {
          const XMLCh* n (a.getLocalName ());

          // If this DOM does not support namespaces then use getName.
          //
          if (n == 0)
            n = a.getName ();
          else
            namespace__ = transcode<C> (a.getNamespaceURI ());

          name_ = transcode<C> (n);
        }

        template <typename C>
        attribute<C>::
        attribute (const string_type& name,
                   xercesc::DOMElement& parent,
                   const string_type& v)
            : a_ (0),
              ca_ (0),
              name_ (name),
              value_ ()
        {
          xercesc::DOMDocument* doc (parent.getOwnerDocument ());

          a_ = doc->createAttribute (string (name).c_str ());

          if (!v.empty ())
            value (v);

          parent.setAttributeNode (a_);

          ca_ = a_;
        }

        template <typename C>
        attribute<C>::
        attribute (const string_type& name,
                   const string_type& ns,
                   xercesc::DOMElement& parent,
                   const string_type& v)
            : a_ (0),
              ca_ (0),
              name_ (name),
              namespace__ (ns),
              value_ ()
        {
          xercesc::DOMDocument* doc (parent.getOwnerDocument ());

          if (!ns.empty ())
          {
            string_type p (prefix (ns, parent));

            a_ = doc->createAttributeNS (
              string (ns).c_str (),
              string (p.empty ()
                      ? name
                      : p + string_type (1, ':') + name).c_str ());
          }
          else
            a_ = doc->createAttribute (string (name).c_str ());

          if (!v.empty ())
            value (v);

          parent.setAttributeNodeNS (a_);

          ca_ = a_;
        }

        template <typename C>
        const xercesc::DOMElement& attribute<C>::
        element () const
        {
          return *ca_->getOwnerElement ();
        }

        template <typename C>
        xercesc::DOMElement& attribute<C>::
        element ()
        {
          return *a_->getOwnerElement ();
        }


        //
        //

        template <typename C>
        std::basic_string<C>
        ns_name (const element<C>& e, const std::basic_string<C>& n)
        {
          std::basic_string<C> p (prefix (n));

          // 'xml' prefix requires special handling and Xerces folks refuse
          // to handle this in DOM so I have to do it myself.
          //
          if (p == xml::bits::xml_prefix<C> ())
            return xml::bits::xml_namespace<C> ();

          const XMLCh* xns (e.dom_element ()->lookupNamespaceURI (
                              p.empty () ? 0 : string (p).c_str ()));

          if (xns == 0)
            throw no_mapping ();

          return transcode<C> (xns);
        }

        template <typename C>
        std::basic_string<C>
        fq_name (const element<C>& e, const std::basic_string<C>& n)
        {
          std::basic_string<C> ns (ns_name (e, n));
          std::basic_string<C> un (uq_name (n));

          return ns.empty () ? un : (ns + C ('#') + un);
        }

        template <typename C>
        std::basic_string<C>
        prefix (const std::basic_string<C>& ns, const xercesc::DOMElement& e)
        {
          string xns (ns);

          const XMLCh* p (e.lookupNamespacePrefix (xns.c_str (), false));

          if (p == 0)
          {
            if (e.isDefaultNamespace (xns.c_str ()))
            {
              return std::basic_string<C> ();
            }
            else
            {
              // 'xml' prefix requires special handling and Xerces folks
              // refuse to handle this in DOM so I have to do it myself.
              //
              if (ns == xml::bits::xml_namespace<C> ())
                return xml::bits::xml_prefix<C> ();

              throw no_prefix ();
            }
          }

          return transcode<C> (p);
        }


        //
        //

        template <typename C>
        xml::dom::auto_ptr<xercesc::DOMDocument>
        parse (const xercesc::DOMInputSource& is,
               error_handler<C>& eh,
               const properties<C>& prop,
               unsigned long flags)
        {
          bits::error_handler_proxy<C> ehp (eh);
          return xml::dom::parse (is, ehp, prop, flags);
        }

        template <typename C>
        auto_ptr<xercesc::DOMDocument>
        parse (const xercesc::DOMInputSource& is,
               xercesc::DOMErrorHandler& eh,
               const properties<C>& prop,
               unsigned long flags)
        {
          // HP aCC cannot handle using namespace xercesc;
          //
          using xercesc::DOMImplementationRegistry;
          using xercesc::DOMImplementationLS;
          using xercesc::DOMImplementation;
          using xercesc::DOMDocument;
          using xercesc::DOMBuilder;
          using xercesc::XMLUni;


          // Instantiate the DOM parser.
          //
          const XMLCh ls_id[] = {xercesc::chLatin_L,
                                 xercesc::chLatin_S,
                                 xercesc::chNull};

          // Get an implementation of the Load-Store (LS) interface.
          //
          DOMImplementation* impl (
            DOMImplementationRegistry::getDOMImplementation (ls_id));

          // Create a DOMBuilder.
          //
          auto_ptr<DOMBuilder> parser (
            impl->createDOMBuilder(DOMImplementationLS::MODE_SYNCHRONOUS, 0));

          // Discard comment nodes in the document.
          //
          parser->setFeature (XMLUni::fgDOMComments, false);

          // Enable datatype normalization.
          //
          parser->setFeature (XMLUni::fgDOMDatatypeNormalization, true);

          // Do not create EntityReference nodes in the DOM tree. No
          // EntityReference nodes will be created, only the nodes
          // corresponding to their fully expanded substitution text will be
          // created.
          //
          parser->setFeature (XMLUni::fgDOMEntities, false);

          // Perform Namespace processing.
          //
          parser->setFeature (XMLUni::fgDOMNamespaces, true);

          // Do not include ignorable whitespace in the DOM tree.
          //
          parser->setFeature (XMLUni::fgDOMWhitespaceInElementContent, false);

          if (flags & dont_validate)
          {
            parser->setFeature (XMLUni::fgDOMValidation, false);
            parser->setFeature (XMLUni::fgXercesSchema, false);
            parser->setFeature (XMLUni::fgXercesSchemaFullChecking, false);
          }
          else
          {
            parser->setFeature (XMLUni::fgDOMValidation, true);
            parser->setFeature (XMLUni::fgXercesSchema, true);

            // This feature checks the schema grammar for additional
            // errors. We most likely do not need it when validating
            // instances (assuming the schema is valid).
            //
            parser->setFeature (XMLUni::fgXercesSchemaFullChecking, false);
          }

          // We will release DOM ourselves.
          //
          parser->setFeature (XMLUni::fgXercesUserAdoptsDOMDocument, true);


          // Transfer properies if any.
          //

          if (!prop.schema_location ().empty ())
          {
            xml::string sl (prop.schema_location ());
            const void* v (sl.c_str ());

            parser->setProperty (
              XMLUni::fgXercesSchemaExternalSchemaLocation,
              const_cast<void*> (v));
          }

          if (!prop.no_namespace_schema_location ().empty ())
          {
            xml::string sl (prop.no_namespace_schema_location ());
            const void* v (sl.c_str ());

            parser->setProperty (
              XMLUni::fgXercesSchemaExternalNoNameSpaceSchemaLocation,
              const_cast<void*> (v));
          }

          // Set error handler.
          //
          bits::error_handler_proxy<C> ehp (eh);
          parser->setErrorHandler (&ehp);

          auto_ptr<DOMDocument> doc (parser->parse (is));

          if (ehp.failed ())
            doc.reset ();

          return doc;
        }

        template <typename C>
        xml::dom::auto_ptr<xercesc::DOMDocument>
        parse (const std::basic_string<C>& uri,
               error_handler<C>& eh,
               const properties<C>& prop,
               unsigned long flags)
        {
          bits::error_handler_proxy<C> ehp (eh);
          return xml::dom::parse (uri, ehp, prop, flags);
        }

        template <typename C>
        auto_ptr<xercesc::DOMDocument>
        parse (const std::basic_string<C>& uri,
               xercesc::DOMErrorHandler& eh,
               const properties<C>& prop,
               unsigned long flags)
        {
          // HP aCC cannot handle using namespace xercesc;
          //
          using xercesc::DOMImplementationRegistry;
          using xercesc::DOMImplementationLS;
          using xercesc::DOMImplementation;
          using xercesc::DOMDocument;
          using xercesc::DOMBuilder;
          using xercesc::XMLUni;


          // Instantiate the DOM parser.
          //
          const XMLCh ls_id[] = {xercesc::chLatin_L,
                                 xercesc::chLatin_S,
                                 xercesc::chNull};

          // Get an implementation of the Load-Store (LS) interface.
          //
          DOMImplementation* impl (
            DOMImplementationRegistry::getDOMImplementation (ls_id));

          // Create a DOMBuilder.
          //
          auto_ptr<DOMBuilder> parser (
            impl->createDOMBuilder(DOMImplementationLS::MODE_SYNCHRONOUS, 0));

          // Discard comment nodes in the document.
          //
          parser->setFeature (XMLUni::fgDOMComments, false);

          // Disable datatype normalization. The XML 1.0 attribute value
          // normalization always occurs though.
          //
          parser->setFeature (XMLUni::fgDOMDatatypeNormalization, true);

          // Do not create EntityReference nodes in the DOM tree. No
          // EntityReference nodes will be created, only the nodes
          // corresponding to their fully expanded substitution text will be
          // created.
          //
          parser->setFeature (XMLUni::fgDOMEntities, false);

          // Perform Namespace processing.
          //
          parser->setFeature (XMLUni::fgDOMNamespaces, true);

          // Do not include ignorable whitespace in the DOM tree.
          //
          parser->setFeature (XMLUni::fgDOMWhitespaceInElementContent, false);

          if (flags & dont_validate)
          {
            parser->setFeature (XMLUni::fgDOMValidation, false);
            parser->setFeature (XMLUni::fgXercesSchema, false);
            parser->setFeature (XMLUni::fgXercesSchemaFullChecking, false);
          }
          else
          {
            parser->setFeature (XMLUni::fgDOMValidation, true);
            parser->setFeature (XMLUni::fgXercesSchema, true);

            // This feature checks the schema grammar for additional
            // errors. We most likely do not need it when validating
            // instances (assuming the schema is valid).
            //
            parser->setFeature (XMLUni::fgXercesSchemaFullChecking, false);
          }

          // We will release DOM ourselves.
          //
          parser->setFeature (XMLUni::fgXercesUserAdoptsDOMDocument, true);

          // Transfer properies if any.
          //

          if (!prop.schema_location ().empty ())
          {
            xml::string sl (prop.schema_location ());
            const void* v (sl.c_str ());

            parser->setProperty (
              XMLUni::fgXercesSchemaExternalSchemaLocation,
              const_cast<void*> (v));
          }

          if (!prop.no_namespace_schema_location ().empty ())
          {
            xml::string sl (prop.no_namespace_schema_location ());
            const void* v (sl.c_str ());

            parser->setProperty (
              XMLUni::fgXercesSchemaExternalNoNameSpaceSchemaLocation,
              const_cast<void*> (v));
          }

          // Set error handler.
          //
          bits::error_handler_proxy<C> ehp (eh);
          parser->setErrorHandler (&ehp);

          auto_ptr<DOMDocument> doc (
            parser->parseURI (string (uri).c_str ()));

          if (ehp.failed ())
            doc.reset ();

          return doc;
        }
      }
    }
  }
}
