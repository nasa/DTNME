// file      : xsd/cxx/parser/document.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <cassert>

#include <xsd/cxx/parser/exceptions.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      // event_consumer
      //
      template <typename C>
      event_consumer<C>::
      ~event_consumer ()
      {
      }


      // document
      //
      template <typename C>
      document<C>::
      document (parser_base<C>& root,
                const std::basic_string<C>& ns,
                const std::basic_string<C>& name)
          : root_ (root), name_ (name), ns_ (ns), depth_ (0)
      {
      }

      template <typename C>
      void document<C>::
      start_element (const ro_string<C>& ns, const ro_string<C>& name)
      {
        if (depth_++ > 0)
          root_._start_element (ns, name);
        else
        {
          if (name_ == name && ns_ == ns)
          {
            root_.pre ();
            root_._pre_impl ();
          }
          else
            throw expected_element<C> (ns_, name_, ns, name);
        }
      }

      template <typename C>
      void document<C>::
      end_element (const ro_string<C>& ns, const ro_string<C>& name)
      {
        assert (depth_ > 0);

        if (--depth_ > 0)
          root_._end_element (ns, name);
        else
        {
          if (name_ == name && ns_ == ns)
          {
	    root_._post_impl ();
            //
            // post() will be called by somebody else.
          }
          else
            throw expected_element<C> (ns_, name_, ns, name);
        }
      }

      template <typename C>
      void document<C>::
      attribute (const ro_string<C>& ns,
                 const ro_string<C>& name,
                 const ro_string<C>& value)
      {
        assert (depth_ > 0);
        root_._attribute (ns, name, value);
      }

      template <typename C>
      void document<C>::
      characters (const ro_string<C>& s)
      {
        assert (depth_ > 0);
        root_._characters (s);
      }
    }
  }
}
