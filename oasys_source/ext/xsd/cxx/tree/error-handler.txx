// file      : xsd/cxx/tree/error-handler.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      template <typename C>
      bool error_handler<C>::
      handle (const std::basic_string<C>& id,
              unsigned long line,
              unsigned long column,
              severity s,
              const std::basic_string<C>& message)
      {
        if (s == severity::error || s == severity::fatal)
          errors_.push_back (error<C> (id, line, column, message));

        return true;
      }
    }
  }
}
