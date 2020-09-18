// file      : xsd/cxx/tree/parsing.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <xsd/cxx/xml/string.hxx>        // xml::{string, transcode}
#include <xsd/cxx/xml/elements.hxx>      // xml::{prefix, uq_name}
#include <xsd/cxx/xml/bits/literals.hxx> // xml::bits::{xml_prefix,
                                         //             xml_namespace}

#include <xsd/cxx/tree/exceptions.hxx>   // no_prefix_mapping
#include <xsd/cxx/tree/elements.hxx>
#include <xsd/cxx/tree/types.hxx>
#include <xsd/cxx/tree/list.hxx>
#include <xsd/cxx/tree/text.hxx>         // text_content

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      // type
      //
      inline _type::
      _type (const xercesc::DOMElement& e, flags f, type* c)
          : dom_info_ (0), container_ (c)
      {
        if (f & flags::keep_dom)
        {
          std::auto_ptr<dom_info> r (dom_info_factory::create (e, *this, f));
          dom_info_ = r;
        }
      }

      inline _type::
      _type (const xercesc::DOMAttr& a, flags f, type* c)
          : dom_info_ (0), container_ (c)
      {
        if (f & flags::keep_dom)
        {
          std::auto_ptr<dom_info> r (dom_info_factory::create (a, *this, f));
          dom_info_ = r;
        }
      }

      template <typename C>
      inline _type::
      _type (const std::basic_string<C>&,
             const xercesc::DOMElement*,
             flags,
             type* c)
          : dom_info_ (0), // List elements don't have associated DOM nodes.
            container_ (c)
      {
      }

      // simple_type
      //
      template <typename B>
      inline simple_type<B>::
      simple_type (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c)
      {
      }

      template <typename B>
      inline simple_type<B>::
      simple_type (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c)
      {
      }

      template <typename B>
      template <typename C>
      inline simple_type<B>::
      simple_type (const std::basic_string<C>& s,
                   const xercesc::DOMElement* e,
                   flags f,
                   type* c)
          : B (s, e, f, c)
      {
      }

      // fundamental_base
      //
      template <typename X, typename C, typename B>
      fundamental_base<X, C, B>::
      fundamental_base (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            x_ (traits<X, C>::create (e, f, c))
      {
      }

      template <typename X, typename C, typename B>
      fundamental_base<X, C, B>::
      fundamental_base (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            x_ (traits<X, C>::create (a, f, c))
      {
      }

      template <typename X, typename C, typename B>
      fundamental_base<X, C, B>::
      fundamental_base (const std::basic_string<C>& s,
                        const xercesc::DOMElement* e,
                        flags f,
                        type* c)
          : B (s, e, f, c),
            x_ (traits<X, C>::create (s, e, f, c))
      {
      }

      // Insertion operators for list.
      //

      // Individual items of the list have no DOM association. Therefore
      // I clear keep_dom from flags.
      //

      template <typename X, typename C>
      list<X, C, false>::
      list (const xercesc::DOMElement& e, flags f, type* c)
          : sequence<X> (flags (f & ~flags::keep_dom), c) // ambiguous
      {
        init (text_content<C> (e), &e);
      }

      template <typename X, typename C>
      list<X, C, false>::
      list (const xercesc::DOMAttr& a, flags f, type* c)
          : sequence<X> (flags (f & ~flags::keep_dom), c) // ambiguous
      {
        init (xml::transcode<C> (a.getValue ()), a.getOwnerElement ());
      }

      template <typename X, typename C>
      list<X, C, false>::
      list (const std::basic_string<C>& s,
            const xercesc::DOMElement* e,
            flags f,
            type* c)
          : sequence<X> (flags (f & ~flags::keep_dom), c) // ambiguous
      {
        init (s, e);
      }

      template <typename X, typename C>
      void list<X, C, false>::
      init (const std::basic_string<C>& s, const xercesc::DOMElement* parent)
      {
        // Note that according to the spec the separator is exactly
        // one space (0x20). This makes our life much easier.
        //
        using std::basic_string;

        for (std::size_t i (0), j (s.find (C (' ')));;)
        {
          if (j != basic_string<C>::npos)
          {
            ptr r (
              new X (basic_string<C> (s, i, j - i),
                     parent,
                     this->flags_,
                     this->container_));

            this->v_.push_back (r);

            i = j + 1;
            j = s.find (C (' '), i);
          }
          else
          {
            // Last element.
            //
            ptr r (
              new X (basic_string<C> (s, i),
                     parent,
                     this->flags_,
                     this->container_));

            this->v_.push_back (r);

            break;
          }
        }
      }

      template <typename X, typename C>
      list<X, C, true>::
      list (const xercesc::DOMElement& e, flags f, type* c)
          : sequence<X> (flags (f & ~flags::keep_dom), c) // ambiguous
      {
        init (text_content<C> (e), &e);
      }

      template <typename X, typename C>
      inline list<X, C, true>::
      list (const xercesc::DOMAttr& a, flags f, type* c)
          : sequence<X> (flags (f & ~flags::keep_dom), c) // ambiguous
      {
        init (xml::transcode<C> (a.getValue ()), a.getOwnerElement ());
      }

      template <typename X, typename C>
      inline list<X, C, true>::
      list (const std::basic_string<C>& s,
            const xercesc::DOMElement* parent,
            flags f,
            type* c)
          : sequence<X> (flags (f & ~flags::keep_dom), c) // ambiguous
      {
        init (s, parent);
      }

      template <typename X, typename C>
      inline void list<X, C, true>::
      init (const std::basic_string<C>& s, const xercesc::DOMElement* parent)
      {
        // Note that according to the spec the separator is exactly
        // one space (0x20). This makes our life much easier.
        //
        using std::basic_string;

        for (std::size_t i (0), j (s.find (C (' ')));;)
        {
          if (j != basic_string<C>::npos)
          {
            push_back (
              traits<X, C>::create (
                basic_string<C> (s, i, j - i), parent, 0, 0));

            i = j + 1;
            j = s.find (C (' '), i);
          }
          else
          {
            // Last element.
            //
            push_back (
              traits<X, C>::create (
                basic_string<C> (s, i), parent, 0, 0));
            break;
          }
        }
      }


      // Insertion operators for built-in types.
      //


      // string
      //
      template <typename C, typename B>
      string<C, B>::
      string (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      string<C, B>::
      string (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      string<C, B>::
      string (const std::basic_string<C>& s,
              const xercesc::DOMElement* e,
              flags f,
              type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // normalized_string
      //
      template <typename C, typename B>
      normalized_string<C, B>::
      normalized_string (const xercesc::DOMElement& e, flags f, type* c)
          : base_type (e, f, c)
      {
      }

      template <typename C, typename B>
      normalized_string<C, B>::
      normalized_string (const xercesc::DOMAttr& a, flags f, type* c)
          : base_type (a, f, c)
      {
      }

      template <typename C, typename B>
      normalized_string<C, B>::
      normalized_string (const std::basic_string<C>& s,
                         const xercesc::DOMElement* e,
                         flags f,
                         type* c)
          : base_type (s, e, f, c)
      {
      }


      // token
      //
      template <typename C, typename B>
      token<C, B>::
      token (const xercesc::DOMElement& e, flags f, type* c)
          : base_type (e, f, c)
      {
      }

      template <typename C, typename B>
      token<C, B>::
      token (const xercesc::DOMAttr& a, flags f, type* c)
          : base_type (a, f, c)
      {
      }

      template <typename C, typename B>
      token<C, B>::
      token (const std::basic_string<C>& s,
             const xercesc::DOMElement* e,
             flags f,
             type* c)
          : base_type (s, e, f, c)
      {
      }


      // nmtoken
      //
      template <typename C, typename B>
      nmtoken<C, B>::
      nmtoken (const xercesc::DOMElement& e, flags f, type* c)
          : base_type (e, f, c)
      {
      }

      template <typename C, typename B>
      nmtoken<C, B>::
      nmtoken (const xercesc::DOMAttr& a, flags f, type* c)
          : base_type (a, f, c)
      {
      }

      template <typename C, typename B>
      nmtoken<C, B>::
      nmtoken (const std::basic_string<C>& s,
               const xercesc::DOMElement* e,
               flags f,
               type* c)
          : base_type (s, e, f, c)
      {
      }


      // nmtokens
      //
      template <typename C, typename B, typename nmtoken>
      nmtokens<C, B, nmtoken>::
      nmtokens (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c), base_type (e, f, c)
      {
      }

      template <typename C, typename B, typename nmtoken>
      nmtokens<C, B, nmtoken>::
      nmtokens (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c), base_type (a, f, c)
      {
      }

      template <typename C, typename B, typename nmtoken>
      nmtokens<C, B, nmtoken>::
      nmtokens (const std::basic_string<C>& s,
                const xercesc::DOMElement* e,
                flags f,
                type* c)
          : B (s, e, f, c), base_type (s, e, f, c)
      {
      }


      // name
      //
      template <typename C, typename B>
      name<C, B>::
      name (const xercesc::DOMElement& e, flags f, type* c)
          : base_type (e, f, c)
      {
      }

      template <typename C, typename B>
      name<C, B>::
      name (const xercesc::DOMAttr& a, flags f, type* c)
          : base_type (a, f, c)
      {
      }

      template <typename C, typename B>
      name<C, B>::
      name (const std::basic_string<C>& s,
            const xercesc::DOMElement* e,
            flags f,
            type* c)
          : base_type (s, e, f, c)
      {
      }


      // ncname
      //
      template <typename C, typename B>
      ncname<C, B>::
      ncname (const xercesc::DOMElement& e, flags f, type* c)
          : base_type (e, f, c)
      {
      }

      template <typename C, typename B>
      ncname<C, B>::
      ncname (const xercesc::DOMAttr& a, flags f, type* c)
          : base_type (a, f, c)
      {
      }

      template <typename C, typename B>
      ncname<C, B>::
      ncname (const std::basic_string<C>& s,
              const xercesc::DOMElement* e,
              flags f,
              type* c)
          : base_type (s, e, f, c)
      {
      }


      // language
      //
      template <typename C, typename B>
      language<C, B>::
      language (const xercesc::DOMElement& e, flags f, type* c)
          : base_type (e, f, c)
      {
      }

      template <typename C, typename B>
      language<C, B>::
      language (const xercesc::DOMAttr& a, flags f, type* c)
          : base_type (a, f, c)
      {
      }

      template <typename C, typename B>
      language<C, B>::
      language (const std::basic_string<C>& s,
                const xercesc::DOMElement* e,
                flags f,
                type* c)
          : base_type (s, e, f, c)
      {
      }


      // id
      //
      template <typename C, typename B>
      id<C, B>::
      id (const xercesc::DOMElement& e, flags f, type* c)
          : base_type (e, f, c), identity_ (*this)
      {
        register_id ();
      }

      template <typename C, typename B>
      id<C, B>::
      id (const xercesc::DOMAttr& a, flags f, type* c)
          : base_type (a, f, c), identity_ (*this)
      {
        register_id ();
      }

      template <typename C, typename B>
      id<C, B>::
      id (const std::basic_string<C>& s,
          const xercesc::DOMElement* e,
          flags f,
          type* c)
          : base_type (s, e, f, c), identity_ (*this)
      {
        register_id ();
      }


      // idref
      //
      template <typename X, typename C, typename B>
      idref<X, C, B>::
      idref (const xercesc::DOMElement& e, flags f, tree::type* c)
          : base_type (e, f, c), identity_ (*this)
      {
      }

      template <typename X, typename C, typename B>
      idref<X, C, B>::
      idref (const xercesc::DOMAttr& a, flags f, tree::type* c)
          : base_type (a, f , c), identity_ (*this)
      {
      }

      template <typename X, typename C, typename B>
      idref<X, C, B>::
      idref (const std::basic_string<C>& s,
             const xercesc::DOMElement* e,
             flags f,
             tree::type* c)
          : base_type (s, e, f, c), identity_ (*this)
      {
      }



      // idrefs
      //
      template <typename C, typename B, typename idref>
      idrefs<C, B, idref>::
      idrefs (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c), base_type (e, f, c)
      {
      }

      template <typename C, typename B, typename idref>
      idrefs<C, B, idref>::
      idrefs (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c), base_type (a, f, c)
      {
      }

      template <typename C, typename B, typename idref>
      idrefs<C, B, idref>::
      idrefs (const std::basic_string<C>& s,
              const xercesc::DOMElement* e,
              flags f,
              type* c)
          : B (s, e, f, c), base_type (s, e, f, c)
      {
      }


      // uri
      //
      template <typename C, typename B>
      uri<C, B>::
      uri (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      uri<C, B>::
      uri (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      uri<C, B>::
      uri (const std::basic_string<C>& s,
           const xercesc::DOMElement* e,
           flags f,
           type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // qname
      //
      template <typename C, typename B, typename uri, typename ncname>
      qname<C, B, uri, ncname>::
      qname (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c)
      {
        std::basic_string<C> v (text_content<C> (e));
        ns_ = resolve (v, &e);
        name_ = xml::uq_name (v);
      }

      template <typename C, typename B, typename uri, typename ncname>
      qname<C, B, uri, ncname>::
      qname (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c)
      {
        std::basic_string<C> v (xml::transcode<C> (a.getValue ()));
        ns_ = resolve (v, a.getOwnerElement ());
        name_ = xml::uq_name (v);
      }

      template <typename C, typename B, typename uri, typename ncname>
      qname<C, B, uri, ncname>::
      qname (const std::basic_string<C>& s,
             const xercesc::DOMElement* e,
             flags f,
             type* c)
          : B (s, e, f, c),
            ns_ (resolve (s, e)),
            name_ (xml::uq_name (s))
      {
      }

      template <typename C, typename B, typename uri, typename ncname>
      uri qname<C, B, uri, ncname>::
      resolve (const std::basic_string<C>& s, const xercesc::DOMElement* e)
      {
        std::basic_string<C> p (xml::prefix (s));

        if (e)
        {
          // This code is copied verbatim from xml/dom/elements.hxx.
          //

          // 'xml' prefix requires special handling and Xerces folks refuse
          // to handle this in DOM so I have to do it myself.
          //
          if (p == xml::bits::xml_prefix<C> ())
            return xml::bits::xml_namespace<C> ();

          const XMLCh* xns (
            e->lookupNamespaceURI (
              p.empty () ? 0 : xml::string (p).c_str ()));

          if (xns != 0)
            return xml::transcode<C> (xns);
        }

        throw no_prefix_mapping<C> (p);
      }


      // base64_binary
      //
      template <typename C, typename B>
      base64_binary<C, B>::
      base64_binary (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c)
      {
        // This implementation is not optimal.
        //
        std::basic_string<C> str (text_content<C> (e));
        decode (xml::string (str).c_str ());
      }

      template <typename C, typename B>
      base64_binary<C, B>::
      base64_binary (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c)
      {
        decode (a.getValue ());
      }

      template <typename C, typename B>
      base64_binary<C, B>::
      base64_binary (const std::basic_string<C>& s,
                     const xercesc::DOMElement* e,
                     flags f,
                     type* c)
          : B (s, e, f, c)
      {
        decode (xml::string (s).c_str ());
      }


      // hex_binary
      //
      template <typename C, typename B>
      hex_binary<C, B>::
      hex_binary (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c)
      {
        // This implementation is not optimal.
        //
        std::basic_string<C> str (text_content<C> (e));
        decode (xml::string (str).c_str ());
      }

      template <typename C, typename B>
      hex_binary<C, B>::
      hex_binary (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c)
      {
        decode (a.getValue ());
      }

      template <typename C, typename B>
      hex_binary<C, B>::
      hex_binary (const std::basic_string<C>& s,
                  const xercesc::DOMElement* e,
                  flags f,
                  type* c)
          : B (s, e, f, c)
      {
        decode (xml::string (s).c_str ());
      }


      // date
      //
      template <typename C, typename B>
      date<C, B>::
      date (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      date<C, B>::
      date (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      date<C, B>::
      date (const std::basic_string<C>& s,
            const xercesc::DOMElement* e,
            flags f,
            type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // date_time
      //
      template <typename C, typename B>
      date_time<C, B>::
      date_time (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      date_time<C, B>::
      date_time (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      date_time<C, B>::
      date_time (const std::basic_string<C>& s,
                 const xercesc::DOMElement* e,
                 flags f,
                 type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // duration
      //
      template <typename C, typename B>
      duration<C, B>::
      duration (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      duration<C, B>::
      duration (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      duration<C, B>::
      duration (const std::basic_string<C>& s,
                const xercesc::DOMElement* e,
                flags f,
                type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // day
      //
      template <typename C, typename B>
      day<C, B>::
      day (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      day<C, B>::
      day (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      day<C, B>::
      day (const std::basic_string<C>& s,
           const xercesc::DOMElement* e,
           flags f,
           type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // month
      //
      template <typename C, typename B>
      month<C, B>::
      month (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      month<C, B>::
      month (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      month<C, B>::
      month (const std::basic_string<C>& s,
             const xercesc::DOMElement* e,
             flags f,
             type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // month_day
      //
      template <typename C, typename B>
      month_day<C, B>::
      month_day (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      month_day<C, B>::
      month_day (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      month_day<C, B>::
      month_day (const std::basic_string<C>& s,
                 const xercesc::DOMElement* e,
                 flags f,
                 type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // year
      //
      template <typename C, typename B>
      year<C, B>::
      year (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      year<C, B>::
      year (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      year<C, B>::
      year (const std::basic_string<C>& s,
            const xercesc::DOMElement* e,
            flags f,
            type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // year_month
      //
      template <typename C, typename B>
      year_month<C, B>::
      year_month (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      year_month<C, B>::
      year_month (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      year_month<C, B>::
      year_month (const std::basic_string<C>& s,
                  const xercesc::DOMElement* e,
                  flags f,
                  type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // time
      //
      template <typename C, typename B>
      time<C, B>::
      time (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c),
            base_type (text_content<C> (e))
      {
      }

      template <typename C, typename B>
      time<C, B>::
      time (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c),
            base_type (xml::transcode<C> (a.getValue ()))
      {
      }

      template <typename C, typename B>
      time<C, B>::
      time (const std::basic_string<C>& s,
            const xercesc::DOMElement* e,
            flags f,
            type* c)
          : B (s, e, f, c), base_type (s)
      {
      }


      // entity
      //
      template <typename C, typename B>
      entity<C, B>::
      entity (const xercesc::DOMElement& e, flags f, type* c)
          : base_type (e, f, c)
      {
      }

      template <typename C, typename B>
      entity<C, B>::
      entity (const xercesc::DOMAttr& a, flags f, type* c)
          : base_type (a, f, c)
      {
      }

      template <typename C, typename B>
      entity<C, B>::
      entity (const std::basic_string<C>& s,
              const xercesc::DOMElement* e,
              flags f,
              type* c)
          : base_type (s, e, f, c)
      {
      }


      // entities
      //
      template <typename C, typename B, typename entity>
      entities<C, B, entity>::
      entities (const xercesc::DOMElement& e, flags f, type* c)
          : B (e, f, c), base_type (e, f, c)
      {
      }

      template <typename C, typename B, typename entity>
      entities<C, B, entity>::
      entities (const xercesc::DOMAttr& a, flags f, type* c)
          : B (a, f, c), base_type (a, f, c)
      {
      }

      template <typename C, typename B, typename entity>
      entities<C, B, entity>::
      entities (const std::basic_string<C>& s,
                const xercesc::DOMElement* e,
                flags f,
                type* c)
          : B (s, e, f, c), base_type (s, e, f, c)
      {
      }
    }
  }
}

