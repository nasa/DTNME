// file      : xsd/cxx/parser/non-validating/types.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_PARSER_NON_VALIDATING_TYPES_HXX
#define XSD_CXX_PARSER_NON_VALIDATING_TYPES_HXX

#include <string>

#include <xsd/cxx/parser/ro-string.hxx>
#include <xsd/cxx/parser/non-validating/parser.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      namespace non_validating
      {
        // 8-bit
        //

        template <typename C>
        struct byte: virtual parser<signed char, C>,
                     virtual simple_content<C>
        {
          typedef signed char type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        template <typename C>
        struct unsigned_byte: virtual parser<unsigned char, C>,
                              virtual simple_content<C>
        {
          typedef unsigned char type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        // 16-bit
        //

        template <typename C>
        struct short_: virtual parser<short, C>,
                       virtual simple_content<C>
        {
          typedef short type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        template <typename C>
        struct unsigned_short: virtual parser<unsigned short, C>,
                               virtual simple_content<C>
        {
          typedef unsigned short type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        // 32-bit
        //

        template <typename C>
        struct int_: virtual parser<int, C>,
                     virtual simple_content<C>
        {
          typedef int type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        template <typename C>
        struct unsigned_int: virtual parser<unsigned int, C>,
                             virtual simple_content<C>
        {
          typedef unsigned int type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        // 64-bit
        //

        template <typename C>
        struct long_: virtual parser<long long, C>,
                      virtual simple_content<C>
        {
          typedef long long type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        template <typename C>
        struct unsigned_long: virtual parser<unsigned long long, C>,
                              virtual simple_content<C>
        {
          typedef unsigned long long type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        // Boolean.
        //

        template <typename C>
        struct boolean: virtual parser<bool, C>,
                        virtual simple_content<C>
        {
          typedef bool type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        // Floats.
        //

        template <typename C>
        struct float_: virtual parser<float, C>,
                       virtual simple_content<C>
        {
          typedef float type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        template <typename C>
        struct double_: virtual parser<double, C>,
                        virtual simple_content<C>
        {
          typedef double type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        template <typename C>
        struct decimal: virtual parser<long double, C>,
                        virtual simple_content<C>
        {
          typedef long double type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>&);

          virtual type
          post ();

        protected:
          std::basic_string<C> str_;
        };


        // String.
        //

        template <typename C>
        struct string: virtual parser<std::basic_string<C>, C>,
                       virtual simple_content<C>
        {
          typedef std::basic_string<C> type;

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>& s);

          virtual type
          post ();

        protected:
          type str_;
        };


        // Template for xsd:list.
        //

        template <typename X, typename C>
        struct list: virtual simple_content<C>
        {
          list ()
              : item_ (0)
          {
          }

          // Parser hooks. Override them in your implementation.
          //
          virtual void
          item (const X&)
          {
          }

          // Parser construction API.
          //
          void
          item_parser (parser<X, C>& item)
          {
            item_ = &item;
          }

          void
          parsers (parser<X, C>& item)
          {
            item_ = &item;
          }

          // Implementation.
          //

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>& s);

          virtual void
          _post ();

        private:
          parser<X, C>* item_;
          std::basic_string<C> buf_;
        };


        // Specialization for void.
        //
        template <typename C>
        struct list<void, C>: virtual simple_content<C>
        {
          list ()
              : item_ (0)
          {
          }

          // Parser hooks. Override them in your implementation.
          //
          virtual void
          item ()
          {
          }

          // Parser construction API.
          //
          void
          item_parser (parser<void, C>& item)
          {
            item_ = &item;
          }

          void
          parsers (parser<void, C>& item)
          {
            item_ = &item;
          }

          // Implementation.
          //

          virtual void
          _pre ();

          virtual bool
          _characters_impl (const ro_string<C>& s);

          virtual void
          _post ();

        private:
          parser<void, C>* item_;
          std::basic_string<C> buf_;
        };


        //
        //
        namespace bits
        {
          // float literals: INF -INF NaN
          //
          template<typename C>
          const C*
          positive_inf ();

          template<typename C>
          const C*
          negative_inf ();

          template<typename C>
          const C*
          nan ();

          // boolean literals
          //
          template<typename C>
          const C*
          true_ ();

          template<typename C>
          const C*
          one ();
        }
      }
    }
  }
}

#include <xsd/cxx/parser/non-validating/types.txx>

#endif  // XSD_CXX_PARSER_NON_VALIDATING_TYPES_HXX

#include <xsd/cxx/parser/non-validating/types.ixx>
