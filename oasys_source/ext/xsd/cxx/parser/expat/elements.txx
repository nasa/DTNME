// file      : xsd/cxx/parser/expat/elements.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#include <istream>
#include <fstream>
#include <cstring> // std::strchr

#include <xsd/cxx/parser/error-handler.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      namespace expat
      {

        // document_common
        //

        template <typename C>
        document_common<C>::
        document_common (parser_base<C>& parser,
                         const std::basic_string<C>& ns,
                         const std::basic_string<C>& name)
            : doc_ (parser, ns, name), router_ (doc_), xml_parser_ (0)
        {
        }


        // file
        //

        template <typename C>
        void document_common<C>::
        parse_ (const std::basic_string<C>& file)
        {
          std::ifstream ifs;
          ifs.exceptions (std::ios_base::badbit | std::ios_base::failbit);
          ifs.open (file.c_str (), std::ios_base::in | std::ios_base::binary);

          parse_ (ifs, file);
        }

        template <typename C>
        void document_common<C>::
        parse_ (const std::basic_string<C>& file, xml::error_handler<C>& eh)
        {
          std::ifstream ifs;
          ifs.exceptions (std::ios_base::badbit | std::ios_base::failbit);
          ifs.open (file.c_str (), std::ios_base::in | std::ios_base::binary);

          parse_ (ifs, file, eh);
        }


        // istream
        //

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is)
        {
          error_handler<C> eh;
          parse_ (is, 0, 0, eh);
          eh.throw_if_failed ();
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is, xml::error_handler<C>& eh)
        {
          if (!parse_ (is, 0, 0, eh))
            throw parsing<C> ();
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is, const std::basic_string<C>& system_id)
        {
          error_handler<C> eh;
          parse_ (is, &system_id, 0, eh);
          eh.throw_if_failed ();
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                xml::error_handler<C>& eh)
        {
          if (!parse_ (is, &system_id, 0, eh))
            throw parsing<C> ();
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                const std::basic_string<C>& public_id)
        {
          error_handler<C> eh;
          parse_ (is, &system_id, &public_id, eh);
          eh.throw_if_failed ();
        }

        template <typename C>
        void document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>& system_id,
                const std::basic_string<C>& public_id,
                xml::error_handler<C>& eh)
        {
          if (!parse_ (is, &system_id, &public_id, eh))
            throw parsing<C> ();
        }

        namespace bits
        {
          struct stream_exception_controller
          {
            ~stream_exception_controller ()
            {
              is_.clear (is_.rdstate () & ~clear_state_);
              is_.exceptions (old_state_);
            }

            stream_exception_controller (std::istream& is,
                                         std::ios_base::iostate except_state,
                                         std::ios_base::iostate clear_state)
                : is_ (is),
                  old_state_ (is_.exceptions ()),
                  clear_state_ (clear_state)
            {
              is_.exceptions (except_state);
            }

          private:
            std::istream& is_;
            std::ios_base::iostate old_state_;
            std::ios_base::iostate clear_state_;
          };
        };

        template <typename C>
        bool document_common<C>::
        parse_ (std::istream& is,
                const std::basic_string<C>* system_id,
                const std::basic_string<C>* public_id,
                xml::error_handler<C>& eh)
        {
          parser_auto_ptr parser (XML_ParserCreateNS (0, XML_Char (' ')));

          // @@ TODO: Set base.
          //

          parse_begin_ (parser);

          // Temporarily unset exception failbit. Also clear failbit
          // when we reset the old state.
          //
          bits::stream_exception_controller sec (
            is,
            is.exceptions () & ~std::ios_base::failbit,
            std::ios_base::failbit);

          const std::size_t buf_size = 10240;
          char buf[buf_size];

          do
          {
            is.read (buf, sizeof (buf));

            if (XML_Parse (
                  parser, buf, is.gcount (), is.eof ()) == XML_STATUS_ERROR)
            {
              eh.handle (
                public_id
                ? *public_id
                : (system_id ? *system_id : std::basic_string<C> ()),
                static_cast<unsigned long> (
                  XML_GetCurrentLineNumber (parser)),
                static_cast<unsigned long> (
                  XML_GetCurrentColumnNumber (parser)),
                xml::error_handler<C>::severity::fatal,
                XML_ErrorString (XML_GetErrorCode (parser)));

              return false;
            }
          } while (!is.eof ());

          parse_end_ ();

          return true;
        }


        // XML_Parser
        //

        template <typename C>
        void document_common<C>::
        parse_begin_ (XML_Parser parser)
        {
          xml_parser_ = parser;
          set ();
        }

        template <typename C>
        void document_common<C>::
        parse_end_ ()
        {
          clear ();
        }

        //
        //
        template <typename C>
        void document_common<C>::
        set ()
        {
          assert (xml_parser_ != 0);

          XML_SetUserData(xml_parser_, &router_);

          XML_SetStartElementHandler (
            xml_parser_, event_router<C>::start_element);
          XML_SetEndElementHandler (
            xml_parser_, event_router<C>::end_element);
          XML_SetCharacterDataHandler (
            xml_parser_, event_router<C>::characters);
        }

        template <typename C>
        void document_common<C>::
        clear ()
        {
          assert (xml_parser_ != 0);

          XML_SetUserData (xml_parser_, 0);
          XML_SetStartElementHandler (xml_parser_, 0);
          XML_SetEndElementHandler (xml_parser_, 0);
          XML_SetCharacterDataHandler (xml_parser_, 0);
        }


        // document
        //

        template <typename X, typename C>
        document<X, C>::
        document (parser<X, C>& p, const std::basic_string<C>& name)
            : document_common<C> (p, std::basic_string<C> (), name),
              parser_ (p)
        {
        }

        template <typename X, typename C>
        document<X, C>::
        document (parser<X, C>& p,
                  const std::basic_string<C>& ns,
                  const std::basic_string<C>& name)
            : document_common<C> (p, ns, name), parser_ (p)
        {
        }


        // file
        //

        template <typename X, typename C>
        X document<X, C>::
        parse (const std::basic_string<C>& file)
        {
          parse_ (file);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (const std::basic_string<C>& file, xml::error_handler<C>& eh)
        {
          parse_ (file, eh);
          return parser_.post ();
        }


        // istream


        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is)
        {
          this->parse_ (is);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is, xml::error_handler<C>& eh)
        {
          parse_ (is, eh);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is, const std::basic_string<C>& system_id)
        {
          parse_ (is, system_id);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& system_id,
               xml::error_handler<C>& eh)
        {
          parse_ (is, system_id, eh);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& system_id,
               const std::basic_string<C>& public_id)
        {
          parse_ (is, system_id, public_id);
          return parser_.post ();
        }

        template <typename X, typename C>
        X document<X, C>::
        parse (std::istream& is,
               const std::basic_string<C>& system_id,
               const std::basic_string<C>& public_id,
               xml::error_handler<C>& eh)
        {
          parse_ (is, system_id, public_id, eh);
          return parser_.post ();
        }


        // XML_Parser


        template <typename X, typename C>
        void document<X, C>::
        parse_begin (XML_Parser parser)
        {
          this->parse_begin_ (parser);
        }

        template <typename X, typename C>
        X document<X, C>::
        parse_end ()
        {
          XML_Error e (XML_GetErrorCode (this->xml_parser_));

          this->parse_end_ ();

          if (e != XML_ERROR_NONE)
            throw parsing<C> ();

          return parser_.post ();
        }


        // document<void>
        //

        template <typename C>
        document<void, C>::
        document (parser<void, C>& p, const std::basic_string<C>& name)
            : document_common<C> (p, std::basic_string<C> (), name),
              parser_ (p)
        {
        }

        template <typename C>
        document<void, C>::
        document (parser<void, C>& p,
                  const std::basic_string<C>& ns,
                  const std::basic_string<C>& name)
            : document_common<C> (p, ns, name), parser_ (p)
        {
        }

        // file


        template <typename C>
        void document<void, C>::
        parse (const std::basic_string<C>& file)
        {
          parse_ (file);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (const std::basic_string<C>& file, xml::error_handler<C>& eh)
        {
          parse_ (file, eh);
          parser_.post ();
        }


        // istream


        template <typename C>
        void document<void, C>::
        parse (std::istream& is)
        {
          this->parse_ (is);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is, xml::error_handler<C>& eh)
        {
          parse_ (is, eh);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is, const std::basic_string<C>& system_id)
        {
          parse_ (is, system_id);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& system_id,
               xml::error_handler<C>& eh)
        {
          parse_ (is, system_id, eh);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& system_id,
               const std::basic_string<C>& public_id)
        {
          parse_ (is, system_id, public_id);
          parser_.post ();
        }

        template <typename C>
        void document<void, C>::
        parse (std::istream& is,
               const std::basic_string<C>& system_id,
               const std::basic_string<C>& public_id,
               xml::error_handler<C>& eh)
        {
          parse_ (is, system_id, public_id, eh);
          parser_.post ();
        }


        // XML_Parser


        template <typename C>
        void document<void, C>::
        parse_begin (XML_Parser parser)
        {
          this->parse_begin_ (parser);
        }

        template <typename C>
        void document<void, C>::
        parse_end ()
        {
          XML_Error e (XML_GetErrorCode (this->xml_parser_));

          this->parse_end_ ();

          if (e != XML_ERROR_NONE)
            throw parsing<C> ();

          parser_.post ();
        }


        // event_router
        //

        template <typename C>
        event_router<C>::
        event_router (event_consumer<C>& consumer)
            : consumer_ (consumer)
        {
        }

        // Expat thunks.
        //
        template <typename C>
        void XMLCALL event_router<C>::
        start_element (
          void* data, const XML_Char* ns_name, const XML_Char** atts)
        {
          event_router& r (*reinterpret_cast<event_router*> (data));
          r.start_element_ (ns_name, atts);
        }

        template <typename C>
        void XMLCALL event_router<C>::
        end_element (void* data, const XML_Char* ns_name)
        {
          event_router& r (*reinterpret_cast<event_router*> (data));
          r.end_element_ (ns_name);
        }

        template <typename C>
        void XMLCALL event_router<C>::
        characters (void* data, const XML_Char* s, int n)
        {
          event_router& r (*reinterpret_cast<event_router*> (data));
          r.characters_ (s, static_cast<std::size_t> (n));
        }

        namespace bits
        {
          inline void
          split_name (const XML_Char* s,
                      const char*& ns, std::size_t& ns_s,
                      const char*& name, std::size_t& name_s)
          {
            const char* p (std::strchr (s, ' '));

            if (p)
            {
              ns = s;
              ns_s = p - s;
              name = p + 1;
            }
            else
            {
              ns = s;
              ns_s = 0;
              name = s;
            }

            name_s = std::char_traits<char>::length (name);
          }
        }

        template <typename C>
        void event_router<C>::
        start_element_ (const XML_Char* ns_name, const XML_Char** atts)
        {
          const char* ns_p;
          const char* name_p;
          size_t ns_s, name_s;

          bits::split_name (ns_name, ns_p, ns_s, name_p, name_s);

          {
            const ro_string<C> ns (ns_p, ns_s), name (name_p, name_s);
            consumer_.start_element (ns, name);
          }

          for (; *atts != 0; atts += 2)
          {
            bits::split_name (*atts, ns_p, ns_s, name_p, name_s);

            const ro_string<C> ns (ns_p, ns_s), name (name_p, name_s);
            const ro_string<C> value (*(atts + 1));

            consumer_.attribute (ns, name, value);
          }
        }

        template <typename C>
        void event_router<C>::
        end_element_ (const XML_Char* ns_name)
        {
          const char* ns_p;
          const char* name_p;
          size_t ns_s, name_s;

          bits::split_name (ns_name, ns_p, ns_s, name_p, name_s);

          const ro_string<C> ns (ns_p, ns_s), name (name_p, name_s);

          consumer_.end_element (ns, name);
        }

        template <typename C>
        void event_router<C>::
        characters_ (const XML_Char* s, std::size_t n)
        {
          if (n != 0)
          {
            const ro_string<C> str (s, n);
            consumer_.characters (str);
          }
        }
      }
    }
  }
}
