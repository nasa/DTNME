// file      : xsd/cxx/tree/ace-cdr-stream-common.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2007 Code Synthesis Tools CC
// license   : GNU GPL v2 + exceptions; see accompanying LICENSE file

#ifndef XSD_CXX_TREE_ACE_CDR_STREAM_COMMON_HXX
#define XSD_CXX_TREE_ACE_CDR_STREAM_COMMON_HXX

#include <xsd/cxx/exceptions.hxx>

namespace xsd
{
  namespace cxx
  {
    namespace tree
    {
      struct ace_cdr_stream_operation: virtual xsd::cxx::exception
      {
        virtual const char*
        what () const throw ()
        {
          return "ACE CDR stream operation failed";
        }
      };
    }
  }
}

#endif  // XSD_CXX_TREE_ACE_CDR_STREAM_COMMON_HXX
