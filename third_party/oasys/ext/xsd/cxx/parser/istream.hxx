// file      : xsd/cxx/parser/istream.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2006 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_PARSER_ISTREAM_HXX
#define XSD_CXX_PARSER_ISTREAM_HXX

#include <string>
#include <istream>

#include <xsd/cxx/parser/ro-string.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      // Input streambuffer that does not copy the underlying
      // buffer.
      //
      template <typename C>
      class streambuf: public std::basic_streambuf<C>
      {
      public:
        typedef std::char_traits<C>            traits_type;
        typedef typename traits_type::int_type int_type;

      public:
        streambuf (const ro_string<C>& str)
            : str_ (str.data (), str.size ())
        {
          init ();
        }

        streambuf (const std::basic_string<C>& str)
            : str_ (str)
        {
          init ();
        }

      protected:
        virtual std::streamsize
        showmanyc ()
        {
          return static_cast<std::streamsize> (
            this->egptr () - this->gptr ());
        }

        virtual int_type
        underflow ()
        {
          int_type r = traits_type::eof ();

	  if (this->gptr () < this->egptr ())
	    r = traits_type::to_int_type (*this->gptr ());

          return r;
        }

      private:
        void
        init ()
        {
          C* b (const_cast<C*> (str_.data ()));
          C* e (b + str_.size ());

          setg (b, b,e);
        }

      private:
        streambuf (const streambuf&);

        streambuf&
        operator= (const streambuf&);

      private:
        ro_string<C> str_;
      };

      // Input string stream that does not copy the underlying string.
      //
      template <typename C>
      class istream_base
      {
      protected:
        istream_base (const ro_string<C>& str)
            : buf_ (str)
        {
        }

        istream_base (const std::basic_string<C>& str)
            : buf_ (str)
        {
        }

      protected:
        streambuf<C> buf_;
      };

      template <typename C>
      class istream: protected istream_base<C>,
                     public std::basic_istream<C>
      {
      public:
        istream (const ro_string<C>& str)
            : istream_base<C> (str),
              std::basic_istream<C> (&this->buf_)
        {
        }

        istream (const std::basic_string<C>& str)
            : istream_base<C> (str),
              std::basic_istream<C> (&this->buf_)
        {
        }

        bool
        exhausted ()
        {
          return this->get () == std::basic_istream<C>::traits_type::eof ();
        }

      private:
        istream (const istream&);

        istream&
        operator= (const istream&);
      };
    }
  }
}

#endif  // XSD_CXX_PARSER_ISTREAM_HXX
