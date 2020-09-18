/* dtn-config.h.  Generated from dtn-config.h.in by configure.  */
/* dtn-config.h.in.  Generated from configure.ac by autoheader.  */

/* whether Aggregate Custody Signals support is enabled */
#define ACS_ENABLED 1

/* whether BBN IPND discovery protocol is enabled */
/* #undef BBN_IPND_ENABLED */

/* whether BundleDaemon Statistics support is enabled */
/* #undef BDSTATS_ENABLED */

/* whether Bundle Protocol Query Extension support is enabled */
/* #undef BPQ_ENABLED */

/* whether Bundle Security Protocol support is enabled */
/* #undef BSP_ENABLED */

/* Bundle ID size */
#define BUNDLEID_SIZE 64

/* Max value of the Bundle ID */
#define BUNDLE_ID_MAX __UINT64_C(0xffffffffffffffff)

/* 64 bit version of the max Bundle ID value */
#define BUNDLE_ID_MAX64 BUNDLE_ID_MAX

/* always defined so code can ensure that dtn-config.h is properly included */
#define DTN_CONFIG_STATE 1

/* whether Delay Tolerant Payload Conditioning support is enabled */
/* #undef DTPC_ENABLED */

/* whether Extend Class Of Service Block support is enabled */
#define ECOS_ENABLED 1

/* whether to build the EHS External Router */
/* #undef EHSROUTER_ENABLED */

/* whether external convergence layer support is enabled */
/* #undef EXTERNAL_CL_ENABLED */

/* whether external decision plane support is enabled */
/* #undef EXTERNAL_DP_ENABLED */

/* Define to 1 if you have the <dns_sd.h> header file. */
/* #undef HAVE_DNS_SD_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `crypto' library (-lcrypto). */
/* #undef HAVE_LIBCRYPTO */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* directory for local state (default /var) */
#define INSTALL_LOCALSTATEDIR "/var"

/* directory for config files (default /etc) */
#define INSTALL_SYSCONFDIR "/etc"

/* whether odbc is enabled */
/* #undef LIBODBC_ENABLED */

/* whether LTPUDP Authentication support is enabled */
/* #undef LTPUDP_AUTH_ENABLED */

/* whether LTPUDP Authentication support is enabled */
#define LTPUDP_ENABLED 1

/* whether LTP support is enabled */
/* #undef LTP_ENABLED */

/* whether norm support is enabled */
/* #undef NORM_ENABLED */

/* whether bonjour support is enabled */
/* #undef OASYS_BONJOUR_ENABLED */

/* whether oasys lock debugging is enabled */
/* #undef OASYS_DEBUG_LOCKING_ENABLED */

/* whether oasys memory debugging is enabled */
/* #undef OASYS_DEBUG_MEMORY_ENABLED */

/* whether oasys log_debug is enabled */
/* #undef OASYS_LOG_DEBUG_ENABLED */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Macro for printing a Bundle ID */
#define PRIbid PRIu64

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Bundle ID is 64 bits */
#define bundleid_t u_int64_t

/* Enable inclusion of the STD C Macros */
#ifndef __STDC_FORMAT_MACROS
    #define __STDC_FORMAT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
    #define __STDC_LIMIT_MACROS
#endif
#include <inttypes.h>

/* Include oasys configuration state */
#include <oasys/oasys-config.h>
