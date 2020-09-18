// file      : xsd/cxx/parser/non-validating/parser.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <cassert>

#include <xsd/cxx/xml/bits/literals.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      namespace non_validating
      {

        // empty_content
        //

        template <typename C>
        void empty_content<C>::
        _start_any_element (const ro_string<C>&,
                            const ro_string<C>&)
        {
        }

        template <typename C>
        void empty_content<C>::
        _end_any_element (const ro_string<C>&,
                          const ro_string<C>&)
        {
        }

        template <typename C>
        void empty_content<C>::
        _any_attribute (const ro_string<C>&,
                        const ro_string<C>&,
                        const ro_string<C>&)
        {
        }

        template <typename C>
        void empty_content<C>::
        _any_characters (const ro_string<C>&)
        {
        }

        //
        //
        template <typename C>
        bool empty_content<C>::
        _start_element_impl (const ro_string<C>&,
                             const ro_string<C>&)
        {
          return false;
        }

        template <typename C>
        bool empty_content<C>::
        _end_element_impl (const ro_string<C>&,
                           const ro_string<C>&)
        {
          return false;
        }

        template <typename C>
        bool empty_content<C>::
        _attribute_impl (const ro_string<C>&,
                         const ro_string<C>&,
                         const ro_string<C>&)
        {
          return false;
        }

        template <typename C>
        bool empty_content<C>::
        _characters_impl (const ro_string<C>&)
        {
          return false;
        }

        template <typename C>
        void empty_content<C>::
        _start_element (const ro_string<C>& ns,
                        const ro_string<C>& name)
        {
          if (!_start_element_impl (ns, name))
            _start_any_element (ns, name);
        }

        template <typename C>
        void empty_content<C>::
        _end_element (const ro_string<C>& ns,
                      const ro_string<C>& name)
        {
          if (!_end_element_impl (ns, name))
            _end_any_element (ns, name);
        }

        template <typename C>
        void empty_content<C>::
        _attribute (const ro_string<C>& ns,
                    const ro_string<C>& name,
                    const ro_string<C>& value)
        {
          // Weed out special attributes: xsi:type, xsi:nil,
          // xsi:schemaLocation and noNamespaceSchemaLocation.
          // See section 3.2.7 in Structures for details.
          //
          if (ns == xml::bits::xsi_namespace<C> () &&
              (name == xml::bits::type<C> () ||
               name == xml::bits::nil<C> () ||
               name == xml::bits::schema_location<C> () ||
               name == xml::bits::no_namespace_schema_location<C> ()))
            return;

          // Also some parsers (notably Xerces-C++) supplies us with
          // namespace-prefix mapping attributes.
          //
          if (ns == xml::bits::xmlns_namespace<C> ())
            return;

          if (!_attribute_impl (ns, name, value))
            _any_attribute (ns, name, value);
        }

        template <typename C>
        void empty_content<C>::
        _characters (const ro_string<C>& s)
        {
          if (!_characters_impl (s))
            _any_characters (s);
        }


        // simple_content
        //

        template <typename C>
        void simple_content<C>::
        _attribute (const ro_string<C>& ns,
                    const ro_string<C>& name,
                    const ro_string<C>& value)
        {
          // Weed out special attributes: xsi:type, xsi:nil,
          // xsi:schemaLocation and xsi:noNamespaceSchemaLocation.
          // See section 3.2.7 in Structures for details.
          //
          if (ns == xml::bits::xsi_namespace<C> () &&
              (name == xml::bits::type<C> () ||
               name == xml::bits::nil<C> () ||
               name == xml::bits::schema_location<C> () ||
               name == xml::bits::no_namespace_schema_location<C> ()))
            return;

          // Also some parsers (notably Xerces-C++) supplies us with
          // namespace-prefix mapping attributes.
          //
          if (ns == xml::bits::xmlns_namespace<C> ())
            return;

          if (!_attribute_impl (ns, name, value))
            _any_attribute (ns, name, value);
        }

        template <typename C>
        void simple_content<C>::
        _characters (const ro_string<C>& str)
        {
          _characters_impl (str);
        }


        // complex_content
        //

        template <typename C>
        void complex_content<C>::
        _start_element (const ro_string<C>& ns,
                        const ro_string<C>& name)
        {
          state& s (context_.top ());

          if (s.depth_++ > 0)
          {
            if (s.any_)
              _start_any_element (ns, name);
            else if (s.parser_)
              s.parser_->_start_element (ns, name);
          }
          else
          {
            if (!_start_element_impl (ns, name))
            {
              _start_any_element (ns, name);
              s.any_ = true;
            }
            else if (s.parser_ != 0)
              s.parser_->_pre_impl ();
          }
        }

        template <typename C>
        void complex_content<C>::
        _end_element (const ro_string<C>& ns,
                      const ro_string<C>& name)
        {
          // To understand what's going on here it is helpful to think of
          // a "total depth" as being the sum of individual depths over
          // all elements.
          //

          if (context_.top ().depth_ == 0)
          {
            state& s (context_.under_top ()); // One before last.

            if (--s.depth_ > 0)
            {
              // Indirect recursion.
              //
              if (s.parser_)
                s.parser_->_end_element (ns, name);
            }
            else
            {
              // Direct recursion.
              //
              assert (this == s.parser_);

              this->_post_impl ();

              if (!_end_element_impl (ns, name))
                assert (false);
            }
          }
          else
          {
            state& s (context_.top ());

            if (--s.depth_ > 0)
            {
              if (s.any_)
                _end_any_element (ns, name);
              else if (s.parser_)
                s.parser_->_end_element (ns, name);
            }
            else
            {
              if (s.parser_ != 0 && !s.any_)
                s.parser_->_post_impl ();

              if (!_end_element_impl (ns, name))
              {
                s.any_ = false;
                _end_any_element (ns, name);
              }
            }
          }
        }

        template <typename C>
        void complex_content<C>::
        _attribute (const ro_string<C>& ns,
                    const ro_string<C>& name,
                    const ro_string<C>& value)
        {
          // Weed out special attributes: xsi:type, xsi:nil,
          // xsi:schemaLocation and xsi:noNamespaceSchemaLocation.
          // See section 3.2.7 in Structures for details.
          //
          if (ns == xml::bits::xsi_namespace<C> () &&
              (name == xml::bits::type<C> () ||
               name == xml::bits::nil<C> () ||
               name == xml::bits::schema_location<C> () ||
               name == xml::bits::no_namespace_schema_location<C> ()))
            return;

          // Also some parsers (notably Xerces-C++) supplies us with
          // namespace-prefix mapping attributes.
          //
          if (ns == xml::bits::xmlns_namespace<C> ())
            return;

          state& s (context_.top ());

          if (s.depth_ > 0)
          {
            if (s.any_)
              _any_attribute (ns, name, value);
            else if (s.parser_)
              s.parser_->_attribute (ns, name, value);
          }
          else
          {
            if (!_attribute_impl (ns, name, value))
              _any_attribute (ns, name, value);
          }
        }

        template <typename C>
        void complex_content<C>::
        _characters (const ro_string<C>& str)
        {
          state& s (context_.top ());

          if (s.depth_ > 0)
          {
            if (s.any_)
              _any_characters (str);
            else if (s.parser_)
              s.parser_->_characters (str);
          }
          else
          {
            if (!_characters_impl (str))
              _any_characters (str);
          }
        }

        template <typename C>
        void complex_content<C>::
        _pre_impl ()
        {
          context_.push (state ());
          this->_pre ();
        }

        template <typename C>
        void complex_content<C>::
        _post_impl ()
        {
          this->_post ();
          context_.pop ();
        }
      }
    }
  }
}
