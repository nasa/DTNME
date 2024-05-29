// file      : xsd/cxx/tree/type-name-map.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2006 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_TREE_TYPE_NAME_MAP_HXX
#define XSD_CXX_TREE_TYPE_NAME_MAP_HXX

#include <map>
#include <string>
#include <typeinfo>

#include <xercesc/dom/DOMElement.hpp>

#include <xsd/cxx/xml/bits/literals.hxx> // xml::bits::{xsi_namespace, type}
#include <xsd/cxx/xml/dom/elements.hxx>  // dom::{element, attribute}
#include <xsd/cxx/xml/dom/serialization.hxx>

#include <xsd/cxx/tree/elements.hxx>
#include <xsd/cxx/tree/types.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      template <typename C>
      struct qualified_name
      {
        qualified_name (std::basic_string<C> name,
                        std::basic_string<C> namespace_)
            : name_ (name), namespace__ (namespace_)
        {
        }

        const std::basic_string<C>&
        name () const
        {
          return name_;
        }

        std::basic_string<C>
        namespace_ () const
        {
          return namespace__;
        }

      private:
        std::basic_string<C> name_;
        std::basic_string<C> namespace__;
      };

      template <typename C>
      inline bool
      operator== (const qualified_name<C>& x, const qualified_name<C>& y)
      {
        return x.name () == y.name () && x.namespace_ () == y.namespace_ ();
      }

      template <typename C>
      inline bool
      operator!= (const qualified_name<C>& x, const qualified_name<C>& y)
      {
        return !(x == y);
      }

      template <typename C>
      inline bool
      operator< (const qualified_name<C>& x, const qualified_name<C>& y)
      {
        return (x.name () < y.name ()) ||
          (x.name () == y.name () && x.namespace_ () < y.namespace_ ());
      }


      template <typename C>
      struct type_name_map
      {
        typedef std::type_info type_id;

        typedef void (*serializer) (xercesc::DOMElement&, const type&);

        void
        register_type (const type_id& tid,
                       const qualified_name<C>& name,
                       serializer s,
                       bool override = true)
        {
          if (override || type_map_.find (&tid) == type_map_.end ())
            type_map_[&tid] = type_info (name, s);
        }

        void
        register_element (const qualified_name<C>& root,
                          const qualified_name<C>& subst,
                          const type_id& tid,
                          serializer s)
        {
          element_map_[root][&tid] = type_info (subst, s);
        }

      public:
        template<typename X>
        void
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


        // Serialize into existing element.
        //
        template<typename X>
        void
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

        // Create DOMDocument with root element suitable for serializing
        // X into it.
        //
        template<typename X>
        xml::dom::auto_ptr <xercesc::DOMDocument>
        serialize (const std::basic_string<C>& name, // element name
                   const std::basic_string<C>& ns,   // element namespace
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

      public:
        type_name_map ();

      public:
        struct type_info
        {
          type_info (const qualified_name<C>& name,
                     typename type_name_map::serializer serializer)
              : name_ (name), serializer_ (serializer)
          {
          }

          const qualified_name<C>&
          name () const
          {
            return name_;
          }

          typename type_name_map::serializer
          serializer () const
          {
            return serializer_;
          }

          // For std::map.
          //
          type_info ()
              : name_ (std::basic_string<C> (), std::basic_string<C> ()),
                serializer_ (0)
          {
          }

        private:
          qualified_name<C> name_;
          typename type_name_map::serializer serializer_;
        };

      public:
        const type_info*
        find_type (const type_id& tid) const
        {
          typename type_map::const_iterator i (type_map_.find (&tid));

          if (i != type_map_.end ())
            return &i->second;
          else
            return 0;
        }

      private:
        struct type_id_comparator
        {
          bool
          operator() (const type_id* x, const type_id* y) const
          {
            return x->before (*y);
          }
        };

        typedef
        std::map<const type_id*, type_info, type_id_comparator>
        type_map;

        // Map of (root-element to map of (type_id to type_info)).
        // Note that here type_info::name is element name.
        //
        typedef
        std::map<const type_id*, type_info, type_id_comparator>
        subst_map;

        typedef
        std::map<qualified_name<C>, subst_map>
        element_map;

        type_map type_map_;
        element_map element_map_;

      private:
        const type_info*
        find_substitution (const subst_map& start,
                           const type_id& tid) const
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

        // Sets an xsi:type attribute corresponding to the type_info.
        //
        void
        set_xsi_type (xercesc::DOMElement& e, const type_info& ti) const
        {
          xml::dom::attribute<C> a (xml::bits::type<C> (),
                                    xml::bits::xsi_namespace<C> (),
                                    e);

          std::basic_string<C> id (
            bits::resolve_prefix (ti.name ().namespace_ (), e));

          if (!id.empty ())
            id += C (':');

          id += ti.name ().name ();

          a.dom_attribute () << id;
        }
      };


      //
      //
      template<unsigned long id, typename C>
      struct type_name_plate
      {
        static type_name_map<C>* map;
        static unsigned long count;

        type_name_plate ()
        {
          if (count == 0)
            map = new type_name_map<C>;

          ++count;
        }

        ~type_name_plate ()
        {
          if (--count == 0)
            delete map;
        }
      };

      template<unsigned long id, typename C>
      type_name_map<C>* type_name_plate<id, C>::map = 0;

      template<unsigned long id, typename C>
      unsigned long type_name_plate<id, C>::count = 0;


      //
      //
      template<unsigned long id, typename C>
      type_name_map<C>&
      type_name_map_instance ()
      {
        return *type_name_plate<id, C>::map;
      }

      //
      //
      namespace bits
      {
        template<typename C, typename X>
        struct serializer
        {
          static void
          impl (xercesc::DOMElement& e, const type& x)
          {
            e << static_cast<const X&> (x);
          }
        };
      }

      template<unsigned long id, typename C, typename X>
      struct type_name_initializer
      {
        // Register type.
        //
        type_name_initializer (const C* name, const C* ns)
        {
          type_name_map_instance<id, C> ().register_type (
            typeid (X),
            qualified_name<C> (name, ns),
            &bits::serializer<C, X>::impl);
        }

        // Register element.
        //
        type_name_initializer (const C* root_name,
                               const C* root_ns,
                               const C* subst_name,
                               const C* subst_ns)
        {
          type_name_map_instance<id, C> ().register_element (
            qualified_name<C> (root_name, root_ns),
            qualified_name<C> (subst_name, subst_ns),
            typeid (X),
            &bits::serializer<C, X>::impl);
        }
      };
    }
  }
}


#endif // XSD_CXX_TREE_TYPE_NAME_MAP_HXX

#include <xsd/cxx/tree/type-name-map.ixx>
