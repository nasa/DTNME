
// This header file defines the namespaces (if applicable) being used.
// You should #include this file right after your other includes as
// needed.
//
// So long as no name conflicts are generated as a result, you can
// theoretically include whatever namespaces you want in here, and
// you won't be required to fully qualify the attributes from that
// namespace each time you use them.  However, it is recommended
// that this be done only for the "std" namespace.

// NOTE: this is one of the very rare occasions where it is advisable
// to allow multiple inclusion of the contents of a header file.
//
// Since this header file defines nothing but namespace utilization,
// multiple inclusions are not only safe, they are sometimes an
// absolute necessity.  The whole point of this discussion is, of
// course, to explain why the following #ifndef is intentionally
// commented out, so someone won't mistakenly add it back in later.

// #ifndef __NAMESPACES_H__
// #define __NAMESPACES_H__

#include <cstdio>

#if (                                                                \
  ( defined(__sgi)  &&  defined(__STL_HAS_NAMESPACES) )  ||          \
  ( defined(__linux)  &&  defined(__STL_USE_NAMESPACES) )  ||        \
  ( defined(sun)  &&  ! defined(_RWSTD_NO_NAMESPACE) )  ||           \
  ( defined(USE_NAMESPACES) )  )

  using namespace std;

#if defined(_GLIBCXX_CSTDIO)

# if defined(__linux)
    using namespace __gnu_cxx;
# endif

#endif

#endif


// #endif  // __NAMESPACES_H__

