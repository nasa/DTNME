// file      : xsd/cxx/tree/type-factory-map.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_TREE_TYPE_FACTORY_MAP_HXX
#define XSD_CXX_TREE_TYPE_FACTORY_MAP_HXX

#include <map>
#include <string>
#include <memory> // std::auto_ptr

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
      struct type_factory_map
      {
        typedef std::auto_ptr<type> (*factory) (const xercesc::DOMElement&,
                                                flags,
                                                type*);
        void
        register_type (const std::basic_string<C>& type_id,
                       factory,
                       bool override = true);

        void
        register_element (const std::basic_string<C>& root_id,
                          const std::basic_string<C>& subst_id,
                          factory);

        template <typename X>
        std::auto_ptr<X>
        create (const std::basic_string<C>& name, // element name
                const std::basic_string<C>& ns,   // element namespace
                bool global,
                bool qualified,
                const xml::dom::element<C>&,
                flags,
                type* c) const;

        type_factory_map ();

      private:
        template <typename X>
        static std::auto_ptr<type>
        traits_adapter (const xercesc::DOMElement&, flags, type*);

      private:
        typedef
        std::map<std::basic_string<C>, factory>
        type_map;

        // Map of (root-element to map of (subst-element to factory)).
        //
        typedef
        std::map<std::basic_string<C>, factory>
        subst_map;

        typedef
        std::map<std::basic_string<C>, subst_map>
        element_map;

        type_map type_map_;
        element_map element_map_;

      private:
        factory
        find_substitution (const subst_map& start,
                           const std::basic_string<C>& id) const;

        // The name argument is as specified in xsi:type.
        //
        factory
        find_type (const std::basic_string<C>& name,
                   const xercesc::DOMElement&) const;
      };


      //
      //
      template<unsigned long id, typename C>
      struct type_factory_plate
      {
        static type_factory_map<C>* map;
        static unsigned long count;

        type_factory_plate ();
        ~type_factory_plate ();
      };

      template<unsigned long id, typename C>
      type_factory_map<C>* type_factory_plate<id, C>::map = 0;

      template<unsigned long id, typename C>
      unsigned long type_factory_plate<id, C>::count = 0;


      //
      //
      template<unsigned long id, typename C>
      type_factory_map<C>&
      type_factory_map_instance ();

      //
      //
      template<typename C, typename X>
      std::auto_ptr<type>
      factory_impl (const xercesc::DOMElement& e, flags f, type* c);

      //
      //
      template<unsigned long id, typename C, typename X>
      struct type_factory_initializer
      {
        // Register type.
        //
        type_factory_initializer (const C* type_id);

        // Register element.
        //
        type_factory_initializer (const C* root_id, const C* subst_id);
      };
    }
  }
}

#include <xsd/cxx/tree/type-factory-map.txx>

#endif // XSD_CXX_TREE_TYPE_FACTORY_MAP_HXX

#include <xsd/cxx/tree/type-factory-map.ixx>
