// file      : xsd/cxx/parser/validating/parser.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_PARSER_VALIDATING_PARSER_HXX
#define XSD_CXX_PARSER_VALIDATING_PARSER_HXX

#include <stack>
#include <cstdlib> // std::size_t

#include <xsd/cxx/parser/ro-string.hxx>
#include <xsd/cxx/parser/elements.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace parser
    {
      namespace validating
      {
        //
        //
        template <typename C>
        struct empty_content: virtual parser_base<C>
        {
          // These functions are called when wildcard content
          // is encountered. Use them to handle mixed content
          // models, any/anyAttribute, and anyType/anySimpleType.
          // By default these functions do nothing.
          //
          virtual void
          _start_any_element (const ro_string<C>& ns,
                              const ro_string<C>& name);

          virtual void
          _end_any_element (const ro_string<C>& ns,
                            const ro_string<C>& name);

          virtual void
          _any_attribute (const ro_string<C>& ns,
                          const ro_string<C>& name,
                          const ro_string<C>& value);

          virtual void
          _any_characters (const ro_string<C>&);


          //
          //
          virtual bool
          _start_element_impl (const ro_string<C>&,
                               const ro_string<C>&);

          virtual bool
          _end_element_impl (const ro_string<C>&,
                             const ro_string<C>&);

          virtual bool
          _attribute_impl (const ro_string<C>&,
                           const ro_string<C>&,
                           const ro_string<C>&);

          virtual bool
          _characters_impl (const ro_string<C>&);


          //
          //
          virtual void
          _start_element (const ro_string<C>&,
                          const ro_string<C>&);

          virtual void
          _end_element (const ro_string<C>&,
                        const ro_string<C>&);

          virtual void
          _attribute (const ro_string<C>&,
                      const ro_string<C>&,
                      const ro_string<C>&);

          virtual void
          _characters (const ro_string<C>&);


          //
          //
          virtual void
          _expected_element (const C* expected_ns,
                             const C* expected_name);

          virtual void
          _expected_element (const C* expected_ns,
                             const C* expected_name,
                             const ro_string<C>& encountered_ns,
                             const ro_string<C>& encountered_name);

          virtual void
          _unexpected_element (const ro_string<C>& ns,
                               const ro_string<C>& name);

          virtual void
          _expected_attribute (const C* expected_ns,
                               const C* expected_name);

          virtual void
          _unexpected_attribute (const ro_string<C>& ns,
                                 const ro_string<C>& name,
                                 const ro_string<C>& value);

          virtual void
          _unexpected_characters (const ro_string<C>&);
        };


        //
        //
        template <typename C>
        struct simple_content: virtual empty_content<C>
        {
          //
          //
          virtual void
          _attribute (const ro_string<C>& ns,
                      const ro_string<C>& name,
                      const ro_string<C>& value);

          virtual void
          _characters (const ro_string<C>&);

          //
          //
          virtual bool
          _attribute_impl (const ro_string<C>&,
                           const ro_string<C>&,
                           const ro_string<C>&);

          //
          //
          virtual void
          _pre_impl ();

          virtual void
          _post_impl ();


          // Implementation hooks.
          //
          virtual void
          _pre_a_validate ();

          virtual void
          _post_a_validate ();


          // Attribute validation: during phase one we are searching for
          // matching attributes (Structures, section 3.4.4, clause 2.1).
          // During phase two we are searching for attribute wildcards
          // (section 3.4.4, clause 2.2). Both phases run across
          // inheritance hierarchy from derived to base for extension
          // only. Both functions return true if the match was found and
          // validation has been performed.
          //
          virtual bool
          _attribute_impl_phase_one (const ro_string<C>& ns,
                                     const ro_string<C>& name,
                                     const ro_string<C>& value);

          virtual bool
          _attribute_impl_phase_two (const ro_string<C>& ns,
                                     const ro_string<C>& name,
                                     const ro_string<C>& value);
        };


        //
        //
        template <typename C>
        struct complex_content: virtual empty_content<C>
        {
          //
          //
          virtual void
          _start_element (const ro_string<C>& ns,
                          const ro_string<C>& name);

          virtual void
          _end_element (const ro_string<C>& ns,
                        const ro_string<C>& name);

          virtual void
          _attribute (const ro_string<C>& ns,
                      const ro_string<C>& name,
                      const ro_string<C>& value);

          virtual void
          _characters (const ro_string<C>&);

          //
          //
          virtual bool
          _attribute_impl (const ro_string<C>&,
                           const ro_string<C>&,
                           const ro_string<C>&);

          //
          //
          virtual void
          _pre_impl ();

          virtual void
          _post_impl ();


          // Implementation hooks.
          //
          virtual void
          _pre_e_validate ();

          virtual void
          _post_e_validate ();

          virtual void
          _pre_a_validate ();

          virtual void
          _post_a_validate ();


          // Attribute validation: during phase one we are searching for
          // matching attributes (Structures, section 3.4.4, clause 2.1).
          // During phase two we are searching for attribute wildcards
          // (section 3.4.4, clause 2.2). Both phases run across
          // inheritance hierarchy from derived to base for extension
          // only. Both functions return true if the match was found and
          // validation has been performed.
          //
          virtual bool
          _attribute_impl_phase_one (const ro_string<C>& ns,
                                     const ro_string<C>& name,
                                     const ro_string<C>& value);

          virtual bool
          _attribute_impl_phase_two (const ro_string<C>& ns,
                                     const ro_string<C>& name,
                                     const ro_string<C>& value);
        protected:
          struct state
          {
            state ()
                : any_ (false), depth_ (0), parser_ (0)
            {
            }

            bool any_;
            std::size_t depth_;
            parser_base<C>* parser_;
          };

          // Optimized state stack for non-recursive case (one element).
          //
          struct state_stack
          {
            state_stack ()
                : size_ (0)
            {
            }

            void
            push (const state& s)
            {
              if (size_ > 0)
                rest_.push (top_);

              top_ = s;
              ++size_;
            }

            void
            pop ()
            {
              if (size_ > 1)
              {
                top_ = rest_.top ();
                rest_.pop ();
              }

              --size_;
            }

            const state&
            top () const
            {
              return top_;
            }

            state&
            top ()
            {
              return top_;
            }

            state&
            under_top ()
            {
              return rest_.top ();
            }

          private:
            state top_;
            std::stack<state> rest_;
            std::size_t size_;
          };

          state_stack context_;
        };

        //
        //
        template <typename X, unsigned long N>
        struct fixed_size_stack
        {
          fixed_size_stack ()
              : size_ (0)
          {
          }

          void
          push (const X& x)
          {
            data_[size_++] = x;
          }

          void
          pop ()
          {
            --size_;
          }

          const X&
          top () const
          {
            return data_[size_ - 1];
          }

          X&
          top ()
          {
            return data_[size_ - 1];
          }

        private:
          X data_[N];
          unsigned long size_;
        };


        //
        //
        template <typename T, unsigned long N, typename C>
        struct state_stack
        {
          typedef void (T::*func_type) (unsigned long&,
                                        unsigned long&,
                                        const ro_string<C>&,
                                        const ro_string<C>&,
                                        bool);

          struct state_type
          {
            state_type ()
                : function (0), state (0), count (0)
            {
            }

            state_type (func_type f, unsigned long s, unsigned long c)
                : function (f), state (s), count (c)
            {
            }

            func_type function;
            unsigned long state;
            unsigned long count;
          };

        public:
          state_stack ()
              : size_ (0)
          {
          }

          void
          push (func_type f, unsigned long s, unsigned long c)
          {
            top_.push (state_type (f, s, c));
          }

          void
          pop ()
          {
            top_.pop ();
          }

          state_type&
          top ()
          {
            return top_.top ();
          }

          // The stack is optimized for non-recursive case (one element).
          //
        public:
          void
          push_stack ()
          {
            if (size_ > 0)
              rest_.push (top_);

            top_ = state_stack_ ();
            ++size_;
          }

          void
          pop_stack ()
          {
            if (size_ > 1)
            {
              top_ = rest_.top ();
              rest_.pop ();
            }

            --size_;
          }

        private:
          //typedef std::stack<state_type> state_stack_;
          typedef fixed_size_stack<state_type, N> state_stack_;

          state_stack_ top_;
          std::stack<state_stack_> rest_;
          std::size_t size_;
        };

        template <unsigned long N>
        struct all_count
        {
          struct counter
          {
            counter ()
            {
              for (unsigned long i (0); i < N; ++i)
                data_[i] = 0;
            }

            unsigned char data_[N];
          };

        public:
          all_count ()
              : size_ (0)
          {
          }


          // The stack is optimized for non-recursive case (one element).
          //
        public:
          void
          push ()
          {
            if (size_ > 0)
              rest_.push (top_);

            top_ = counter ();
            ++size_;

          }

          void
          pop ()
          {
            if (size_ > 1)
            {
              top_ = rest_.top ();
              rest_.pop ();
            }

            --size_;
          }

          unsigned char*
          top ()
          {
            return top_.data_;
          }

        private:
          counter top_;
          std::stack<counter> rest_;
          std::size_t size_;
        };
      }
    }
  }
}

#include <xsd/cxx/parser/validating/parser.txx>

#endif  // XSD_CXX_PARSER_VALIDATING_PARSER_HXX
