// file      : xsd/cxx/parser/exceptions.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      // error
      //
      template <typename C>
      error<C>::
      error (const std::basic_string<C>& id,
             unsigned long line,
             unsigned long column,
             const std::basic_string<C>& message)
          : id_ (id), line_ (line), column_ (column), message_ (message)
      {
      }


      // parsing
      //
      template <typename C>
      parsing<C>::
      ~parsing () throw ()
      {
      }

      template <typename C>
      parsing<C>::
      parsing ()
      {
      }

      template <typename C>
      parsing<C>::
      parsing (const cxx::parser::errors<C>& errors)
          : errors_ (errors)
      {
      }

      template <typename C>
      const char* parsing<C>::
      what () const throw ()
      {
        return "instance document parsing failed";
      }


      // expected_element
      //
      template <typename C>
      expected_element<C>::
      ~expected_element () throw ()
      {
      }

      template <typename C>
      expected_element<C>::
      expected_element (const std::basic_string<C>& expected_namespace,
                        const std::basic_string<C>& expected_name)
          : expected_namespace_ (expected_namespace),
            expected_name_ (expected_name)
      {
      }

      template <typename C>
      expected_element<C>::
      expected_element (const std::basic_string<C>& expected_namespace,
                        const std::basic_string<C>& expected_name,
                        const std::basic_string<C>& encountered_namespace,
                        const std::basic_string<C>& encountered_name)
          : expected_namespace_ (expected_namespace),
            expected_name_ (expected_name),
            encountered_namespace_ (encountered_namespace),
            encountered_name_ (encountered_name)
      {
      }

      template <typename C>
      const char* expected_element<C>::
      what () const throw ()
      {
        return "expected element not encountered";
      }


      // unexpected_element
      //
      template <typename C>
      unexpected_element<C>::
      ~unexpected_element () throw ()
      {
      }

      template <typename C>
      unexpected_element<C>::
      unexpected_element (const std::basic_string<C>& encountered_namespace,
                          const std::basic_string<C>& encountered_name)
          : encountered_namespace_ (encountered_namespace),
            encountered_name_ (encountered_name)
      {
      }

      template <typename C>
      const char* unexpected_element<C>::
      what () const throw ()
      {
        return "unexpected element encountered";
      }


      // expected_attribute
      //
      template <typename C>
      expected_attribute<C>::
      ~expected_attribute () throw ()
      {
      }

      template <typename C>
      expected_attribute<C>::
      expected_attribute (const std::basic_string<C>& expected_namespace,
                          const std::basic_string<C>& expected_name)
          : expected_namespace_ (expected_namespace),
            expected_name_ (expected_name)
      {
      }

      template <typename C>
      const char* expected_attribute<C>::
      what () const throw ()
      {
        return "expected attribute not encountered";
      }


      // unexpected_attribute
      //
      template <typename C>
      unexpected_attribute<C>::
      ~unexpected_attribute () throw ()
      {
      }

      template <typename C>
      unexpected_attribute<C>::
      unexpected_attribute (const std::basic_string<C>& encountered_namespace,
                            const std::basic_string<C>& encountered_name)
          : encountered_namespace_ (encountered_namespace),
            encountered_name_ (encountered_name)
      {
      }

      template <typename C>
      const char* unexpected_attribute<C>::
      what () const throw ()
      {
        return "unexpected attribute encountered";
      }


      // unexpected_characters
      //
      template <typename C>
      unexpected_characters<C>::
      ~unexpected_characters () throw ()
      {
      }

      template <typename C>
      unexpected_characters<C>::
      unexpected_characters (const std::basic_string<C>& s)
          : characters_ (s)
      {
      }

      template <typename C>
      const char* unexpected_characters<C>::
      what () const throw ()
      {
        return "unexpected characters encountered";
      }


      // invalid_value
      //
      template <typename C>
      invalid_value<C>::
      ~invalid_value () throw ()
      {
      }

      template <typename C>
      invalid_value<C>::
      invalid_value (const C* type,
                     const std::basic_string<C>& value)
          : type_ (type), value_ (value)
      {
      }

      template <typename C>
      invalid_value<C>::
      invalid_value (const C* type,
                     const ro_string<C>& value)
          : type_ (type), value_ (value)
      {
      }

      template <typename C>
      invalid_value<C>::
      invalid_value (const std::basic_string<C>& type,
                     const std::basic_string<C>& value)
          : type_ (type), value_ (value)
      {
      }

      template <typename C>
      const char* invalid_value<C>::
      what () const throw ()
      {
        return "invalid value representation encountered";
      }
    }
  }
}
