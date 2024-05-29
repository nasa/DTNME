// file      : xsd/cxx/tree/stream-insertion.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_TREE_STREAM_INSERTION_HXX
#define XSD_CXX_TREE_STREAM_INSERTION_HXX

#include <xsd/cxx/tree/elements.hxx>
#include <xsd/cxx/tree/containers.hxx>
#include <xsd/cxx/tree/types.hxx>

#include <xsd/cxx/tree/ostream.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      // type
      //
      template <typename S>
      inline ostream<S>&
      operator<< (ostream<S>& s, const type&)
      {
        return s;
      }

      // simple_type
      //
      template <typename S, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const simple_type<B>&)
      {
        return s;
      }

      // fundamental_base
      //
      template <typename S, typename X, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const fundamental_base<X, C, B>& x)
      {
        const X& r (x);
        return s << r;
      }

      // optional
      //
      template <typename S, typename X, bool fund>
      inline ostream<S>&
      operator<< (ostream<S>& s, const optional<X, fund>& x)
      {
        bool p (x.present ());

        s << p;

        if (p)
          s << x.get ();

        return s;
      }

      // sequence
      //
      template <typename S, typename X, bool fund>
      ostream<S>&
      operator<< (ostream<S>& s, const sequence<X, fund>& x)
      {
        s << ostream_common::as_size<std::size_t> (x.size ());

        for (typename sequence<X, fund>::const_iterator
               b (x.begin ()), e (x.end ()), i (b); i != e; ++i)
        {
          s << *i;
        }

        return s;
      }

      // list
      //
      template <typename S, typename X, typename C, bool fund>
      ostream<S>&
      operator<< (ostream<S>& s, const list<X, C, fund>& x)
      {
        const sequence<X>& r (x);
        s << r;

        return s;
      }


      // Insertion operators for built-in types.
      //


      // string
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const string<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // normalized_string
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const normalized_string<C, B>& x)
      {
        const B& r (x);
        return s << r;
      }


      // token
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const token<C, B>& x)
      {
        const B& r (x);
        return s << r;
      }


      // nmtoken
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const nmtoken<C, B>& x)
      {
        const B& r (x);
        return s << r;
      }


      // nmtokens
      //
      template <typename S, typename C, typename B, typename nmtoken>
      inline ostream<S>&
      operator<< (ostream<S>& s, const nmtokens<C, B, nmtoken>& x)
      {
        const list<nmtoken, C>& r (x);
        return s << r;
      }


      // name
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const name<C, B>& x)
      {
        const B& r (x);
        return s << r;
      }


      // ncname
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const ncname<C, B>& x)
      {
        const B& r (x);
        return s << r;
      }


      // language
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const language<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // id
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const id<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // idref
      //
      template <typename S, typename X, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const idref<X, C, B>& x)
      {
        const B& r (x);
        return s << r;
      }


      // idrefs
      //
      template <typename S, typename C, typename B, typename idref>
      inline ostream<S>&
      operator<< (ostream<S>& s, const idrefs<C, B, idref>& x)
      {
        const list<idref, C>& r (x);
        return s << r;
      }


      // uri
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const uri<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // qname
      //
      template <typename S,
                typename C,
                typename B,
                typename uri,
                typename ncname>
      inline ostream<S>&
      operator<< (ostream<S>& s, const qname<C, B, uri, ncname>& x)
      {
        return s << x.uri () << x.name ();
      }


      // base64_binary
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const base64_binary<C, B>& x)
      {
        const buffer<C>& r (x);
        return s << r;
      }


      // hex_binary
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const hex_binary<C, B>& x)
      {
        const buffer<C>& r (x);
        return s << r;
      }


      // date
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const date<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // date_time
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const date_time<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // duration
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const duration<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // day
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const day<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // month
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const month<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // month_day
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const month_day<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // year
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const year<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // year_month
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const year_month<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // time
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const time<C, B>& x)
      {
        const std::basic_string<C>& r (x);
        return s << r;
      }


      // entity
      //
      template <typename S, typename C, typename B>
      inline ostream<S>&
      operator<< (ostream<S>& s, const entity<C, B>& x)
      {
        const B& r (x);
        return s << r;
      }


      // entities
      //
      template <typename S, typename C, typename B, typename entity>
      inline ostream<S>&
      operator<< (ostream<S>& s, const entities<C, B, entity>& x)
      {
        const list<entity, C>& r (x);
        return s << r;
      }
    }
  }
}

#endif  // XSD_CXX_TREE_STREAM_INSERTION_HXX
