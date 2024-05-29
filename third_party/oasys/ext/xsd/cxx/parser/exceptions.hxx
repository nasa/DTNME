// file      : xsd/cxx/parser/exceptions.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_PARSER_EXCEPTIONS_HXX
#define XSD_CXX_PARSER_EXCEPTIONS_HXX

#include <string>
#include <vector>
#include <ostream>

#include <xsd/cxx/exceptions.hxx>       // xsd::cxx::exception
#include <xsd/cxx/parser/ro-string.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      //
      //
      template <typename C>
      struct exception: virtual xsd::cxx::exception
      {
        friend
        std::basic_ostream<C>&
        operator<< (std::basic_ostream<C>& os, const exception& e)
        {
          e.print (os);
          return os;
        }

      protected:
        virtual void
        print (std::basic_ostream<C>&) const = 0;
      };


      //
      //
      template <typename C>
      struct error
      {
        error (const std::basic_string<C>& id,
               unsigned long line,
               unsigned long column,
               const std::basic_string<C>& message);

        const std::basic_string<C>&
        id () const
        {
          return id_;
        }

        unsigned long
        line () const
        {
          return line_;
        }

        unsigned long
        column () const
        {
          return column_;
        }

        const std::basic_string<C>&
        message () const
        {
          return message_;
        }

      private:
        std::basic_string<C> id_;
        unsigned long line_;
        unsigned long column_;
        std::basic_string<C> message_;
      };

      // See exceptions.ixx for operator<< (error).


      //
      //
      template <typename C>
      struct errors: std::vector<error<C> >
      {
      };

      // See exceptions.ixx for operator<< (errors).

      //
      //
      template <typename C>
      struct parsing: virtual exception<C>
      {
        virtual
        ~parsing () throw ();

        parsing ();

        parsing (const cxx::parser::errors<C>& errors);

        const cxx::parser::errors<C>&
        errors () const
        {
          return errors_;
        }

        virtual const char*
        what () const throw ();

      protected:
        virtual void
        print (std::basic_ostream<C>&) const;

      private:
        cxx::parser::errors<C> errors_;
      };

      //
      //
      template <typename C>
      struct expected_element: virtual exception<C>
      {
        virtual
        ~expected_element () throw ();

        expected_element (const std::basic_string<C>& expected_namespace,
                          const std::basic_string<C>& expected_name);

        expected_element (const std::basic_string<C>& expected_namespace,
                          const std::basic_string<C>& expected_name,
                          const std::basic_string<C>& encountered_namespace,
                          const std::basic_string<C>& encountered_name);

        const std::basic_string<C>&
        expected_namespace () const
        {
          return expected_namespace_;
        }

        const std::basic_string<C>&
        expected_name () const
        {
          return expected_name_;
        }

        // Encountered element namespace and name are empty if none
        // encountered.
        //
        const std::basic_string<C>&
        encountered_namespace () const
        {
          return encountered_namespace_;
        }

        const std::basic_string<C>&
        encountered_name () const
        {
          return encountered_name_;
        }

        virtual const char*
        what () const throw ();

      protected:
        virtual void
        print (std::basic_ostream<C>&) const;

      private:
        std::basic_string<C> expected_namespace_;
        std::basic_string<C> expected_name_;

        std::basic_string<C> encountered_namespace_;
        std::basic_string<C> encountered_name_;
      };


      //
      //
      template <typename C>
      struct unexpected_element: virtual exception<C>
      {
        virtual
        ~unexpected_element () throw ();

        unexpected_element (const std::basic_string<C>& encountered_namespace,
                            const std::basic_string<C>& encountered_name);

        const std::basic_string<C>&
        encountered_namespace () const
        {
          return encountered_namespace_;
        }

        const std::basic_string<C>&
        encountered_name () const
        {
          return encountered_name_;
        }

        virtual const char*
        what () const throw ();

      protected:
        virtual void
        print (std::basic_ostream<C>&) const;

      private:
        std::basic_string<C> encountered_namespace_;
        std::basic_string<C> encountered_name_;
      };


      //
      //
      template <typename C>
      struct expected_attribute: virtual exception<C>
      {
        virtual
        ~expected_attribute () throw ();

        expected_attribute (const std::basic_string<C>& expected_namespace,
                            const std::basic_string<C>& expected_name);

        const std::basic_string<C>&
        expected_namespace () const
        {
          return expected_namespace_;
        }

        const std::basic_string<C>&
        expected_name () const
        {
          return expected_name_;
        }

        virtual const char*
        what () const throw ();

      protected:
        virtual void
        print (std::basic_ostream<C>&) const;

      private:
        std::basic_string<C> expected_namespace_;
        std::basic_string<C> expected_name_;
      };

      //
      //
      template <typename C>
      struct unexpected_attribute: virtual exception<C>
      {
        virtual
        ~unexpected_attribute () throw ();

        unexpected_attribute (
          const std::basic_string<C>& encountered_namespace,
          const std::basic_string<C>& encountered_name);


        const std::basic_string<C>&
        encountered_namespace () const
        {
          return encountered_namespace_;
        }

        const std::basic_string<C>&
        encountered_name () const
        {
          return encountered_name_;
        }

        virtual const char*
        what () const throw ();

      protected:
        virtual void
        print (std::basic_ostream<C>&) const;

      private:
        std::basic_string<C> encountered_namespace_;
        std::basic_string<C> encountered_name_;
      };


      //
      //
      template <typename C>
      struct unexpected_characters: virtual exception<C>
      {
        virtual
        ~unexpected_characters () throw ();

        unexpected_characters (const std::basic_string<C>& s);

        const std::basic_string<C>&
        characters () const
        {
          return characters_;
        }

        virtual const char*
        what () const throw ();

      protected:
        virtual void
        print (std::basic_ostream<C>&) const;

      private:
        std::basic_string<C> characters_;
      };

      //
      //
      template <typename C>
      struct invalid_value: virtual exception<C>
      {
        virtual
        ~invalid_value () throw ();

        invalid_value (const C* type, const std::basic_string<C>& value);

        invalid_value (const C* type, const ro_string<C>& value);

        invalid_value (const std::basic_string<C>& type,
                       const std::basic_string<C>& value);

        const std::basic_string<C>&
        type () const
        {
          return type_;
        }

        const std::basic_string<C>&
        value () const
        {
          return value_;
        }

        virtual const char*
        what () const throw ();

      protected:
        virtual void
        print (std::basic_ostream<C>&) const;

      private:
        std::basic_string<C> type_;
        std::basic_string<C> value_;
      };
    }
  }
}

#include <xsd/cxx/parser/exceptions.txx>

#endif  // XSD_CXX_PARSER_EXCEPTIONS_HXX

#include <xsd/cxx/parser/exceptions.ixx>
