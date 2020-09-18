// file      : xsd/cxx/tree/type-serializer-map.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_TREE_TYPE_SERIALIZER_MAP_HXX
#define XSD_CXX_TREE_TYPE_SERIALIZER_MAP_HXX

#include <map>
#include <string>
#include <typeinfo>

#include <xercesc/dom/DOMElement.hpp>

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
                        std::basic_string<C> namespace_);

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
      struct type_serializer_map
      {
        typedef std::type_info type_id;

        typedef void (*serializer) (xercesc::DOMElement&, const type&);

        void
        register_type (const type_id&,
                       const qualified_name<C>& name,
                       serializer,
                       bool override = true);

        void
        register_element (const qualified_name<C>& root,
                          const qualified_name<C>& subst,
                          const type_id&,
                          serializer);

      public:
        template<typename X>
        void
        serialize (const std::basic_string<C>& name, // element name
                   const std::basic_string<C>& ns,   // element namespace
                   bool global,
                   bool qualified,
                   xercesc::DOMElement& parent,
                   const X&) const;

        // Serialize into existing element.
        //
        template<typename X>
        void
        serialize (const std::basic_string<C>& root_name,
                   const std::basic_string<C>& root_ns,
                   xml::dom::element<C>&,
                   const X&) const;

        // Create DOMDocument with root element suitable for serializing
        // X into it.
        //
        template<typename X>
        xml::dom::auto_ptr<xercesc::DOMDocument>
        serialize (const std::basic_string<C>& name, // element name
                   const std::basic_string<C>& ns,   // element namespace
                   const xml::dom::namespace_infomap<C>&,
                   const X&,
                   unsigned long flags) const;

      public:
        type_serializer_map ();

      public:
        struct type_info
        {
          type_info (const qualified_name<C>& name,
                     typename type_serializer_map::serializer serializer)
              : name_ (name), serializer_ (serializer)
          {
          }

          const qualified_name<C>&
          name () const
          {
            return name_;
          }

          typename type_serializer_map::serializer
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
          typename type_serializer_map::serializer serializer_;
        };

      public:
        const type_info*
        find_type (const type_id&) const;

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
        find_substitution (const subst_map& start, const type_id&) const;

        // Sets an xsi:type attribute corresponding to the type_info.
        //
        void
        set_xsi_type (xercesc::DOMElement&, const type_info&) const;
      };


      //
      //
      template<unsigned long id, typename C>
      struct type_serializer_plate
      {
        static type_serializer_map<C>* map;
        static unsigned long count;

        type_serializer_plate ();
        ~type_serializer_plate ();
      };

      template<unsigned long id, typename C>
      type_serializer_map<C>* type_serializer_plate<id, C>::map = 0;

      template<unsigned long id, typename C>
      unsigned long type_serializer_plate<id, C>::count = 0;


      //
      //
      template<unsigned long id, typename C>
      type_serializer_map<C>&
      type_serializer_map_instance ();

      //
      //
      namespace bits
      {
        template<typename C, typename X>
        struct serializer
        {
          static void
          impl (xercesc::DOMElement&, const type&);
        };
      }

      template<unsigned long id, typename C, typename X>
      struct type_serializer_initializer
      {
        // Register type.
        //
        type_serializer_initializer (const C* name, const C* ns);

        // Register element.
        //
        type_serializer_initializer (const C* root_name,
                                     const C* root_ns,
                                     const C* subst_name,
                                     const C* subst_ns);
      };
    }
  }
}

#include <xsd/cxx/tree/type-serializer-map.txx>

#endif // XSD_CXX_TREE_TYPE_SERIALIZER_MAP_HXX

#include <xsd/cxx/tree/type-serializer-map.ixx>
