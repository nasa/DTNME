// file      : xsd/cxx/xml/dom/serialization.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <xercesc/dom/DOMWriter.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>

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
        template <typename C>
        auto_ptr<xercesc::DOMDocument>
        serialize (const std::basic_string<C>& el,
                   const std::basic_string<C>& ns,
                   const namespace_infomap<C>& map,
                   unsigned long)
        {
          // HP aCC cannot handle using namespace xercesc;
          //
          using xercesc::DOMImplementationRegistry;
          using xercesc::DOMImplementation;
          using xercesc::DOMDocument;
          using xercesc::DOMElement;

          //
          //
          typedef std::basic_string<C> string;
          typedef namespace_infomap<C> infomap;
          typedef typename infomap::const_iterator infomap_iterator;

          C colon (':'), space (' ');

          string prefix;

          if (!ns.empty ())
          {
            infomap_iterator i (map.begin ()), e (map.end ());

            for ( ;i != e; ++i)
            {
              if (i->second.name == ns)
              {
                prefix = i->first;
                break;
              }
            }

            if (i == e)
              throw mapping<C> (ns);
          }


          DOMImplementation* impl (
            DOMImplementationRegistry::getDOMImplementation (
              xml::string (xml::bits::load_store<C> ()).c_str ()));

          auto_ptr<DOMDocument> doc (
            impl->createDocument (
              (ns.empty () ? 0 : xml::string (ns).c_str ()),
              xml::string ((prefix.empty ()
                            ? el
                            : prefix + colon + el)).c_str (),
              0));

          DOMElement* root (doc->getDocumentElement ());


          // Check if we need to provide xsi mapping.
          //
          bool xsi (false);
          string xsi_prefix (xml::bits::xsi_prefix<C> ());
          string xmlns_prefix (xml::bits::xmlns_prefix<C> ());

          for (infomap_iterator i (map.begin ()), e (map.end ()); i != e; ++i)
          {
            if (!i->second.schema.empty ())
            {
              xsi = true;
              break;
            }
          }

          // Check if we were told to provide xsi mapping.
          //
          if (xsi)
          {
            for (infomap_iterator i (map.begin ()), e (map.end ());
                 i != e;
                 ++i)
            {
              if (i->second.name == xml::bits::xsi_namespace<C> ())
              {
                xsi_prefix = i->first;
                xsi = false;
                break;
              }
            }

            if (xsi)
            {
              // If we were not told to provide xsi mapping, make sure our
              // prefix does not conflict with user-defined prefixes.
              //
              infomap_iterator i (map.find (xsi_prefix));

              if (i != map.end ())
                throw xsi_already_in_use ();
            }
          }

          // Create xmlns:xsi attribute.
          //
          if (xsi)
          {
            root->setAttributeNS (
              xml::string (xml::bits::xmlns_namespace<C> ()).c_str (),
              xml::string (xmlns_prefix + colon + xsi_prefix).c_str (),
              xml::string (xml::bits::xsi_namespace<C> ()).c_str ());
          }


          // Create user-defined mappings.
          //
          for (infomap_iterator i (map.begin ()), e (map.end ()); i != e; ++i)
          {
            if (i->first.empty ())
            {
              // Empty prefix.
              //
              if (!i->second.name.empty ())
                root->setAttributeNS (
                  xml::string (xml::bits::xmlns_namespace<C> ()).c_str (),
                  xml::string (xmlns_prefix).c_str (),
                  xml::string (i->second.name).c_str ());
            }
            else
            {
              root->setAttributeNS (
                xml::string (xml::bits::xmlns_namespace<C> ()).c_str (),
                xml::string (xmlns_prefix + colon + i->first).c_str (),
                xml::string (i->second.name).c_str ());
            }
          }

          // Create xsi:schemaLocation and xsi:noNamespaceSchemaLocation
          // attributes.
          //
          string schema_location;
          string no_namespace_schema_location;

          for (infomap_iterator i (map.begin ()), e (map.end ()); i != e; ++i)
          {
            if (!i->second.schema.empty ())
            {
              if (i->second.name.empty ())
              {
                if (!no_namespace_schema_location.empty ())
                  no_namespace_schema_location += space;

                no_namespace_schema_location += i->second.schema;
              }
              else
              {
                if (!schema_location.empty ())
                  schema_location += space;

                schema_location += i->second.name + space + i->second.schema;
              }
            }
          }

          if (!schema_location.empty ())
          {
            root->setAttributeNS (
              xml::string (xml::bits::xsi_namespace<C> ()).c_str (),
              xml::string (xsi_prefix + colon +
                           xml::bits::schema_location<C> ()).c_str (),
              xml::string (schema_location).c_str ());
          }

          if (!no_namespace_schema_location.empty ())
          {
            root->setAttributeNS (
              xml::string (xml::bits::xsi_namespace<C> ()).c_str (),
              xml::string (
                xsi_prefix + colon +
                xml::bits::no_namespace_schema_location<C> ()).c_str (),
              xml::string (no_namespace_schema_location).c_str ());
          }

          return doc;
        }


        template <typename C>
        bool
        serialize (xercesc::XMLFormatTarget& target,
                   const xercesc::DOMDocument& doc,
                   const std::basic_string<C>& encoding,
                   xercesc::DOMErrorHandler& eh,
                   unsigned long flags)
        {
          // HP aCC cannot handle using namespace xercesc;
          //
          using xercesc::DOMImplementationRegistry;
          using xercesc::DOMImplementation;
          using xercesc::DOMWriter;
          using xercesc::XMLUni;

          DOMImplementation* impl (
            DOMImplementationRegistry::getDOMImplementation (
              xml::string (xml::bits::load_store<C> ()).c_str ()));


          // Get an instance of DOMWriter.
          //
          bits::error_handler_proxy<C> ehp (eh);

          xml::dom::auto_ptr<DOMWriter> writer (impl->createDOMWriter ());

          writer->setErrorHandler (&ehp);
          writer->setEncoding(xml::string (encoding).c_str ());

          // Set some nice features if the serializer supports them.
          //
          if (writer->canSetFeature (
                XMLUni::fgDOMWRTDiscardDefaultContent, true))
            writer->setFeature (XMLUni::fgDOMWRTDiscardDefaultContent, true);

          if (writer->canSetFeature (XMLUni::fgDOMWRTFormatPrettyPrint, true))
            writer->setFeature (XMLUni::fgDOMWRTFormatPrettyPrint, true);

          // See if we need to write XML declaration.
          //
          if ((flags & no_xml_declaration) &&
              writer->canSetFeature (XMLUni::fgDOMXMLDeclaration, false))
            writer->setFeature (XMLUni::fgDOMXMLDeclaration, false);

          writer->writeNode (&target, doc);

          if (ehp.failed ())
            return false;

          return true;
        }

        template <typename C>
        bool
        serialize (xercesc::XMLFormatTarget& target,
                   const xercesc::DOMDocument& doc,
                   const std::basic_string<C>& enconding,
                   error_handler<C>& eh,
                   unsigned long flags)
        {
          bits::error_handler_proxy<C> ehp (eh);
          return serialize (target, doc, enconding, ehp, flags);
        }
      }
    }
  }
}
