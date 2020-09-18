// file      : xsd/cxx/parser/validating/types.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <limits>
#include <xsd/cxx/parser/zc-istream.hxx>
#include <xsd/cxx/parser/exceptions.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      namespace validating
      {

        // byte
        //

        template <typename C>
        void byte<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool byte<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename byte<C>::type byte<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          short t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted () && t >= -128 && t <= 127))
          {
            throw invalid_value<C> (bits::byte<C> (), str);
          }

          return static_cast<type> (t);
        }

        // unsigned_byte
        //

        template <typename C>
        void unsigned_byte<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool unsigned_byte<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename unsigned_byte<C>::type unsigned_byte<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          unsigned short t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted () && t <= 255))
          {
            throw invalid_value<C> (bits::unsigned_byte<C> (), str);
          }

          return static_cast<type> (t);
        }

        // short
        //

        template <typename C>
        void short_<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool short_<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename short_<C>::type short_<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          type t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted ()))
          {
            throw invalid_value<C> (bits::short_<C> (), str);
          }

          return t;
        }

        // unsigned_short
        //

        template <typename C>
        void unsigned_short<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool unsigned_short<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename unsigned_short<C>::type unsigned_short<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          type t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted ()))
          {
            throw invalid_value<C> (bits::unsigned_short<C> (), str);
          }

          return t;
        }

        // int
        //

        template <typename C>
        void int_<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool int_<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename int_<C>::type int_<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          type t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted ()))
          {
            throw invalid_value<C> (bits::int_<C> (), str);
          }

          return t;
        }

        // unsigned_int
        //

        template <typename C>
        void unsigned_int<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool unsigned_int<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename unsigned_int<C>::type unsigned_int<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          type t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted ()))
          {
            throw invalid_value<C> (bits::unsigned_int<C> (), str);
          }

          return t;
        }

        // long
        //

        template <typename C>
        void long_<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool long_<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename long_<C>::type long_<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          type t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted ()))
          {
            throw invalid_value<C> (bits::long_<C> (), str);
          }

          return t;
        }

        // unsigned_long
        //

        template <typename C>
        void unsigned_long<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool unsigned_long<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename unsigned_long<C>::type unsigned_long<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          type t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted ()))
          {
            throw invalid_value<C> (bits::unsigned_long<C> (), str);
          }

          return t;
        }

        // boolean
        //

        template <typename C>
        void boolean<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool boolean<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename boolean<C>::type boolean<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          if (str == bits::true_<C> () || str == bits::one<C> ())
            return true;
          else if (str == bits::false_<C> () || str == bits::zero<C> ())
            return false;

          throw invalid_value<C> (bits::boolean<C> (), str);
        }


        // float
        //

        template <typename C>
        void float_<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool float_<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename float_<C>::type float_<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          if (str == bits::positive_inf<C> ())
            return std::numeric_limits<type>::infinity ();

          if (str == bits::negative_inf<C> ())
            return -std::numeric_limits<type>::infinity ();

          if (str == bits::nan<C> ())
            return std::numeric_limits<type>::quiet_NaN ();

          type t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted ()))
          {
            throw invalid_value<C> (bits::float_<C> (), str);
          }

          return t;
        }

        // double
        //

        template <typename C>
        void double_<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool double_<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename double_<C>::type double_<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          if (str == bits::positive_inf<C> ())
            return std::numeric_limits<type>::infinity ();

          if (str == bits::negative_inf<C> ())
            return -std::numeric_limits<type>::infinity ();

          if (str == bits::nan<C> ())
            return std::numeric_limits<type>::quiet_NaN ();

          type t;
          zc_istream<C> is (str);

          if (!(is >> t && is.exhausted ()))
          {
            throw invalid_value<C> (bits::double_<C> (), str);
          }

          return t;
        }

        // decimal
        //

        template <typename C>
        void decimal<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool decimal<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename decimal<C>::type decimal<C>::
        post ()
        {
          std::basic_string<C> tmp;
          tmp.swap (str_);

          ro_string<C> str (tmp);
          trim (str);

          type t;
          zc_istream<C> is (str);

          //@@ TODO: now we accept both fixed and scientific notations.
          //
          if (!(is >> t && is.exhausted ()))
          {
            throw invalid_value<C> (bits::decimal<C> (), str);
          }

          return t;
        }

        // string
        //

        template <typename C>
        void string<C>::
        _pre ()
        {
          str_.clear ();
        }

        template <typename C>
        bool string<C>::
        _characters_impl (const ro_string<C>& s)
        {
          str_ += s;
          return true;
        }

        template <typename C>
        typename string<C>::type string<C>::
        post ()
        {
          type r;
          r.swap (str_);
          return r;
        }

        //
        //
        namespace bits
        {
          // Find first non-space character.
          //
          template <typename C>
          typename ro_string<C>::size_type
          find_ns (const C* s,
                   typename ro_string<C>::size_type size,
                   typename ro_string<C>::size_type pos)
          {
            typename ro_string<C>::size_type i (pos);

            while (i < size &&
                   (s[i] == C (0x20) || // space
                    s[i] == C (0x0D) || // carriage return
                    s[i] == C (0x09) || // tab
                    s[i] == C (0x0A)))
              ++i;

            return i < size ? i : ro_string<C>::npos;
          }

          // Find first space character.
          //
          template <typename C>
          typename ro_string<C>::size_type
          find_s (const C* s,
                  typename ro_string<C>::size_type size,
                  typename ro_string<C>::size_type pos)
          {
            typename ro_string<C>::size_type i (pos);

            while (i < size &&
                   s[i] != C (0x20) && // space
                   s[i] != C (0x0D) && // carriage return
                   s[i] != C (0x09) && // tab
                   s[i] != C (0x0A))
              ++i;

            return i < size ? i : ro_string<C>::npos;
          }
        }


        // list
        //

        // Relevant XML Schema Part 2: Datatypes sections: 4.2.1.2, 4.3.6.
        //

        template <typename X, typename C>
        void list<X, C>::
        _pre ()
        {
          buf_.clear ();
        }

        template <typename X, typename C>
        bool list<X, C>::
        _characters_impl (const ro_string<C>& s)
        {
          if (item_ == 0 || s.size () == 0)
            return true;

          typedef typename ro_string<C>::size_type size_type;

          const C* data (s.data ());
          size_type size (s.size ());

          // Traverse the data while logically collapsing spaces.
          //
          for (size_type i (bits::find_ns (data, size, 0));
               i != ro_string<C>::npos;)
          {
            size_type j (bits::find_s (data, size, i));

            if (j != ro_string<C>::npos)
            {
              item_->pre ();
              item_->_pre ();

              if (!buf_.empty ())
              {
                // Assemble the first item in str from buf_ and s.
                //
                std::basic_string<C> str;
                str.swap (buf_);
                str.append (data + i, j - i);

                ro_string<C> tmp (str); // Private copy ctor.
                item_->_characters (tmp);
              }
              else
              {
                ro_string<C> tmp (data + i, j - i); // Private copy ctor.
                item_->_characters (tmp);
              }

              item_->_post ();
              item (item_->post ());

              i = bits::find_ns (data, size, j);
            }
            else
            {
              buf_.assign (data + i, size - i);
              break;
            }
          }

          return true;
        }

        template <typename X, typename C>
        void list<X, C>::
        _post ()
        {
          // Handle last item.
          //
          if (!buf_.empty ())
          {
            if (item_ != 0)
            {
              item_->pre ();
              item_->_pre_impl ();

              ro_string<C> tmp (buf_); // Private copy ctor.
              item_->_characters (buf_);

              item_->_post_impl ();
              item (item_->post ());
            }

            buf_.clear ();
          }

          simple_content<C>::_post ();
        }


        // list<void>
        //


        template <typename C>
        void list<void, C>::
        _pre ()
        {
          buf_.clear ();
        }

        template <typename C>
        bool list<void, C>::
        _characters_impl (const ro_string<C>& s)
        {
          if (item_ == 0 || s.size () == 0)
            return true;

          typedef typename ro_string<C>::size_type size_type;

          const C* data (s.data ());
          size_type size (s.size ());

          // Traverse the data while logically collapsing spaces.
          //
          for (size_type i (bits::find_ns (data, size, 0));
               i != ro_string<C>::npos;)
          {
            size_type j (bits::find_s (data, size, i));

            if (j != ro_string<C>::npos)
            {
              item_->pre ();
              item_->_pre ();

              if (!buf_.empty ())
              {
                // Assemble the first item in str from buf_ and s.
                //
                std::basic_string<C> str;
                str.swap (buf_);
                str.append (data + i, j - i);

                ro_string<C> tmp (str); // Private copy ctor.
                item_->_characters (tmp);
              }
              else
              {
                ro_string<C> tmp (data + i, j - i); // Private copy ctor.
                item_->_characters (tmp);
              }

              item_->_post ();
              item_->post ();
              item ();

              i = bits::find_ns (data, size, j);
            }
            else
            {
              buf_.assign (data + i, size - i);
              break;
            }
          }

          return true;
        }

        template <typename C>
        void list<void, C>::
        _post ()
        {
          // Handle last item.
          //
          if (!buf_.empty ())
          {
            if (item_ != 0)
            {
              item_->pre ();
              item_->_pre_impl ();

              ro_string<C> tmp (buf_); // Private copy ctor.
              item_->_characters (buf_);

              item_->_post_impl ();
              item_->post ();
              item ();
            }

            buf_.clear ();
          }

          simple_content<C>::_post ();
        }
      }
    }
  }
}
