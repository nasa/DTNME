// file      : xsd/cxx/tree/type-serializer-map.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <xsd/cxx/xml/bits/literals.hxx> // xml::bits::{xsi_namespace, type}
#include <xsd/cxx/xml/dom/elements.hxx>  // dom::{element, attribute}
#include <xsd/cxx/xml/dom/serialization.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      // qualified_name
      //
      template <typename C>
      qualified_name<C>::
      qualified_name (std::basic_string<C> name,
                      std::basic_string<C> namespace_)
          : name_ (name), namespace__ (namespace_)
      {
      }


      // type_serializer_map
      //
      template <typename C>
      void type_serializer_map<C>::
      register_type (const type_id& tid,
                     const qualified_name<C>& name,
                     serializer s,
                     bool override)
      {
        if (override || type_map_.find (&tid) == type_map_.end ())
          type_map_[&tid] = type_info (name, s);
      }

      template <typename C>
      void type_serializer_map<C>::
      register_element (const qualified_name<C>& root,
                        const qualified_name<C>& subst,
                        const type_id& tid,
                        serializer s)
      {
        element_map_[root][&tid] = type_info (subst, s);
      }

      template <typename C>
      template<typename X>
      void type_serializer_map<C>::
      serialize (const std::basic_string<C>& name, // element name
                 const std::basic_string<C>& ns,   // element namespace
                 bool global,
                 bool qualified,
                 xercesc::DOMElement& parent,
                 const X& x) const
      {
        const type_id& tid (typeid (x));

        // If dynamic type and static type are the same then
        // we are set for serialization.
        //
        if (typeid (X) == tid)
        {
          if (qualified)
          {
            xml::dom::element<C> e (name, ns, parent);
            e.dom_element () << x;
          }
          else
          {
            xml::dom::element<C> e (name, parent);
            e.dom_element () << x;
          }

          return;
        }

        // First see if we can find a substitution.
        //
        if (global)
        {
          qualified_name<C> qname (name, ns);

          typename element_map::const_iterator i (
            element_map_.find (qname));

          if (i != element_map_.end ())
          {
            if (const type_info* ti = find_substitution (i->second, tid))
            {
              xml::dom::element<C> e (ti->name ().name (),
                                      ti->name ().namespace_ (),
                                      parent);

              ti->serializer () (e.dom_element (), x);
              return;
            }
          }
        }

        // The last resort is xsi:type.
        //
        if (const type_info* ti = find_type (tid))
        {
          if (qualified)
          {
            xml::dom::element<C> e (name, ns, parent);
            ti->serializer () (e.dom_element (), x);

            set_xsi_type (e.dom_element (), *ti);
          }
          else
          {
            xml::dom::element<C> e (name, parent);
            ti->serializer () (e.dom_element (), x);

            set_xsi_type (e.dom_element (), *ti);
          }

          return;
        }

        throw no_type_info<C> (std::basic_string<C> (),
                               std::basic_string<C> ()); //@@ TODO
      }

      template <typename C>
      template<typename X>
      void type_serializer_map<C>::
      serialize (const std::basic_string<C>& root_name,
                 const std::basic_string<C>& root_ns,
                 xml::dom::element<C>& e,
                 const X& x) const
      {
        std::basic_string<C> name (e.name ());
        std::basic_string<C> ns (e.namespace_ ());

        const type_id& tid (typeid (x));

        // If dynamic type and static type are the same then
        // we are set for serialization.
        //
        if (typeid (X) == tid)
        {
          if (root_name != name || root_ns != ns)
            throw unexpected_element<C> (name, ns, root_name, root_ns);

          e.dom_element () << x;

          return;
        }

        // First see if this is a substitution.
        //
        if (root_name != name || root_ns != ns)
        {
          qualified_name<C> qname (root_name, root_ns);

          typename element_map::const_iterator i (
            element_map_.find (qname));

          if (i != element_map_.end ())
          {
            if (const type_info* ti = find_substitution (i->second, tid))
            {

              if (ti->name ().name () != name ||
                  ti->name ().namespace_ () != ns)
              {
                throw unexpected_element<C> (
                  name, ns,
                  ti->name ().name (), ti->name ().namespace_ ());
              }

              ti->serializer () (e.dom_element (), x);

              return;
            }
          }

          // This is not a valid substitution.
          //
          throw unexpected_element<C> (name, ns, root_name, root_ns);
        }

        // The last resort is xsi:type.
        //
        if (const type_info* ti = find_type (tid))
        {
          ti->serializer () (e.dom_element (), x);

          set_xsi_type (e.dom_element (), *ti);

          return;
        }

        throw no_type_info<C> (std::basic_string<C> (),
                               std::basic_string<C> ()); //@@ TODO
      }

      template <typename C>
      template<typename X>
      xml::dom::auto_ptr<xercesc::DOMDocument> type_serializer_map<C>::
      serialize (const std::basic_string<C>& name,
                 const std::basic_string<C>& ns,
                 const xml::dom::namespace_infomap<C>& m,
                 const X& x,
                 unsigned long flags) const
      {
        const type_id& tid (typeid (x));

        // If dynamic type and static type are the same then
        // we are set for serialization.
        //
        if (typeid (X) == tid)
        {
          return xml::dom::serialize<C> (name, ns, m, flags);
        }

        // See if we can find a substitution.
        //
        {
          qualified_name<C> qname (name, ns);

          typename element_map::const_iterator i (
            element_map_.find (qname));

          if (i != element_map_.end ())
          {
            if (const type_info* ti = find_substitution (i->second, tid))
            {
              return xml::dom::serialize<C> (
                ti->name ().name (), ti->name ().namespace_ (), m, flags);
            }
          }
        }

        // If there is no substitution then serialize() will have to
        // find suitable xsi:type.
        //
        return xml::dom::serialize<C> (name, ns, m, flags);
      }

      template <typename C>
      const typename type_serializer_map<C>::type_info*
      type_serializer_map<C>::
      find_type (const type_id& tid) const
      {
        typename type_map::const_iterator i (type_map_.find (&tid));

        if (i != type_map_.end ())
          return &i->second;
        else
          return 0;
      }

      template <typename C>
      const typename type_serializer_map<C>::type_info*
      type_serializer_map<C>::
      find_substitution (const subst_map& start, const type_id& tid) const
      {
        typename subst_map::const_iterator i (start.find (&tid));

        if (i != start.end ())
          return &i->second;
        else
        {
          for (i = start.begin (); i != start.end (); ++i)
          {
            typename element_map::const_iterator j (
              element_map_.find (i->second.name ()));

            if (j != element_map_.end ())
            {
              if (const type_info* ti = find_substitution (j->second, tid))
                return ti;
            }
          }
        }

        return 0;
      }

      template <typename C>
      void type_serializer_map<C>::
      set_xsi_type (xercesc::DOMElement& e, const type_info& ti) const
      {
        try
        {
          xml::dom::attribute<C> a (xml::bits::type<C> (),
                                    xml::bits::xsi_namespace<C> (),
                                    e);

          std::basic_string<C> id;

          const std::basic_string<C>& ns (ti.name ().namespace_ ());

          if (!ns.empty ())
          {
            id = bits::resolve_prefix (ns, e);

            if (!id.empty ())
              id += C (':');
          }

          id += ti.name ().name ();

          a.dom_attribute () << id;
        }
        catch (const xml::dom::no_prefix&)
        {
          // No prefix for xsi namespace. Let try to fix this for
          // everybody.
          //

          // Check if 'xsi' is already taken.
          //
          if (e.lookupNamespaceURI (
                xml::string (xml::bits::xsi_prefix<C> ()).c_str ()) != 0)
          {
            throw xsi_already_in_use<C> ();
          }
          std::basic_string<C> xsi (xml::bits::xmlns_prefix<C> ());
          xsi += C(':');
          xsi += xml::bits::xsi_prefix<C> ();

          xercesc::DOMElement& root (
            *e.getOwnerDocument ()->getDocumentElement ());

          root.setAttributeNS (
            xml::string (xml::bits::xmlns_namespace<C> ()).c_str (),
            xml::string (xsi).c_str (),
            xml::string (xml::bits::xsi_namespace<C> ()).c_str ());

          set_xsi_type (e, ti);
        }
      }


      // type_serializer_plate
      //
      template<unsigned long id, typename C>
      type_serializer_plate<id, C>::
      type_serializer_plate ()
      {
        if (count == 0)
          map = new type_serializer_map<C>;

        ++count;
      }

      template<unsigned long id, typename C>
      type_serializer_plate<id, C>::
      ~type_serializer_plate ()
      {
        if (--count == 0)
          delete map;
      }


      //
      //
      template<unsigned long id, typename C>
      type_serializer_map<C>&
      type_serializer_map_instance ()
      {
        return *type_serializer_plate<id, C>::map;
      }

      // bits::serializer
      //
      namespace bits
      {
        template<typename C, typename X>
        void serializer<C, X>::
        impl (xercesc::DOMElement& e, const type& x)
        {
          e << static_cast<const X&> (x);
        }
      }


      // type_serializer_initializer
      //
      template<unsigned long id, typename C, typename X>
      type_serializer_initializer<id, C, X>::
      type_serializer_initializer (const C* name, const C* ns)
      {
        type_serializer_map_instance<id, C> ().register_type (
          typeid (X),
          qualified_name<C> (name, ns),
          &bits::serializer<C, X>::impl);
      }

      template<unsigned long id, typename C, typename X>
      type_serializer_initializer<id, C, X>::
      type_serializer_initializer (const C* root_name,
                             const C* root_ns,
                             const C* subst_name,
                             const C* subst_ns)
      {
        type_serializer_map_instance<id, C> ().register_element (
          qualified_name<C> (root_name, root_ns),
          qualified_name<C> (subst_name, subst_ns),
          typeid (X),
          &bits::serializer<C, X>::impl);
      }
    }
  }
}
