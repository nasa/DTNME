// file      : xsd/cxx/tree/traits.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <sstream>

#include <xsd/cxx/xml/string.hxx> // xml::transcode

#include <xsd/cxx/tree/text.hxx>  // text_content

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      // bool
      //
      template <typename C>
      bool traits<bool, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      bool traits<bool, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      bool traits<bool, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        return (s == bits::true_<C> ()) || (s == bits::one<C> ());
      }


      // 8 bit
      //

      template <typename C>
      signed char traits<signed char, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      signed char traits<signed char, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      signed char traits<signed char, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        short t;
        is >> t;

        return static_cast<type> (t);
      }



      template <typename C>
      unsigned char traits<unsigned char, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      unsigned char traits<unsigned char, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      unsigned char traits<unsigned char, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        unsigned short t;
        is >> t;

        return static_cast<type> (t);
      }


      // 16 bit
      //

      template <typename C>
      short traits<short, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      short traits<short, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      short traits<short, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        type t;
        is >> t;

        return t;
      }


      template <typename C>
      unsigned short traits<unsigned short, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      unsigned short traits<unsigned short, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      unsigned short traits<unsigned short, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        type t;
        is >> t;

        return t;
      }


      // 32 bit
      //

      template <typename C>
      int traits<int, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      int traits<int, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      int traits<int, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        type t;
        is >> t;

        return t;
      }


      template <typename C>
      unsigned int traits<unsigned int, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      unsigned int traits<unsigned int, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      unsigned int traits<unsigned int, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        type t;
        is >> t;

        return t;
      }


      // 64 bit
      //

      template <typename C>
      long long traits<long long, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      long long traits<long long, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      long long traits<long long, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        type t;
        is >> t;

        return t;
      }


      template <typename C>
      unsigned long long traits<unsigned long long, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      unsigned long long traits<unsigned long long, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      unsigned long long traits<unsigned long long, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        type t;
        is >> t;

        return t;
      }


      // floating point
      //

      template <typename C>
      float traits<float, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      float traits<float, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      float traits<float, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        type t;
        is >> t;

        return t;
      }


      template <typename C>
      double traits<double, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      double traits<double, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      double traits<double, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        type t;
        is >> t;

        return t;
      }


      template <typename C>
      long double traits<long double, C>::
      create (const xercesc::DOMElement& e, flags f, tree::type* c)
      {
        return create (text_content<C> (e), 0, f, c);
      }

      template <typename C>
      long double traits<long double, C>::
      create (const xercesc::DOMAttr& a, flags f, tree::type* c)
      {
        return create (xml::transcode<C> (a.getValue ()), 0, f, c);
      }

      template <typename C>
      long double traits<long double, C>::
      create (const std::basic_string<C>& s,
              const xercesc::DOMElement*,
              flags,
              tree::type*)
      {
        std::basic_istringstream<C> is (s);

        type t;
        is >> t;

        return t;
      }
    }
  }
}

