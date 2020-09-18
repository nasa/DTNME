dnl
dnl Autoconf support for Aggregate Custody Signals 
dnl

AC_DEFUN(AC_CONFIG_ACS, [
    ac_acs='yes'
    AC_ARG_WITH(acs,
        AC_HELP_STRING([--without-acs],
                [disable Aggregate Custody Signals support]),
        ac_acs=$withval)
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_acs" = no; then
        ACS_ENABLED=0
    else
        ACS_ENABLED=1
        AC_DEFINE_UNQUOTED(ACS_ENABLED, 1, [whether Aggregate Custody Signals support is enabled])
    fi # ACS_ENABLED
])
dnl
dnl Autoconf support for BundleDaemon Statistics
dnl

AC_DEFUN(AC_CONFIG_BDSTATS, [
    ac_bdstats='no'
    AC_ARG_WITH(bdstats,
        AC_HELP_STRING([--with-bdstats],
                [enable BundleDaemon Statistics support]),
        ac_bdstats=$withval)
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_bdstats" = no; then
        BDSTATS_ENABLED=0
    else
        BDSTATS_ENABLED=1
        with_extra_ldflags="$with_extra_ldflags -lrt"
        AC_DEFINE_UNQUOTED(BDSTATS_ENABLED, 1, [whether BundleDaemon Statistics support is enabled])
    fi # BDSTATS_ENABLED
])
dnl
dnl Autoconf support for Bundle Protocol Query Extension
dnl

AC_DEFUN(AC_CONFIG_BPQ, [
    ac_bpq='no'
    AC_ARG_WITH(bpq,
        AC_HELP_STRING([--with-bpq],
                [enable Bundle Protocol Query Extension support]),
        ac_bpq=$withval)
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_bpq" = no; then
        BPQ_ENABLED=0
    else
        BPQ_ENABLED=1
        AC_DEFINE_UNQUOTED(BPQ_ENABLED, 1, [whether Bundle Protocol Query Extension support is enabled])
    fi # BPQ_ENABLED
])dnl
dnl Autoconf support for Bundle Security Protocol
dnl

AC_DEFUN(AC_CONFIG_BSP, [
    ac_bsp='no'
    AC_ARG_WITH(bsp,
        AC_HELP_STRING([--with-bsp],
    		   	[enable Bundle Security Protocol support (EXPERIMENTAL)]),
        ac_bsp=$withval)
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_bsp" = no; then
        BSP_ENABLED=0
    else
        BSP_ENABLED=1
        AC_DEFINE_UNQUOTED(BSP_ENABLED, 1, [whether Bundle Security Protocol support is enabled])
    fi # BSP_ENABLED
])
dnl
dnl Autoconf support for specifying a 64bit Bundle ID 
dnl

AC_DEFUN(AC_CONFIG_BID64BIT, [
    ac_bid64bit='yes'
    AC_ARG_ENABLE(bid64bit,
        AC_HELP_STRING([--disable-bid64bit],
    		   	[set the size of the Bundle ID to 32 bits]),
        ac_bid64bit=$enableval) 
    dnl
    dnl First make sure we even want it
    dnl
    AC_MSG_CHECKING(whether to enable 64bit Bundle ID)
    if test "$ac_bid64bit" = no; then
        AC_MSG_RESULT(no)

        #BUNDLEID_SIZE=32
        #bundleid_t=u_int32_t
        #BUNDLE_ID_MAX=0xffffffff
        #BUNDLE_ID_MAX64=__UINT64_C(0x00000000ffffffff)
        #PRIbid=PRIu32
        AC_DEFINE_UNQUOTED(BUNDLEID_SIZE, 32, [Bundle ID size])
        AC_DEFINE_UNQUOTED(bundleid_t, u_int32_t, [Bundle ID is 32 bits])
        AC_DEFINE_UNQUOTED(BUNDLE_ID_MAX, 0xffffffff, [Max valus of the Bundle ID])
        AC_DEFINE_UNQUOTED(BUNDLE_ID_MAX64, __UINT64_C(0x00000000ffffffff), [64 bit version of the max Bundle ID value])
        AC_DEFINE_UNQUOTED(PRIbid, PRIu32, [Macro for printing a Bundle ID])
    else
        AC_MSG_RESULT(yes)

        #BUNDLEID_SIZE=64
        #bundleid_t=u_int64_t
        #BUNDLE_ID_MAX=__UINT64_C(0xffffffffffffffff)
        #BUNDLE_ID_MAX64=BUNDLE_ID_MAX
        #PRIbid=PRIu64
        AC_DEFINE_UNQUOTED(BUNDLEID_SIZE, 64, [Bundle ID size])
        AC_DEFINE_UNQUOTED(bundleid_t, u_int64_t, [Bundle ID is 64 bits])
        AC_DEFINE_UNQUOTED(BUNDLE_ID_MAX, __UINT64_C(0xffffffffffffffff), [Max value of the Bundle ID])
        AC_DEFINE_UNQUOTED(BUNDLE_ID_MAX64, BUNDLE_ID_MAX, [64 bit version of the max Bundle ID value])
        AC_DEFINE_UNQUOTED(PRIbid, PRIu64, [Macro for printing a Bundle ID])
    fi # BID64BIT
])
dnl
dnl Autoconf support for Delay Tolerant Payload Conditioning (DTPC)
dnl

AC_DEFUN(AC_CONFIG_DTPC, [
    ac_dtpc='no'
    AC_ARG_WITH(dtpc,
        AC_HELP_STRING([--with-dtpc],
                [enable DTPC - Delay Tolerant Payload Conditioning support]),
        ac_dtpc=$withval)
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_dtpc" = no; then
        DTPC_ENABLED=
    else
        DTPC_ENABLED=1
        AC_DEFINE_UNQUOTED(DTPC_ENABLED, 1, [whether Delay Tolerant Payload Conditioning support is enabled])
        AC_SUBST(DTPC_ENABLED)
    fi # DTPC_ENABLED
])
dnl
dnl Autoconf support for Extended Class Of Service Blocks
dnl

AC_DEFUN(AC_CONFIG_ECOS, [
    ac_ecos='yes'
    AC_ARG_WITH(ecos,
        AC_HELP_STRING([--without-ecos],
                [disable Extended Class Of Service Block support]),
        ac_ecos=$withval)
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_ecos" = no; then
        ECOS_ENABLED=0
    else
        ECOS_ENABLED=1
        AC_DEFINE_UNQUOTED(ECOS_ENABLED, 1, [whether Extend Class Of Service Block support is enabled])
    fi # ECOS_ENABLED
])
dnl
dnl Autoconf support for the EHS External Router
dnl
 AC_DEFUN(AC_CONFIG_EHSROUTER, [
     ac_ehsrouter='no'
     AC_ARG_WITH(ehsrouter,
         AC_HELP_STRING([--with-ehsrouter],
                 [enable building the EHS External Router which also requires Xerces C]),
         ac_ehsrouter=$withval)
     dnl
     dnl First make sure we even want it
     dnl
     if test "$ac_ehsrouter" = no; then
         EHSROUTER_ENABLED=0
     else
         EHSROUTER_ENABLED=1
         AC_DEFINE_UNQUOTED(EHSROUTER_ENABLED, 1, [whether to build the EHS External Router])
     fi # EHSROUTER_ENABLED
 ])
dnl
dnl Autoconf support for external convergence layer
dnl

AC_DEFUN(AC_CONFIG_EXTERNAL_CL, [
    ac_ecl='yes'
    AC_ARG_ENABLE(ecl,
        AC_HELP_STRING([--disable-ecl],
    		   	[disable external convergence layer support]),
        ac_ecl=$enableval) 
    dnl
    dnl First make sure we even want it
    dnl
    AC_MSG_CHECKING(whether to enable external convergence layer support)
    if test "$ac_ecl" = no; then
        AC_MSG_RESULT(no)
        EXTERNAL_CL_ENABLED=0
    else
        AC_MSG_RESULT(yes)

        AC_OASYS_SUPPORTS(XERCES_C_ENABLED)
	if test $ac_oasys_supports_result != yes ; then
	    AC_MSG_ERROR([external convergence layer support requires xerces... install it or configure --disable-ecl])
	fi

        EXTERNAL_CL_ENABLED=1
        AC_DEFINE_UNQUOTED(EXTERNAL_CL_ENABLED, 1, [whether external convergence layer support is enabled])
    fi # EXTERNAL_CL_ENABLED
])
dnl
dnl Autoconf support for external decision plane
dnl

AC_DEFUN(AC_CONFIG_EXTERNAL_DP, [
    ac_edp='yes'
    AC_ARG_ENABLE(edp,
        AC_HELP_STRING([--disable-edp],
    		   	[disable external decision plane support]),
        ac_edp=$enableval) 
    dnl
    dnl First make sure we even want it
    dnl
    AC_MSG_CHECKING(whether to enable external decision plane support)
    if test "$ac_edp" = no; then
        AC_MSG_RESULT(no)
        EXTERNAL_DP_ENABLED=0
    else
        AC_MSG_RESULT(yes)

        AC_OASYS_SUPPORTS(XERCES_C_ENABLED)
	if test $ac_oasys_supports_result != yes ; then
	    AC_MSG_ERROR([external decision plane support requires xerces... install it or configure --disable-edp])
	fi

        AC_DEFINE_UNQUOTED(EXTERNAL_DP_ENABLED, 1, [whether external decision plane support is enabled])
    fi # EXTERNAL_DP_ENABLED
])
dnl
dnl Autoconf support for BBN IPND
dnl

AC_DEFUN(AC_CONFIG_BBN_IPND, [
    ac_bbn_ipnd='no'
    AC_ARG_WITH(bbn-ipnd,
        AC_HELP_STRING([--with-bbn-ipnd],
                [enable BBN IPND discovery protocol (EXPERIMENTAL)]),
        ac_bbn_ipnd=$withval)
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_bbn_ipnd" = no; then
        BBN_IPND_ENABLED=0
    else
        BBN_IPND_ENABLED=1
        AC_DEFINE_UNQUOTED(BBN_IPND_ENABLED, 1, [whether BBN IPND discovery protocol is enabled])
        AC_SUBST(BBN_IPND_ENABLED)
    fi # BBN_IPND_ENABLED
])
dnl
dnl    Copyright 2005-2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl


#
# Macro based on AC_CHECK_LIB but which takes a particular
# LFLAGS setting as another argument to go into the cache.
#

# AC_CHECK_LIB_FLAGS(LIBRARY, FUNCTION, LDFLAGS,
#                    [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
#                    [OTHER-LIBRARIES])
# ------------------------------------------------------
#
# Use a cache variable name containing both the library and function name,
# because the test really is for library $1 defining function $2, not
# just for library $1.  Separate tests with the same $1 and different $2s
# may have different results.
#
# Note that using directly AS_VAR_PUSHDEF([ac_Lib], [ac_cv_lib_$1_$2])
# is asking for troubles, since AC_CHECK_LIB($lib, fun) would give
# ac_cv_lib_$lib_fun, which is definitely not what was meant.  Hence
# the AS_LITERAL_IF indirection.
#
# FIXME: This macro is extremely suspicious.  It DEFINEs unconditionally,
# whatever the FUNCTION, in addition to not being a *S macro.  Note
# that the cache does depend upon the function we are looking for.
#
# It is on purpose we used `ac_check_lib_save_LIBS' and not just
# `ac_save_LIBS': there are many macros which don't want to see `LIBS'
# changed but still want to use AC_CHECK_LIB, so they save `LIBS'.
# And ``ac_save_LIBS' is too tempting a name, so let's leave them some
# freedom.
AC_DEFUN([AC_CHECK_LIB_FLAGS],
[m4_ifval([$4], , [AH_CHECK_LIB([$1])])dnl
AS_LITERAL_IF([$1],
              [AS_VAR_PUSHDEF([ac_Lib], [ac_cv_lib_$1_$2_$3])],
              [AS_VAR_PUSHDEF([ac_Lib], [ac_cv_lib_$1''_$2_$3])])dnl
AC_CACHE_CHECK([for $2 in -l$1 with $3], ac_Lib,
[ac_check_lib_save_LIBS=$LIBS
ac_check_lib_save_LDFLAGS=$LDFLAGS
LIBS="-l$1 $6 $LIBS"
LDFLAGS="$3 $LDFLAGS"
AC_LINK_IFELSE([AC_LANG_CALL([], [$2])],
               [AS_VAR_SET(ac_Lib, yes)],
               [AS_VAR_SET(ac_Lib, no)])
LIBS=$ac_check_lib_save_LIBS
LDFLAGS=$ac_check_lib_save_LDFLAGS])
AS_IF([test AS_VAR_GET(ac_Lib) = yes],
      [m4_default([$4], [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_LIB$1))
  LIBS="-l$1 $LIBS"
])],
      [$5])dnl
AS_VAR_POPDEF([ac_Lib])dnl
])# AC_CHECK_LIB
dnl 
dnl Autoconf support for LTP convergence layer based on TCD's LTPLib
dnl

AC_DEFUN(AC_CONFIG_LTP, [

    AC_ARG_WITH(ltp,
      [AC_HELP_STRING([--with-ltp],
                      [enable LTP convergence layer support using TCD's LTPlib (EXPERIMENTAL)])],
      [ac_use_ltp=$withval],
      [ac_use_ltp="no"])
    
    AC_MSG_CHECKING([whether to enable LTP convergence layer support])

    dnl
    dnl Disable LTP if requested
    dnl
    if test "$ac_use_ltp" = "no"; then
        AC_MSG_RESULT(no)
        LTP_ENABLED=0
    else

    dnl
    dnl Find the LTPlib installation
    dnl
    if test "$ac_use_ltp" = try \
         -o "$ac_use_ltp" = yes \ 
         -o "$ac_use_ltp" = "" ; then
        AC_MSG_RESULT([try])
        ac_ltp_inst_dirs="$ac_pwd/../LTPlib /usr /usr/local"
    else
        AC_MSG_RESULT([yes (dir $ac_use_ltp)])
        ac_ltp_inst_dirs="$ac_use_ltp"
    fi

    ac_save_EXTLIB_CFLAGS="$EXTLIB_CFLAGS"
    ac_save_EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS"
    
    AC_MSG_CHECKING([whether ltp libraries are available])
    AC_CACHE_VAL(dtn_cv_path_ltp,
    [
        for ac_ltp_inst_dir in $ac_ltp_inst_dirs; do
            if test -d "$ac_ltp_inst_dir"; then
                AC_LANG([C++])
                EXTLIB_CFLAGS="-DFILE_OFFSET_BITS=64"
                EXTLIB_LDFLAGS="-L$ac_pwd/../LTPlib/lib \
                                -lltp -lpthread -lcrypto -lm"
    
                AC_COMPILE_IFELSE(
                    AC_LANG_PROGRAM(
                        [
                            #include "$ac_ltp_inst_dir/include/ltp.h"
                        ],
    
                        [
                            ltpaddr la; 
                        ]),
                    [
                        dtn_cv_path_ltp=`cd $ac_ltp_inst_dir && pwd`
                        break
                    ],
                    [
                        dtn_cv_path_ltp=
                    ]
                )
            fi
        done
    ])
    
    EXTLIB_CFLAGS="$ac_save_EXTLIB_CFLAGS"
    EXTLIB_LDFLAGS="$ac_save_EXTLIB_LDFLAGS"

    if test -z "$dtn_cv_path_ltp"; then
        AC_MSG_RESULT([no])
        LTP_ENABLED=0
    else
        AC_MSG_RESULT([yes])
        LTP_ENABLED=1
        AC_DEFINE(LTP_ENABLED, 1, [whether LTP support is enabled])
        if test ! "$dtn_cv_path_ltp" = /usr && test ! "$dtn_cv_path_ltp" = /usr/local; then
            EXTLIB_CFLAGS="$EXTLIB_CFLAGS -I$dtn_cv_path_ltp/include"
            EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -L$dtn_cv_path_ltp/lib"
        fi
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -lltp -lpthread -lcrypto -lm"
    fi

    fi
])


dnl
dnl Autoconf support for the LTPUDP Convergence Layer
dnl

AC_DEFUN(AC_CONFIG_LTPUDP, [
    ac_ltpudp='yes'
    AC_ARG_WITH(ltpudp,
        AC_HELP_STRING([--without-ltpudp],
    		   	[enable LTPUDP Convvergence Layer support (EXPERIMENTAL)]),
        ac_ltpudp=$withval)
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_ltpudp" = no; then
        LTPUDP_ENABLED=0
    else
        LTPUDP_ENABLED=1
        AC_DEFINE_UNQUOTED(LTPUDP_ENABLED, 1, [whether LTPUDP Authentication support is enabled])
    fi # LTPUDP_ENABLED
])
dnl
dnl Autoconf support for LTPUDP Authentication
dnl

AC_DEFUN(AC_CONFIG_LTPUDPAUTH, [
    ac_ltpudpauth='no'
    AC_ARG_WITH(ltpudpauth,
        AC_HELP_STRING([--with-ltpudpauth],
    		   	[enable LTPUDP Authentication support (EXPERIMENTAL)]),
        ac_ltpudpauth=$withval)
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_ltpudpauth" = no; then
        LTPUDP_AUTH_ENABLED=0
    else
        LTPUDP_AUTH_ENABLED=1
        AC_DEFINE_UNQUOTED(LTPUDP_AUTH_ENABLED, 1, [whether LTPUDP Authentication support is enabled])
    fi # LTPUDP_AUTH_ENABLED
])
dnl 
dnl   Copyright 2008 The MITRE Corporation
dnl
dnl   Licensed under the Apache License, Version 2.0 (the "License");
dnl   you may not use this file except in compliance with the License.
dnl   You may obtain a copy of the License at
dnl
dnl       http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl   Unless required by applicable law or agreed to in writing, software
dnl   distributed under the License is distributed on an "AS IS" BASIS,
dnl   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl   See the License for the specific language governing permissions and
dnl   limitations under the License.
dnl
dnl   The US Government will not be charged any license fee and/or royalties
dnl   related to this software. Neither name of The MITRE Corporation; nor the
dnl   names of its contributors may be used to endorse or promote products
dnl   derived from this software without specific prior written permission.
dnl

dnl 
dnl Autoconf support for NORM convergence layer
dnl

AC_DEFUN(AC_CONFIG_NORM, [

    AC_ARG_WITH(norm,
      [AC_HELP_STRING([--with-norm],
                      [enable NORM convergence layer support (EXPERIMENTAL)])],
      [ac_use_norm=$withval],
      [ac_use_norm="no"])
    
    AC_MSG_CHECKING([whether to enable NORM convergence layer support])

    dnl
    dnl Disable norm if requested
    dnl
    if test "$ac_use_norm" = "no"; then
        AC_MSG_RESULT(no)
        NORM_ENABLED=0
    else

    dnl
    dnl Find the norm installation
    dnl
    if test "$ac_use_norm" = try \
         -o "$ac_use_norm" = yes \ 
         -o "$ac_use_norm" = "" ; then
        AC_MSG_RESULT([try])
        ac_norm_inst_dirs="$ac_pwd/../norm /usr /usr/local"
    else
        AC_MSG_RESULT([yes (dir $ac_use_norm)])
        ac_norm_inst_dirs="$ac_use_norm"
    fi

    ac_save_EXTLIB_CFLAGS="$EXTLIB_CFLAGS"
    ac_save_EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS"
    
    AC_MSG_CHECKING([whether norm libraries are available])
    AC_CACHE_VAL(dtn_cv_path_norm,
    [
        for ac_norm_inst_dir in $ac_norm_inst_dirs; do
            if test -d "$ac_norm_inst_dir"; then
                AC_LANG([C++])
                EXTLIB_CFLAGS="-DFILE_OFFSET_BITS=64"
                EXTLIB_LDFLAGS="-L$ac_pwd/../norm/build/default \
                                -L$ac_pwd/../norm/build/default/protolib \
                                -lnorm -lprotokit -lpthread"
    
                AC_COMPILE_IFELSE(
                    AC_LANG_PROGRAM(
                        [
                            #include "$ac_norm_inst_dir/include/normApi.h"
                        ],
    
                        [
                            NormInstanceHandle handle = NormCreateInstance();
                            NormDestroyInstance(handle);
                        ]),
                    [
                        dtn_cv_path_norm="$ac_norm_inst_dir"
                        break
                    ],
                    [
                        dtn_cv_path_norm=
                    ]
                )
            fi
        done
    ])
    
    EXTLIB_CFLAGS="$ac_save_EXTLIB_CFLAGS"
    EXTLIB_LDFLAGS="$ac_save_EXTLIB_LDFLAGS"

    if test -z "$dtn_cv_path_norm"; then
        AC_MSG_RESULT([no])
        NORM_ENABLED=0
    else
        AC_MSG_RESULT([yes])
        NORM_ENABLED=1
        AC_DEFINE(NORM_ENABLED, 1, [whether norm support is enabled])
        if test ! "$dtn_cv_path_norm" = /usr && test ! "$dtn_cv_path_norm" = /usr/local; then
            EXTLIB_CFLAGS="$EXTLIB_CFLAGS -I$dtn_cv_path_norm/include"
            EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -L$dtn_cv_path_norm/build/default -L$dtn_cv_path_norm/build/default/protolib"
        fi
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -lnorm -lprotokit"
    fi

    fi
])
dnl
dnl Autoconf support for finding OpenSSL
dnl
dnl

AC_DEFUN(AC_CONFIG_OPENSSL, [
    if test "$ac_bsp" = yes; then
        ac_openssldir='yes'
        
        AC_ARG_WITH(openssl,
            AC_HELP_STRING([--with-openssl=DIR],
                [location of an OpenSSL installation (default system)]),
            ac_openssldir=$withval)
        
        ac_save_CPPFLAGS="$CPPFLAGS"
        ac_save_LDFLAGS="$LDFLAGS"
        ac_save_LIBS="$LIBS"
        
        dnl
        dnl Now check if we have a cached value, unless the user specified
        dnl something explicit with the --with-openssl= argument, in
        dnl which case we force it to redo the checks (i.e. ignore the
        dnl cached values)
        dnl
        if test "$ac_openssldir" = yes -a ! x$openssl_cv_include = x ; then
            echo "checking for OpenSSL installation... (cached) $openssl_cv_include/openssl/evp.h, $openssl_cv_lib -lcrypto"
        else
            if test "$ac_openssldir" = system -o \
                    "$ac_openssldir" = yes -o \
                    "$ac_openssldir" = "" ; 
            then
                ac_openssldir="/usr/include"
                
                openssl_include=$ac_openssldir
                openssl_lib="/usr/lib"
            else
                openssl_include=$ac_openssldir/include
                CPPFLAGS="-I$openssl_include"
                openssl_lib=$ac_openssldir/lib
                LDFLAGS="-L$openssl_lib"
           fi
            
        fi
        
        AC_CHECK_HEADERS([$openssl_include/openssl/evp.h], [], [AC_MSG_FAILURE([Cannot find OpenSSL.
       On Debian-based Linux systems, you need the 'libssl-dev' package.])])
        
        AC_CHECK_LIB([crypto], [EVP_DigestInit], [], [AC_MSG_FAILURE([Cannot find OpenSSL.
       On Debian-based Linux systems, you need the 'libssl-dev' package.])])
        
        AC_CHECK_LIB([crypto], [EVP_sha256], [], [AC_MSG_FAILURE([Cannot find EVP_sha256.
       On Mac OS X systems, you probably need an updated OpenSSL package, version 0.9.8.
       Specify   --with-openssl=/path/to/openssl LDFLAGS="-Wl,-search_paths_first"])])

       if test "$openssl_include" != /usr/include ; then
          EXTLIB_CFLAGS="$EXTLIB_CFLAGS -I$openssl_include"
       fi

       if test "$openssl_lib" != /usr/lib ; then
	   EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -L$openssl_lib"
       fi
       
       EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -lcrypto"

       CPPFLAGS=$ac_save_CPPFLAGS
       LDFLAGS=$ac_save_LDFLAGS
       LIBS=$ac_save_LIBS
    fi
])
dnl
dnl Autoconf support for building scripting apis
dnl
dnl

AC_DEFUN(AC_CONFIG_SCRIPT_APIS, [
	AC_MSG_CHECKING(for --enable-scriptapis)
        AC_ARG_ENABLE(scriptapis,
            AC_HELP_STRING([--enable-scriptapis],
                [Enable PYTHON, PERL and TCL APIs]),
                [ENABLE_SCRIPT_APIS=yes
                AC_SUBST(ENABLE_SCRIPT_APIS)
                AC_MSG_RESULT(yes)
                ],
	        [ENABLE_SCRIPT_APIS=
		AC_SUBST(ENABLE_SCRIPT_APIS)
                AC_MSG_RESULT(no)
                ])
])

dnl
dnl    Copyright 2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl -------------------------------------------------------------------------
dnl Configure the options for oasys atomic functions
dnl -------------------------------------------------------------------------
AC_DEFUN(AC_OASYS_CONFIG_ATOMIC, [
    dnl
    dnl Handle --enable-atomic-nonatomic[=yes|no]
    dnl
    dnl
    AC_ARG_ENABLE(atomic_nonatomic,
                  AC_HELP_STRING([--enable-atomic-nonatomic],
                    [compile with non-atomic "atomic" routines (testing only)]),
                  [atomic_nonatomic=$enableval],
                  [atomic_nonatomic=no])
    
    AC_MSG_CHECKING([whether to compile with non-atomic "atomic" routines])
    AC_MSG_RESULT($atomic_nonatomic)

    if test $atomic_nonatomic = yes ; then
        AC_MSG_NOTICE([***])
        AC_MSG_NOTICE([*** WARNING: non-atomic "atomic" routines are for testing only ***])
        AC_MSG_NOTICE([***])

        AC_DEFINE_UNQUOTED(OASYS_ATOMIC_NONATOMIC, 1,
                       [whether non-atomic "atomic" routines are enabled])
    else

    dnl
    dnl Handle --enable-atomic-asm[=yes|no]
    dnl        --disable-atomic-asm
    dnl
    AC_ARG_ENABLE(atomic_asm,
                  AC_HELP_STRING([--disable-atomic-asm],
                         [compile without assembly-based atomic functions]),
                  [atomic_asm=$enableval],
                  [atomic_asm=yes])
    
    AC_MSG_CHECKING([whether to compile with assembly-based atomic functions])
    AC_MSG_RESULT($atomic_asm)

    if test $atomic_asm = no ; then
        AC_DEFINE_UNQUOTED(OASYS_ATOMIC_MUTEX, 1,
                      [whether atomic routines are implemented with a mutex])
    fi

    fi
])

dnl
dnl    Copyright 2008 Darren Long, darren.long@mac.com
dnl    Copyright 2007 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl 
dnl Autoconf support for configuring whether ax25 stack is 
dnl available on the system
dnl

AC_DEFUN(AC_CONFIG_AX25, [

    AC_ARG_WITH(ax25,
      [AC_HELP_STRING([--with-ax25],
                      [compile in ax25 support (default try)])],
      [ac_use_ax25=$withval],
      [ac_use_ax25=try])
    
    ac_has_ax25_lib="no"
    ac_has_axconfig_h="no"
    ac_has_ax25_h="no"
    ac_has_axlib_h="no"

    AC_MSG_CHECKING([whether ax25 support should be enabled])

    if test "$ac_use_ax25" = "no"; then
        AC_MSG_RESULT(no)

    else
        AC_MSG_RESULT($ac_use_ax25)
	
	dnl look for the library and the headers
	AC_EXTLIB_PREPARE
        AC_CHECK_HEADERS([netax25/ax25.h], ac_has_ax25_h=yes)
        AC_CHECK_HEADERS([netax25/axlib.h], ac_has_axlib_h=yes)
        AC_CHECK_HEADERS([netax25/axconfig.h], ac_has_axconfig_h=yes, [],
     	[#if HAVE_NETAX25_AX25_H
     	# include <netax25/ax25.h>
     	#endif
     	#if HAVE_NETAX25_AXLIB_H
     	# include <netax25/axlib.h>
     	#endif
     	])       
        AC_SEARCH_LIBS(ax25_config_load_ports, ax25, ac_has_ax25_lib=yes)
	AC_EXTLIB_SAVE

	dnl print the result
        AC_MSG_CHECKING([whether ax25 support was found])
        if test "$ac_has_ax25_lib" = yes -a "$ac_has_axconfig_h" = yes -a "$ac_has_ax25_h" = yes -a "$ac_has_axlib_h" = yes ; then
          AC_DEFINE(OASYS_AX25_ENABLED, 1,
              [whether ax25 support is enabled])
          AC_MSG_RESULT(yes)

	elif test "$ac_use_ax25" = "try" ; then
          AC_MSG_RESULT(no)

        else
          AC_MSG_ERROR([can't find ax25 headers or library])
        fi
    fi
])
dnl
dnl    Copyright 2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl 
dnl Autoconf support for configuring whether BlueZ bluetooth is available
dnl on the system
dnl

AC_DEFUN(AC_CONFIG_BLUEZ, [

    AC_ARG_WITH(bluez,
      [AC_HELP_STRING([--with-bluez],
                      [compile in bluetooth support (default try)])],
      [ac_use_bluez=$withval],
      [ac_use_bluez=try])
    
    ac_has_libbluetooth="no"
    ac_has_bluetooth_h="no"

    AC_MSG_CHECKING([whether bluetooth support should be enabled])

    if test "$ac_use_bluez" = "no"; then
        AC_MSG_RESULT(no)

    else
        AC_MSG_RESULT($ac_use_bluez)
	
        dnl
        dnl Look for the baswap() function in libbluetooth
        dnl
	AC_EXTLIB_PREPARE
        AC_SEARCH_LIBS(baswap, bluetooth, ac_has_libbluetooth="yes") 
	AC_EXTLIB_SAVE

        dnl
        dnl Locate standard Bluetooth header file
        dnl
        AC_CHECK_HEADERS([bluetooth/bluetooth.h],
          ac_has_bluetooth_h="yes")

        dnl
        dnl Print out whether or not we found the libraries
        dnl
        AC_MSG_CHECKING([whether bluetooth support was found])

        dnl
        dnl Check which defines, if any, are set
        dnl
        if test "$ac_has_libbluetooth" = yes -a "$ac_has_bluetooth_h" = yes; then
          dnl
          dnl Enable Bluetooth-dependent code
          dnl
          AC_DEFINE(OASYS_BLUETOOTH_ENABLED, 1,
              [whether bluetooth support is enabled])
          AC_MSG_RESULT(yes)

	elif test "$ac_use_bluez" = "try" ; then
          AC_MSG_RESULT(no)

        else
          AC_MSG_ERROR([can't find bluez headers or library])
        fi
    fi
])
dnl
dnl    Copyright 2007 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl 
dnl Autoconf support for configuring whether Apple's Bonjour stack is 
dnl available on the system
dnl
AC_DEFUN(AC_BONJOUR_HELP, [
cat <<EOF

Configure error with bonjour...

The default is to try to compile in bonjour support and
if development headers and libraries are not present
proceed. You appear to have stated '[$ac_use_bonjour]' 
when asked whether bonjour support should be enabled
Can't find the required development headers (dns_sd.h) or 
function (DNSServiceRegister) in dns_sd of the Avahi 
Apple Bonjour library. 


If you do not want bonjour support at all, you can specify
--without-bonjour.


EOF
])
AC_DEFUN(AC_CONFIG_BONJOUR, [

    AC_ARG_WITH(bonjour,
      [AC_HELP_STRING([--with-bonjour],
                      [compile in bonjour support (default try)])],
      [ac_use_bonjour=$withval],
      [ac_use_bonjour=try])
    
    ac_has_bonjour_lib="no"
    ac_has_bonjour_h="no"

    AC_MSG_CHECKING([whether bonjour support should be enabled])


	if test "$ac_use_bonjour" = "no"; then
	AC_MSG_RESULT(no)

	else
	AC_MSG_RESULT($ac_use_bonjour)
	
	dnl look for the library and the header
	AC_EXTLIB_PREPARE
	AC_MSG_CHECKING([searching for the bonjour library development headers])
        AC_CHECK_HEADERS([dns_sd.h], ac_has_bonjour_h=yes, AC_MSG_RESULT(bonjour header not found))
        AC_SEARCH_LIBS(DNSServiceRegister, dns_sd, ac_has_bonjour_lib=yes, AC_MSG_RESULT(function not found in bonjour library))
	AC_EXTLIB_SAVE

	dnl print the result
        AC_MSG_CHECKING([whether bonjour support was found])
        if test "$ac_has_bonjour_lib" = yes -a "$ac_has_bonjour_h" = yes ; then
          AC_DEFINE(OASYS_BONJOUR_ENABLED, 1,
              [whether bonjour support is enabled])
          AC_MSG_RESULT(yes)

	elif test "$ac_use_bonjour" = "try" ; then
          AC_MSG_RESULT(no)

        else
	AC_BONJOUR_HELP
          AC_MSG_ERROR([can't find the required development headers for the Avahi Apple Bonjour library])
        fi
    fi
])
dnl
dnl    Copyright 2005-2006 Intel Corporation
dnl
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl
dnl Autoconf support for finding Berkeley DB
dnl

AC_DEFUN(AC_DB_HELP, [
cat <<EOF

Configure error with Berkeley DB...

If you do not want Berkeley DB support at all, you can specify
--without-db.

If your installed version is not one of the following versions:
'[$ac_dbvers]', you may have to specify the version explicitly
with --with-dbver=<version>.

If your installation is in a non-standard path, you can specify
the path with --with-db=DIR.

To download the latest version, go to http://www.sleepycat.com
To build and install to /usr/local/BerkeleyDB-<version>:

# cd <db_download_dir>/build_unix
# ../dist/configure
# make
# make install

EOF

])

dnl
dnl Main macro for finding a usable db installation
dnl
AC_DEFUN(AC_CONFIG_DB, [
    ac_dbvers='5.3 5.2 5.1 4.8 4.7 4.6 4.5 4.4 4.3 4.2 4.1'
    ac_dbdir='yes'

    AC_ARG_WITH(db,
        AC_HELP_STRING([--with-db=DIR],
    		   [location of a Berkeley DB installation (default system)]),
        ac_dbdir=$withval)

    AC_ARG_WITH(dbver,
        AC_HELP_STRING([--with-dbver=VERSION],
    		   Berkeley DB versions to try (default 5.1-4.2)),
        ac_dbvers=$withval)

    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_dbdir" = no ; then
    LIBDB_ENABLED=0
    else

    LIBDB_ENABLED=1
    AC_DEFINE_UNQUOTED(LIBDB_ENABLED, 1,
        [whether berkeley db storage support is enabled])

    dnl
    dnl Now check if we have a cached value, unless the user specified
    dnl something explicit with the --with-db= argument, in
    dnl which case we force it to redo the checks (i.e. ignore the
    dnl cached values)
    dnl
    if test "$ac_dbdir" = yes -a ! x$oasys_cv_db_incpath = x ; then
        echo "checking for Berkeley DB installation... (cached) -I$oasys_cv_db_incpath -L$oasys_cv_db_libpath -l$oasys_cv_db_lib"
    else
        AC_FIND_DB
    fi # no cache

    if test ! $oasys_cv_db_incpath = /usr/include ; then
        EXTLIB_CFLAGS="$EXTLIB_CFLAGS -I$oasys_cv_db_incpath"
    fi

    if test ! $oasys_cv_db_libpath = /usr/lib ; then
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -L$oasys_cv_db_libpath"
    fi

    EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -l$oasys_cv_db_lib"

    fi # LIBDB_ENABLED
])

dnl
dnl Find db
dnl
AC_DEFUN(AC_FIND_DB, [
    oasys_cv_db_incpath=
    oasys_cv_db_libpath=
    oasys_cv_db_lib=

    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"
    ac_save_LIBS="$LIBS"

    for ac_dbver in $ac_dbvers ; do

    ac_dbver_major=`echo $ac_dbver | cut -d . -f1`
    ac_dbver_minor=`echo $ac_dbver | cut -d . -f2`

    dnl
    dnl For each version, we look in /usr, /usr/local, and /usr/local/BerkeleyDB.XX
    dnl making sure that the resulting include and lib paths must match.
    dnl
    if test "$ac_dbdir" = system -o \
            "$ac_dbdir" = yes -o \
            "$ac_dbdir" = "" ;
    then
	ac_dbdirs="/usr /usr/local /usr/local/BerkeleyDB.$ac_dbver"
    else
	ac_dbdirs="$ac_dbdir"
    fi

    for dir in $ac_dbdirs; do
        ac_dbincdirs="$dir/include"
        ac_dblibdirs="$dir/lib"

	dnl
	dnl Need to also check variations in /usr/local/include/dbXXX
	dnl
	if test $dir = /usr/local ; then
 	    ac_dbincdirs="$ac_dbincdirs /usr/local/include/db$ac_dbver"
 	    ac_dbincdirs="$ac_dbincdirs /usr/local/include/db$ac_dbver_major"
 	    ac_dbincdirs="$ac_dbincdirs /usr/local/include/db$ac_dbver_major$ac_dbver_minor"

 	    ac_dblibdirs="$ac_dblibdirs /usr/local/lib/db$ac_dbver"
 	    ac_dblibdirs="$ac_dblibdirs /usr/local/lib/db$ac_dbver_major"
 	    ac_dblibdirs="$ac_dblibdirs /usr/local/lib/db$ac_dbver_major$ac_dbver_minor"
	fi

	for ac_dbincdir in $ac_dbincdirs ; do

	CPPFLAGS="$ac_save_CPPFLAGS -I$ac_dbincdir"
	LDFLAGS="$ac_save_LDFLAGS"
	LIBS="$ac_save_LIBS"

	dnl
	dnl First check the version in the header file. If there's a match,
	dnl fall through to the other check to make sure it links.
	dnl If not, then we can break out of the two inner loops.
	dnl
        AC_MSG_CHECKING([for Berkeley DB header (version $ac_dbver) in $ac_dbincdir])
	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <db.h>

                #if (DB_VERSION_MAJOR != ${ac_dbver_major}) || \
                    (DB_VERSION_MINOR != ${ac_dbver_minor})
                #error "incorrect version"
                #endif
            ],

            [
            ]),
          [
	      AC_MSG_RESULT([yes])
          ],
          [
              AC_MSG_RESULT([no])
	      continue
          ])

          for ac_dblibdir in $ac_dblibdirs; do
          for ac_dblib    in db-$ac_dbver; do

          LDFLAGS="$ac_save_LDFLAGS -L$ac_dblibdir"
          if test x"$STATIC" = x"extlibs" ; then
                  LIBS="-Wl,-Bstatic -l$ac_dblib -Wl,-Bdynamic $ac_save_LIBS"
          else
                  LIBS="-l$ac_dblib $ac_save_LIBS"
          fi

          AC_MSG_CHECKING([for Berkeley DB library in $ac_dblibdir, -l$ac_dblib])
          AC_LINK_IFELSE(
            AC_LANG_PROGRAM(
              [
                  #include <db.h>
              ],

              [
                  DB *db;
                  db_create(&db, NULL, 0);
              ]),

            [
                AC_MSG_RESULT([yes])
                oasys_cv_db_incpath=$ac_dbincdir
                oasys_cv_db_libpath=$ac_dblibdir
                oasys_cv_db_lib=$ac_dblib
                break 5
            ],
            [
                AC_MSG_RESULT([no])
            ])

          done # foreach ac_dblib
          done # foreach ac_dblibdir
        done # foreach ac_dbincdir
    done # foreach ac_dbdir
    done # foreach ac_dbver

    AC_DEFINE_UNQUOTED(BERKELEY_DB_VERSION, $ac_dbver,
        [configured version of berkeley db])

    CPPFLAGS="$ac_save_CPPFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
    LIBS="$ac_save_LIBS"

    if test x$oasys_cv_db_incpath = x ; then
        AC_DB_HELP
        AC_MSG_ERROR([can't find usable Berkeley DB installation])
    fi
])
dnl
dnl    Copyright 2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl
dnl Autoconf support for finding expat
dnl

dnl
dnl Main macro for finding a usable db installation 
dnl
AC_DEFUN(AC_CONFIG_EXPAT, [
    AC_ARG_WITH(expat,
        AC_HELP_STRING([--with-expat=DIR],
    		   [location of an expat installation (default try)]),
        [ac_expatdir=$withval],
        [ac_expatdir=try])

    AC_MSG_CHECKING([whether expat support should be enabled])

    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_expatdir" = no ; then
    AC_MSG_RESULT([no])
    LIBEXPAT_ENABLED=0
    else
    
    if test "$ac_expatdir" = try ; then
       AC_MSG_RESULT([try])
    else
       AC_MSG_RESULT([yes (dir $ac_expatdir)])
    fi
    dnl
    dnl Now check if we have a cached value, unless the user specified
    dnl something explicit with the --with-expat= argument, in
    dnl which case we force it to redo the checks (i.e. ignore the
    dnl cached values)
    dnl
    if test "$ac_expatdir" = yes -a ! x$oasys_cv_path_expat_h = x ; then
        if test ! "$oasys_cv_lib_expat" = "" ; then
            echo "checking for expat installation... (cached) $oasys_cv_path_expat_h/expat.h, $oasys_cv_path_expat_lib -l$oasys_cv_lib_expat"
        else
            echo "checking for expat installation... (cached) disabled"
        fi
    else
        AC_FIND_EXPAT
    fi

    if test ! "$oasys_cv_lib_expat" = "" ; then
        LIBEXPAT_ENABLED=1
        AC_DEFINE_UNQUOTED(LIBEXPAT_ENABLED, 1, 
            [whether expat is enabled])

        if test ! $oasys_cv_path_expat_h = /usr/include ; then
           EXTLIB_CFLAGS="$EXTLIB_CFLAGS -I$oasys_cv_path_expat_h"
        fi

        if test ! $oasys_cv_path_expat_lib = /usr/lib ; then
            EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -L$oasys_cv_path_expat_lib"
        fi

        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -l$oasys_cv_lib_expat"
    else
        LIBEXPAT_ENABLED=0
    fi

    fi # ac_expatdir != no
])

dnl
dnl Find expat
dnl
AC_DEFUN(AC_FIND_EXPAT, [
    oasys_cv_path_expat_h=
    oasys_cv_path_expat_lib=
    oasys_cv_lib_expat=

    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"
    ac_save_LIBS="$LIBS"

    if test "$ac_expatdir" = system -o \
            "$ac_expatdir" = yes -o \
            "$ac_expatdir" = try -o \
            "$ac_expatdir" = "" ; 
    then
        ac_expatincdirs="/usr/include /usr/local/include"
        ac_expatlibdirs="/usr/lib     /usr/local/lib"
    else
        ac_expatincdirs="$ac_expatdir/include"
        ac_expatlibdirs="$ac_expatdir/lib"
    fi

    for ac_expatincdir in $ac_expatincdirs; do
    for ac_expatlibdir in $ac_expatlibdirs; do
        ac_expatlib="expat"

	CPPFLAGS="$ac_save_CPPFLAGS -I$ac_expatincdir"
	LDFLAGS="$ac_save_LDFLAGS -L$ac_expatlibdir"
	if test x"$STATIC" = x"extlibs" ; then
		LIBS="-Wl,-Bstatic -l$ac_expatlib -Wl,-Bdynamic $ac_save_LIBS"
	else
		LIBS="-l$ac_expatlib $ac_save_LIBS"
	fi

        AC_MSG_CHECKING([for expat in $ac_expatincdir, $ac_expatlibdir, -l$ac_expatlib])
	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <expat.h>
            ],
            [
		XML_Parser parser;
		parser = XML_ParserCreate(NULL);
            ]),

          [
              AC_MSG_RESULT([yes])
              oasys_cv_path_expat_h=$ac_expatincdir
              oasys_cv_path_expat_lib=$ac_expatlibdir
              oasys_cv_lib_expat=$ac_expatlib
              break 4
          ],
          [
              AC_MSG_RESULT([no])
              oasys_cv_path_expat_h=
              oasys_cv_path_expat_lib=
              oasys_cv_lib_expat=
          ])
    done
    done

    CPPFLAGS="$ac_save_CPPFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
    LIBS="$ac_save_LIBS"

    AC_MSG_CHECKING([whether libexpat was found])
    if test ! x$oasys_cv_path_expat_h = x ; then
        AC_MSG_RESULT(yes)
    elif test $ac_expatdir = try ; then
        AC_MSG_RESULT(no)
    else
        AC_MSG_ERROR([can't find usable expat installation])
    fi
])
dnl
dnl Autoconf support for external data store
dnl

AC_DEFUN(AC_CONFIG_EXTERNAL_DS, [
    ac_eds='no'
    AC_ARG_ENABLE(eds,
        AC_HELP_STRING([--enable-eds],
    		   	[enable external data store support]),
        ac_eds=$enableval) 
    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_eds" = no; then
        EXTERNAL_DS_ENABLED=0
    else
        if test ! "$XERCES_C_ENABLED" = 1 ; then
	    AC_MSG_ERROR([external data store support requires xerces... install it or configure --disable-eds])
        fi
        EXTERNAL_DS_ENABLED=1
        AC_DEFINE_UNQUOTED(EXTERNAL_DS_ENABLED, 1, [whether external data store support is enabled])
    fi # EXTERNAL_DS_ENABLED
])
dnl
dnl    Copyright 2007 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl
dnl Helper macros to handle compilation tests on external libraries.
dnl
dnl Before configuring the library, a test should call AC_EXTLIB_PREPARE,
dnl then set CFLAGS, LDFLAGS, and LIBS accordingly to properly configure
dnl the library, and finally call AC_EXTLIB_SAVE to record the settings
dnl in the EXTLIB_CFLAGS and EXTLIB_LDFLAGS variables.
dnl


AC_DEFUN(AC_EXTLIB_PREPARE, [
    ac_extlib_save_CFLAGS=$CFLAGS
    ac_extlib_save_LDFLAGS=$LDFLAGS
    ac_extlib_save_LIBS=$LIBS

    CFLAGS=
    LDFLAGS=
    LIBS=
])

AC_DEFUN(AC_EXTLIB_SAVE, [
    if test ! "$CFLAGS" = "" ; then
        EXTLIB_CFLAGS="$EXTLIB_CFLAGS $CFLAGS"
    fi

    if test ! "$LDFLAGS" = "" ; then
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS $LDFLAGS"
    fi

    if test ! "$LIBS" = "" ; then
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS $LIBS"
    fi
    
    CFLAGS=$ac_extlib_save_CFLAGS
    LDFLAGS=$ac_extlib_save_LDFLAGS
    LIBS=$ac_extlib_save_LIBS
])
dnl
dnl    Copyright 2005-2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl


dnl
dnl Figure out which version of gcc/g++ to use.
dnl
AC_DEFUN(AC_OASYS_CONFIG_GCC_VERSION, [
    dnl
    dnl Handle --with-cc=CC and --with-cxx=CXX
    dnl
    ac_with_cc=$CC
    AC_ARG_WITH(cc,
        AC_HELP_STRING([--with-cc=CC],
    		   [name (or path) of the C compiler to use]),
                       ac_with_cc=$withval)

    ac_with_cxx=$CXX
    AC_ARG_WITH(cxx,
        AC_HELP_STRING([--with-cxx=CXX],
    		   [name (or path) of the C++ compiler to use]),
                       ac_with_cxx=$withval)
    
    if test "x$ac_with_cc" = "x" ; then
        ac_try_cc="gcc gcc-4.4 gcc-4.3 gcc-4.2 gcc-4.1 gcc-4.0 gcc-3.4 gcc-3.3"
        ac_try_cxx="g++ g++-4.4 g++-4.3 g++-4.2 g++-4.1 g++-4.0 g++-3.4 g++-3.3"
    else
        ac_try_cc=$ac_with_cc

	if test x"$ac_with_cxx" = x ; then
            ac_try_cxx=`echo $ac_with_cc | sed 's/cc/++/'`
	    AC_MSG_NOTICE([inferring C++ compiler '$ac_try_cxx' from C compiler setting])
	else
	    ac_try_cxx=$ac_with_cxx
	fi

	CC=$ac_try_cc
	CXX=$ac_try_cxx
    fi

    dnl
    dnl Check that the compiler specified works
    dnl
    AC_MSG_CHECKING([for a C compiler (trying $ac_try_cc)])
    AC_MSG_RESULT([])
    AC_PROG_CC($ac_try_cc)
    
    AC_MSG_CHECKING([for a C++ compiler (trying $ac_try_cxx)])
    AC_MSG_RESULT([])
    AC_PROG_CXX($ac_try_cxx)

    dnl
    dnl Apparently AC_PROG_CXX isn't enough to actually test the c++
    dnl compiler...
    dnl
    AC_MSG_CHECKING([whether the C++ compiler works])
    AC_LANG_PUSH(C++)
    AC_LINK_IFELSE(AC_LANG_PROGRAM([/*nothing*/],[/*nothing*/]),
		   AC_MSG_RESULT(yes),
		   AC_MSG_RESULT(no) 
		   AC_MSG_FAILURE(C++ compiler does not work))
    AC_LANG_POP(C++)
    
    dnl 
    dnl We want the C preprocessor as well
    dnl    
    AC_PROG_CPP

    dnl
    dnl Figure out the version and set version-specific options
    dnl

    EXTRA_CFLAGS=""
    EXTRA_CXXFLAGS=""

    AC_CACHE_CHECK(for the version of the GNU C compiler, oasys_cv_prog_gccver, [
      oasys_cv_prog_gccver=`$CC -dumpversion | cut -d. -f1-2`
    ])      

    AC_CACHE_CHECK(for the version of the GNU C++ compiler, oasys_cv_prog_gxxver, [
      oasys_cv_prog_gxxver=`$CXX -dumpversion | cut -d. -f1-2`
    ])

    if test $oasys_cv_prog_gccver != $oasys_cv_prog_gxxver ; then
        echo "*** "
	echo "*** warning: C compiler version $oasys_cv_prog_gccver doesn't equal"
	echo "***          C++ compiler version $oasys_cv_prog_gxxver"
        echo "*** "
    fi

    # 
    # Set version-specific compiler options
    #
    case "$oasys_cv_prog_gccver" in
        #
        # for gcc 2.9.X and 3.1, the auto-dependency features don't work, and 
        # _GNU_SOURCE isn't defined, so do both those things here
        #
        3.1*|2.9*)
            EXTRA_CFLAGS="-std=gnu11 -D_BSD_SOURCE"
            EXTRA_CXXFLAGS="-std=c++1y"
	    DEPFLAGS=""
            echo "*** "
	    echo "*** warning: using old compiler $cc version $oasys_cv_prog_gccver,"
	    echo "***          automatic dependency generation will not work"
            echo "*** "
	    ;;
	#
	# For 3.2 and beyond, use auto-dependency flags. 
	# Note that for m4 to output a '$' requires the '@S|@' heinosity below.
	#
	3.*|4.*|5.*|6.*|7.*|8|8.*)
            EXTRA_CFLAGS="-std=gnu11 -D_BSD_SOURCE"
            EXTRA_CXXFLAGS="-std=c++1y"
	    DEPFLAGS=['-MMD -MP -MT "@S|@*.o @S|@*.E"']
	    ;;
	#
	# Otherwise bail
	#
        *)
	    echo "error: unsupported compiler version $oasys_cv_prog_gccver"
	    exit  1
	    ;;
    esac
    AC_SUBST(DEPFLAGS)

    dnl
    dnl Look for the appropriate ar and ranlib tools for this build
    dnl
    AC_CHECK_TOOL(AR, ar)
    AC_CHECK_TOOL(RANLIB, ranlib)

    if test -z "$AR" ; then
       AC_MSG_ERROR([can't find a working ar tool])
    fi
])

dnl
dnl GCC options
dnl
AC_DEFUN(AC_OASYS_CONFIG_GCC_OPTS, [
    dnl
    dnl Handle --enable-debug[=yes|no]
    dnl 	   --disable-debug
    dnl
    AC_ARG_ENABLE(debug,
                  AC_HELP_STRING([--disable-debug],
                                 [compile with debugging turned off]),
                  [debug=$enableval],
                  [debug=yes])
    
    AC_MSG_CHECKING([whether to compile with debugging])
    AC_MSG_RESULT($debug)
    
    DEBUG=
    if test $debug = yes ; then 
        DEBUG="-g -fno-inline"
    else
        DEBUG="-DNDEBUG"
    fi
    AC_SUBST(DEBUG)
	
    dnl
    dnl Handle --enable-debug-memory[=yes|no]
    dnl 	   --disable-debug-memory
    dnl
    AC_ARG_ENABLE(debug_memory,
                  AC_HELP_STRING([--enable-debug-memory],
                                 [enable memory debugging]),
                  [debug_memory=$enableval],
                  [debug_memory=no])
    
    AC_MSG_CHECKING([whether to compile with memory debugging])
    AC_MSG_RESULT($debug_memory)
    
    if test $debug_memory = yes ; then
        AC_DEFINE_UNQUOTED(OASYS_DEBUG_MEMORY_ENABLED, 1,
                       [whether oasys memory debugging is enabled])
    fi

    dnl
    dnl Handle --enable-debug-locking[=yes|no]
    dnl --disable-debug-locking
    dnl
    AC_ARG_ENABLE(debug_locking,
                  AC_HELP_STRING([--enable-debug-locking],
                                 [enable lock debugging]),
                  [debug_locking=$enableval],
                  [debug_locking=no])
    
    AC_MSG_CHECKING([whether to compile with lock debugging])
    AC_MSG_RESULT($debug_locking)
    
    if test $debug_locking = yes ; then
        AC_DEFINE_UNQUOTED(OASYS_DEBUG_LOCKING_ENABLED, 1,
                       [whether oasys lock debugging is enabled])
    fi
    
    dnl
    dnl Handle --enable-debug-logging[=yes|no]
    dnl --disable-debug-logging
    dnl
    AC_ARG_ENABLE(debug_logging,
                  AC_HELP_STRING([--enable-debug-logging],
                                 [enable log_debug]),
                  [debug_logging=$enableval],
                  [debug_logging=no])
    
    AC_MSG_CHECKING([whether to compile with log_debug enabled])
    AC_MSG_RESULT($debug_logging)
    
    if test $debug_logging = yes ; then
        AC_DEFINE_UNQUOTED(OASYS_LOG_DEBUG_ENABLED, 1,
                       [whether oasys log_debug is enabled])
    fi
    
    dnl
    dnl Handle --enable-optimize[=yes|no]
    dnl 	   --disable-optimize
    dnl
    optimize=yes
    AC_ARG_ENABLE(optimize,
                  AC_HELP_STRING([--disable-optimize],
                                 [compile with optimization turned off]),
                  [optimize=$enableval],
                  [optimize=yes])
    
    AC_MSG_CHECKING([whether to compile with optimization])
    AC_MSG_RESULT($optimize)

    OPTIMIZE=
    OPTIMIZE_WARN=
    if test $optimize = yes ; then
        OPTIMIZE="-O2 -fno-strict-aliasing"
        OPTIMIZE_WARN=-Wuninitialized
    fi
    AC_SUBST(OPTIMIZE)
    AC_SUBST(OPTIMIZE_WARN)

    dnl
    dnl Handle --enable-profiling[=yes|no]
    dnl        --disable-profiling
    profile=
    AC_ARG_ENABLE(profile,
                  AC_HELP_STRING([--enable-profile],[compile with profiling]),
                  [profile=$enableval],
                  [profile=no])
    
    AC_MSG_CHECKING([whether to compile with profiling])
    AC_MSG_RESULT($profile)

    PROFILE=
    if test $profile = yes ; then
        PROFILE="-pg"   
    fi
    AC_SUBST(PROFILE)

    dnl
    dnl Handle --enable-static[=yes|no]
    dnl        --enable-static-external-libs[=yes|no]
    dnl
    AC_ARG_ENABLE(static,
                  AC_HELP_STRING([--enable-static],
                                 [statically link all binaries]),
                  [static=$enableval],
                  [static=no])

    AC_MSG_CHECKING([whether to link statically])
    AC_MSG_RESULT($static)

    AC_ARG_ENABLE(static-external-libs,
                  AC_HELP_STRING([--enable-static-external-libs],
                                 [use static external libraries]),
                  [staticextlibs=$enableval],
                  [staticextlibs=no])

    AC_MSG_CHECKING([whether to link using static external libraries])
    AC_MSG_RESULT($staticextlibs)

    STATIC=no
    if [[ $static = yes ]] ; then
       STATIC=yes
       LDFLAGS="$LDFLAGS -static"
    elif [[ $staticextlibs = yes ]] ; then
       STATIC=extlibs
    fi
    AC_SUBST(STATIC)

    AC_MSG_CHECKING([whether to try to build shared libraries])
    if [[ $static = yes ]] ; then
        AC_MSG_RESULT(no)
    else

    dnl    
    dnl Handle --enable-shlibs 
    dnl
    dnl Turns on shared library building which also builds all code 
    dnl with -fPIC -DPIC.
    dnl
    AC_ARG_ENABLE(shlibs,
                  AC_HELP_STRING([--enable-shlibs],
                                 [enable building of shared libraries (default try)]),
                  [shlibs=$enableval],
                  [shlibs=yes])

    AC_MSG_RESULT($shlibs)

    SHLIBS=no
    PICFLAGS=""
    LDFLAGS_SHLIB=""
    if [[ $shlibs = yes ]] ; then
       ac_save_LDFLAGS=$LDFLAGS
       ac_save_CFLAGS=$CFLAGS
       picflags="-fPIC -DPIC"
       CFLAGS="$picflags $ac_save_CFLAGS -Werror"
       AC_MSG_CHECKING([whether the compiler supports -fPIC])
       AC_COMPILE_IFELSE([int myfunc() {return 0;}], [pic=yes], [pic=no])
       AC_MSG_RESULT($pic)

       if [[ $pic = yes ]] ; then
           
           for shopt in -shared "-shared -fPIC" "-dynamiclib -single_module" ; do
             AC_MSG_CHECKING([whether the compiler can link a dynamic library with $shopt])
	     LDFLAGS="$shopt $ac_save_LDFLAGS"
             AC_LINK_IFELSE([void myfunc() {}], [shlink=yes], [shlink=no])
             AC_MSG_RESULT($shlink)

             if [[ $shlink = yes ]] ; then
                 SHLIBS=yes
                 PICFLAGS=$picflags
                 LDFLAGS_SHLIB="$shopt $PICFLAGS"

		 AC_MSG_CHECKING([extension for dynamic libraries])
		 dnl XXX/demmer this could be done in some better way but I don't know how
		 if [[ "$shopt" = "-dynamiclib -single_module" ]] ; then
                     SHLIB_EXT=dylib
		 elif [[ "$shopt" = "-shared -fPIC" ]] ; then
                     SHLIB_EXT=so
                 elif [[ $shopt = -shared ]] ; then
                     SHLIB_EXT=so
                 else
                     # shouldn't happen if above options are correctly checked
                     AC_MSG_ERROR(internal error in configure script)
                 fi
		 AC_MSG_RESULT($SHLIB_EXT)

		 AC_MSG_CHECKING([if the compiler supports -Wl,--rpath=.])
		 LDFLAGS="$LDFLAGS -Wl,-rpath=/foo"
		 AC_LINK_IFELSE([void myfunc() {}], [rpath=yes], [rpath=no])
		 AC_MSG_RESULT($rpath)
		 if [[ $rpath = yes ]] ; then
		     RPATH="-Wl,-rpath=. -Wl,-rpath=\$(prefix)/lib"
		 fi
                 break
             fi
           done
       fi

       CFLAGS=$ac_save_CFLAGS
       LDFLAGS=$ac_save_LDFLAGS
    fi
    AC_SUBST(SHLIBS)
    AC_SUBST(SHLIB_EXT)
    AC_SUBST(PICFLAGS)
    AC_SUBST(RPATH)
    AC_SUBST(LDFLAGS_SHLIB)

    fi # ! $static = yes

    dnl
    dnl Add options to add extra compilation flags
    dnl
    dnl EXTRA_CFLAGS=""
    AC_ARG_WITH(extra-cflags,
        AC_HELP_STRING([--with-extra-cflags=?],
	 	       [additional options to pass to the compiler]),
                       EXTRA_CFLAGS=$withval)
    AC_SUBST(EXTRA_CFLAGS)

    dnl EXTRA_CXXFLAGS=""
    AC_ARG_WITH(extra-cxxflags,
        AC_HELP_STRING([--with-extra-cxxflags=?],
                [additional options to pass to the compiler]),
                        EXTRA_CXXFLAGS=$withval)
    AC_SUBST(EXTRA_CXXFLAGS)

    EXTRA_LDFLAGS=""
    AC_ARG_WITH(extra-ldflags,
        AC_HELP_STRING([--with-extra-ldflags=?],
	 	       [additional options to pass to the linker]),
                       EXTRA_LDFLAGS=$withval)
    AC_SUBST(EXTRA_LDFLAGS)
])

dnl
dnl Configure gcc
dnl
AC_DEFUN(AC_OASYS_CONFIG_GCC, [
    dnl
    dnl Handle the various autoconf options for cross compilation that 
    dnl set either --target or (better?) both --build and --host.
    dnl
    if test ! -z "$host_alias" ; then
        TARGET=$host_alias
    elif test ! -z "$target_alias" ; then
        TARGET=$target_alias
    elif test ! -z "$target" ; then
        TARGET=$target
    else
        TARGET=native
    fi
    AC_SUBST(TARGET)

    AC_OASYS_CONFIG_GCC_VERSION
    AC_OASYS_CONFIG_GCC_OPTS

    dnl
    dnl Set the build system in a variable so it's usable by
    dnl Makefiles and other scripts
    dnl
    BUILD_SYSTEM=`uname -s || echo UNKNOWN | sed 's/CYGWIN.*/CYGWIN/'`
    AC_SUBST(BUILD_SYSTEM)
])
dnl
dnl    Copyright 2005-2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl
dnl Autoconf support for linking with google's performace code
dnl

AC_DEFUN(AC_CONFIG_GOOGLE_PERFTOOLS, [

    ac_google_perfdir='no'
    AC_ARG_WITH(google-perftools,
        AC_HELP_STRING([--with-google-perftools=DIR],
            [location of a google perftools installation (default none)]),
        ac_google_perfdir=$withval) 

    AC_ARG_ENABLE(google-profiling,
                  AC_HELP_STRING([--enable-google-profiling],
		  [compile with google profiling]),
                  [google_profile=$enableval],
                  [google_profile=no])
    
    AC_MSG_CHECKING([whether to compile with google profiling])
    AC_MSG_RESULT($google_profile)

    GOOGLE_PROFILE_ENABLED=0
    if test $google_profile = yes ; then
        if test $ac_google_perfdir = no ; then
	   AC_MSG_ERROR([must specify --with-google-perftools to use google profiling])
	fi
        GOOGLE_PROFILE_ENABLED=1
    fi
    
    AC_DEFINE_UNQUOTED(GOOGLE_PROFILE_ENABLED, $GOOGLE_PROFILE_ENABLED,
	 [whether google profiling support is enabled])

    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_google_perfdir" = no; then
    GOOGLE_PERFTOOLS_ENABLED=0
    else

    GOOGLE_PERFTOOLS_ENABLED=1
    AC_DEFINE_UNQUOTED(GOOGLE_PERFTOOLS_ENABLED, 1, 
	 [whether google perftools support is enabled])

    dnl
    dnl Now check if we have a cached value, unless the user specified
    dnl something explicit with the --with-google-perf= argument, in
    dnl which case we force it to redo the checks (i.e. ignore the
    dnl cached values)
    dnl
    if test "$ac_google_perfdir" = yes -a \
	    ! "$oasys_cv_path_google_perf_h" = "" ; then
        echo "checking for google_perf installation... (cached)"
    else
        AC_OASYS_FIND_GOOGLE_PERFTOOLS
    fi

    if test ! $oasys_cv_path_google_perf_h = /usr/include ; then
        EXTLIB_CFLAGS="$EXTLIB_CFLAGS -I$oasys_cv_path_google_perf_h"
    fi

    if test ! $oasys_cv_path_google_perf_lib = /usr/lib ; then
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -L$oasys_cv_path_google_perf_lib"
    fi
    
    EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -Wl,-Bstatic -lprofiler -Wl,-Bdynamic"
    
    fi # GOOGLE_PERF_ENABLED
])

AC_DEFUN(AC_OASYS_FIND_GOOGLE_PERFTOOLS, [
    oasys_cv_path_google_perf_h=
    oasys_cv_path_google_perf_lib=

    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"
    ac_save_LIBS="$LIBS"

    AC_LANG_PUSH(C++)

    if test "$ac_google_perfdir" = system -o \
            "$ac_google_perfdir" = yes -o \
            "$ac_google_perfdir" = "" ; 
    then
        ac_google_perfincdirs="/usr/local/include \
	                       /usr/local/google-perftools/include"
        ac_google_perflibdirs="/usr/local/lib \
	                       /usr/local/google-perftools/lib"
    else
        ac_google_perfincdirs="$ac_google_perfdir/include"
        ac_google_perflibdirs="$ac_google_perfdir/lib"
    fi

    for ac_google_perfincdir in $ac_google_perfincdirs; do

	CPPFLAGS="$ac_save_CPPFLAGS -I$ac_google_perfincdir"
	LDFLAGS="$ac_save_LDFLAGS"
	LIBS="$ac_save_LIBS"

	dnl
	dnl First check the version in the header file. If there's a match, 
	dnl fall through to the other check to make sure it links.
	dnl If not, then we can break out of the two inner loops.
	dnl
        AC_MSG_CHECKING([for google perftools header profiler.h in $ac_google_perfincdir])
	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <google/profiler.h>
            ],
            
            [
            ]),
          [ 
	      AC_MSG_RESULT([yes])
              oasys_cv_path_google_perf_h=$ac_google_perfincdir
	      break
          ],
          [
              AC_MSG_RESULT([no])
	      continue
          ])
    done

    AC_LANG_POP(C++)

    if test x$oasys_cv_path_google_perf_h = x ; then
        AC_MSG_ERROR([can't find usable google perftools installation])
    fi

    AC_LANG_PUSH(C++)

    for ac_google_perflibdir in $ac_google_perflibdirs; do

	LDFLAGS="$ac_save_LDFLAGS -L$ac_google_perflibdir"
        LIBS="$ac_save_LIBS -lprofiler"

        AC_MSG_CHECKING([for google perftolos library libprofiler in $ac_google_perflibdir])
	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <google/profiler.h>
            ],
            
            [
		ProfilerStart("test");
            ]),

          [
              AC_MSG_RESULT([yes])
              oasys_cv_path_google_perf_lib=$ac_google_perflibdir
              break
          ],
          [
              AC_MSG_RESULT([no])
          ])
    done

    CPPFLAGS="$ac_save_CPPFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
    LIBS="$ac_save_LIBS"

    AC_LANG_POP(C++)

    if test x$oasys_cv_path_google_perf_lib = x ; then
        AC_MSG_ERROR([can't find usable google perftools library])
    fi
])
dnl
dnl    Copyright 2005-2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl
dnl Autoconf support for finding mysql
dnl

dnl
dnl Main macro for finding a usable mysql installation 
dnl
AC_DEFUN(AC_CONFIG_MYSQL, [
    ac_mysqldir='no'

    AC_ARG_WITH(mysql,
        AC_HELP_STRING([--with-mysql=DIR],
    		   [location of a mysql installation (default none)]),
        ac_mysqldir=$withval) 

    ac_mysqldaemon='no'
    AC_ARG_WITH(mysql-daemon,
        AC_HELP_STRING([--with-mysql-daemon],
    		   [use the daemon library (not the client library)]),
        ac_mysqldaemon=$withval)

    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_mysqldir" = no; then
    MYSQL_ENABLED=0
    else

    MYSQL_ENABLED=1
    AC_DEFINE_UNQUOTED(MYSQL_ENABLED, 1, [whether mysql support is enabled])

    dnl
    dnl If the user wants to use the embedded libmysqld, then we have
    dnl some additional external library dependencies
    dnl    
    if test ! "$ac_mysqldaemon" = "no" ; then
       AC_DEFINE_UNQUOTED(LIBMYSQLD_ENABLED, 1, 
           [whether the mysql embedded server is enabled])

	AC_MSG_CHECKING([for mysql external dependencies.: checking for request_init() in libwrap])
	AC_SEARCH_LIBS(request_init, wrap, [], 
                      AC_MSG_ERROR([can't find request_init() in libwrap]))
	AC_MSG_CHECKING([for mysql external dependencies.: checking for exp() in libm])
	AC_SEARCH_LIBS(exp, m, [],
                      AC_MSG_ERROR([can't find exp() in libm]))
	AC_MSG_CHECKING([for mysql external dependencies.: checking for crypt() in libcrypt])
	AC_SEARCH_LIBS(crypt, crypt, [],
                      AC_MSG_ERROR([can't find crypt() in libcrypt]))
	AC_MSG_CHECKING([for mysql external dependencies.: checking for compress() in libz])
	AC_SEARCH_LIBS(compress, z, [],
                      AC_MSG_ERROR([can't find compress() in libz]))
    fi

    dnl
    dnl Now check if we have a cached value, unless the user specified
    dnl something explicit with the --with-mysql= argument, in
    dnl which case we force it to redo the checks (i.e. ignore the
    dnl cached values)
    dnl
    if test "$ac_mysqldir" = yes -a ! x$oasys_cv_path_mysql_h = x ; then
        echo "checking for mysql installation... (cached) $oasys_cv_path_mysql_h/mysql.h, $oasys_cv_path_mysql_lib -lmysql"
    else
        AC_OASYS_FIND_MYSQL
    fi

    if test ! $oasys_cv_path_mysql_h = /usr/include ; then
        EXTLIB_CFLAGS="$EXTLIB_CFLAGS -I$oasys_cv_path_mysql_h"
    fi

    if test ! $oasys_cv_path_mysql_lib = /usr/lib ; then
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -L$oasys_cv_path_mysql_lib"
    fi

    if test "$ac_mysqldaemon" = "no" ; then
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -lmysqlclient"
    else
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -lmysqld"
    fi
    
    fi # MYSQL_ENABLED
])

dnl
dnl Find mysql
dnl
AC_DEFUN(AC_OASYS_FIND_MYSQL, [
    oasys_cv_path_mysql_h=
    oasys_cv_path_mysql_lib=

    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"
    ac_save_LIBS="$LIBS"

    if test "$ac_mysqldir" = system -o \
            "$ac_mysqldir" = yes -o \
            "$ac_mysqldir" = "" ; 
    then
        ac_mysqlincdirs="/usr/include /usr/include/mysql \
                         /usr/local/mysql/include"
        ac_mysqllibdirs="/usr/lib /usr/lib/mysql /usr/local/mysql/lib"
    else
        ac_mysqlincdirs="$ac_mysqldir/include"
        ac_mysqllibdirs="$ac_mysqldir/lib"
    fi

    AC_LANG_PUSH(C++)
    for ac_mysqlincdir in $ac_mysqlincdirs; do

	CPPFLAGS="$ac_save_CPPFLAGS -I$ac_mysqlincdir"
	LDFLAGS="$ac_save_LDFLAGS"
	LIBS="$ac_save_LIBS"

	dnl
	dnl First check the version in the header file. If there's a match, 
	dnl fall through to the other check to make sure it links.
	dnl If not, then we can break out of the two inner loops.
	dnl
        AC_MSG_CHECKING([for mysql header mysql.h in $ac_mysqlincdir])
	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <mysql.h>
            ],
            
            [
            ]),
          [ 
	      AC_MSG_RESULT([yes])
              oasys_cv_path_mysql_h=$ac_mysqlincdir
	      break
          ],
          [
              AC_MSG_RESULT([no])
	      continue
          ])
    done

    if test x$oasys_cv_path_mysql_h = x ; then
        AC_MSG_ERROR([can't find usable mysql installation])
    fi

    for ac_mysqllibdir in $ac_mysqllibdirs; do

	LDFLAGS="$ac_save_LDFLAGS -L$ac_mysqllibdir"
	if test "$ac_mysqldaemon" = "no" ; then
 	    LIBS="$ac_save_LIBS -lmysqlclient"
        else
 	    LIBS="$ac_save_LIBS -lmysqld"
        fi

        AC_MSG_CHECKING([for mysql library libmysql in $ac_mysqllibdir])
	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <mysql.h>
            ],
            
            [
		MYSQL m;
	        mysql_init(&m);
            ]),

          [
              AC_MSG_RESULT([yes])
              oasys_cv_path_mysql_lib=$ac_mysqllibdir
              break
          ],
          [
              AC_MSG_RESULT([no])
          ])
    done
    AC_LANG_POP(C++)

    CPPFLAGS="$ac_save_CPPFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
    LIBS="$ac_save_LIBS"

    if test x$oasys_cv_path_mysql_lib = x ; then
        AC_MSG_ERROR([can't find usable mysql library])
    fi
])
dnl
dnl    Copyright 2007 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl 
dnl Autoconf support for configuring an external package to use oasys
dnl itself.
dnl

AC_DEFUN(AC_OASYS_CONFIG_HELP, [
cat <<EOF
    
Configure error finding oasys.

This script first looks for an oasys installation in the same location
as the tool that is using the oasys library (where this configure
script is run from). In other words, it first tries ../oasys.

If ../oasys does not exist, it looks for an oasys installation in
/usr. If that is not found, you need to specify the location of oasys
using the --with-oasys argument to configure.

If problems still exist, then look in the config.log to see exactly
what the failing command was.

Download oasys-$ac_oasysver_major.$ac_oasysver_minor (or later) 
from http://dtn.hg.sourceforge.net/hgweb/dtn/oasys to $ac_oasysdir. 
You will then probably have to run ./configure and make  
in the oasys code tree in order to be able to compile DTNME

EOF

])

dnl
dnl AC_OASYS_CONFIG [major] [minor]
dnl
AC_DEFUN(AC_OASYS_CONFIG, [
    if test -d oasys ; then
    	rm -rf oasys
    fi  

    ac_oasysver_major=$1
    ac_oasysver_minor=$2
    ac_oasysdir_ver_base="oasys-$ac_oasysver_major.$ac_oasysver_minor"
    ac_oasysdir=

    AC_ARG_WITH(oasys,
        AC_HELP_STRING([--with-oasys=DIR],
    		   [location of an oasys installation (default ../oasys-<major>.<minor>.* or ../oasys or /usr)]),
        ac_oasysdir=$withval)

    AC_MSG_CHECKING([for an oasys installation (version $ac_oasysver_major.$ac_oasysver_minor or better)])

    # if --with-oasys was not specified then look for the fallback locations
    # which are ../oasys.<major>.<minor>.* and ../oasys
    if test "$ac_oasysdir" = ""; then
      ac_oasysdir_ver=`find .. -maxdepth 1 -type d -name $ac_oasysdir_ver_base.* | tail -1`

      if test -d "$ac_oasysdir_ver" ; then
        ac_oasysdir=$ac_oasysdir_ver
   
      elif test -d ../oasys ; then
        ac_oasysdir=../oasys
        ac_oasysdir_ver=$ac_oasysdir 
      fi
    fi

    
    if test -d "$ac_oasysdir" ; then
      mkdir oasys oasys/include
      OASYS_INCDIR="oasys/include"
      OASYS_LIBDIR="oasys/lib"
      OASYS_ETCDIR="oasys/share"
      if test "${ac_oasysdir:0:1}" = "/" ; then
        # we have an absolute path
        # assume this is an installed instance and not a source directory
        ln -s $ac_oasysdir/include/oasys $OASYS_INCDIR/oasys
        ln -s $ac_oasysdir/lib $OASYS_LIBDIR
        ln -s $ac_oasysdir/share/oasys $OASYS_ETCDIR
      else
        # relative path to source directory
        ln -s ../../$ac_oasysdir $OASYS_INCDIR/oasys
        ln -s ../$ac_oasysdir/lib $OASYS_LIBDIR
        ln -s ../$ac_oasysdir $OASYS_ETCDIR
      fi
    else
      if test "$ac_oasysdir" = "" ; then
        ac_oasysdir=/usr
      fi
      OASYS_INCDIR="$ac_oasysdir/include"
      OASYS_LIBDIR="$ac_oasysdir/lib"
      OASYS_ETCDIR="$ac_oasysdir/share/oasys"
    fi
   
    if test ! -d $OASYS_INCDIR ; then echo ""
    AC_OASYS_CONFIG_HELP
    AC_MSG_ERROR(nonexistent oasys include directory $OASYS_INCDIR)
    fi

    if test ! -d $OASYS_LIBDIR ; then echo ""
    AC_OASYS_CONFIG_HELP
    AC_MSG_ERROR(nonexistent oasys library directory $OASYS_LIBDIR)
    fi

    if test ! -d $OASYS_ETCDIR ; then echo ""
    AC_OASYS_CONFIG_HELP
    AC_MSG_ERROR(nonexistent oasys tools directory $OASYS_ETCDIR)
    fi

    if test "$ac_oasysver_major" = "" ; then
       ac_oasysver_major=1
    fi

    if test "$ac_oasysver_minor" = "" ; then
       ac_oasysver_minor=1
    fi

    AC_OASYS_SUBST_CONFIG

    #
    # Check the settings to make sure that we can build a program.
    #
    ac_save_CFLAGS=$CFLAGS
    CFLAGS="$CFLAGS -I$OASYS_INCDIR"
    
    ac_save_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS $OASYS_LDFLAGS"
    AC_LINK_IFELSE(
	AC_LANG_PROGRAM(
    [
	#include "oasys/oasys-version.h"
	#if (OASYS_VERSION_MAJOR != ${ac_oasysver_major}) || \
            (OASYS_VERSION_MINOR <  ${ac_oasysver_minor})
	#error incorrect oasys version (wanted ${ac_oasysver_major}.${ac_oasysver_minor})
	#endif
    ], 
    [
	const char* s = oasys_version;
	(void)s;
    ]),
    [ 
    AC_MSG_RESULT($ac_oasysdir)
    ],
    [
    AC_MSG_RESULT([no])
    AC_OASYS_CONFIG_HELP
    AC_MSG_ERROR([Can't find compatible oasys libraries which are a required for DTNME.]) 
      ])

    CFLAGS=$ac_save_CFLAGS
    LDFLAGS=$ac_save_LDFLAGS
])

AC_DEFUN(AC_OASYS_SUBST_CONFIG, [
    #
    # XXX/demmer is there a better way to make the paths absolute??
    #
    OASYS_INCDIR=`cd $OASYS_INCDIR && pwd`
    OASYS_LIBDIR=`cd $OASYS_LIBDIR && pwd`
    OASYS_ETCDIR=`cd $OASYS_ETCDIR && pwd`

    OASYS_VERSION=`$OASYS_ETCDIR/tools/extract-version $OASYS_ETCDIR/oasys-version.dat version`

    #
    # By default, oasys apps link statically to make it easier when
    # linking with a source directory, and for building the test apps
    # that are part of the library itself.
    #
    OASYS_LDFLAGS_STATIC="$OASYS_LIBDIR/liboasys-$OASYS_VERSION.a"

    if test -f $OASYS_LIBDIR/liboasys-$OASYS_VERSION.a ; then
	OASYS_LDFLAGS="$OASYS_LIBDIR/liboasys-$OASYS_VERSION.a"
	OASYS_COMPAT_LDFLAGS="$OASYS_LIBDIR/liboasyscompat-$OASYS_VERSION.a"

    elif test "$ac_oasysdir" = /usr ; then
	OASYS_LDFLAGS="-loasys-$OASYS_VERSION"
	OASYS_COMPAT_LDFLAGS="-loasyscompat-$OASYS_VERSION"

    else
	OASYS_LDFLAGS="-L$OASYS_LIBDIR -loasys-$OASYS_VERSION"
	OASYS_COMPAT_LDFLAGS="-L$OASYS_LIBDIR -loasyscompat-$OASYS_VERSION"
    fi
        
    AC_SUBST(OASYS_INCDIR)
    AC_SUBST(OASYS_LIBDIR)
    AC_SUBST(OASYS_ETCDIR)
    AC_SUBST(OASYS_LDFLAGS)
    AC_SUBST(OASYS_LDFLAGS_STATIC)
    AC_SUBST(OASYS_COMPAT_LDFLAGS)
    AC_SUBST(OASYS_VERSION)

    AC_SUBST(EXTLIB_CFLAGS)
    AC_SUBST(EXTLIB_LDFLAGS)

    AC_SUBST(SYS_EXTLIB_CFLAGS)
    AC_SUBST(SYS_EXTLIB_LDFLAGS)
])

dnl
dnl Check if oasys has support for the given feature. Returns result
dnl in ac_oasys_supports_result.
dnl
AC_DEFUN(AC_OASYS_SUPPORTS, [
    AC_MSG_CHECKING(whether oasys is configured with $1)

    if test x$cv_oasys_supports_$1 != x ; then
        ac_oasys_supports_result=$cv_oasys_supports_$1
        AC_MSG_RESULT($ac_oasys_supports_result (cached))
    else

    ac_save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -I$OASYS_INCDIR"
    AC_LINK_IFELSE(
      AC_LANG_PROGRAM(
        [
            #include <oasys/oasys-config.h>
            #ifndef $1
            #error $1 not configured
            #endif
        ], [] ),
      ac_oasys_supports_result=yes,
      ac_oasys_supports_result=no)

    cv_oasys_supports_$1=$ac_oasys_supports_result
    
    AC_MSG_RESULT([$ac_oasys_supports_result])
    CPPFLAGS=$ac_save_CPPFLAGS

    fi
])
dnl
dnl    Copyright 2011 Alex McMahon, alex.mcmahon@cs.tcd.ie
dnl    Copyright 2011 Elwyn Davies, davieseb@scss.tcd.ie
dnl    Copyright 2011 Trinity Collage Dublin
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl 
dnl Autoconf support for configuring whether odbc library is 
dnl available on the system
dnl

AC_DEFUN(AC_CONFIG_ODBC, [

    AC_ARG_WITH(odbc,
      [AC_HELP_STRING([--with-odbc],
                      [compile in odbc support (default try)])],
      [ac_use_odbc=$withval],
      [ac_use_odbc=try])
    
    AC_MSG_CHECKING([whether odbc support should be enabled (needs ODBC version >= 3.51)])

    if test "$ac_use_odbc" = "no"; then
        AC_MSG_RESULT(no)

    else
        if test "$ac_use_odbc" = try ;
        then
            AC_MSG_RESULT(try)
        elif test "$ac_use_odbc" = yes -o \
                  "$ac_use_odbc" = "" ; 
        then
            AC_MSG_RESULT(yes)
        else
            AC_MSG_RESULT([check in $ac_use_odbc])
        fi
        AC_FIND_ODBC
        if test ! "$oasys_cv_lib_odbc" = "" ; then
            LIBODBC_ENABLED=1
            AC_DEFINE_UNQUOTED(LIBODBC_ENABLED, 1,
                [whether odbc is enabled])
            AC_EXTLIB_PREPARE
            if test ! $oasys_cv_path_odbc_h = /usr/include ; then
                CFLAGS="-I$oasys_cv_path_odbc_h"
            fi
            
            if test ! $oasys_cv_path_odbc_lib = /usr/lib ; then
                LDFLAGS="-L$oasys_cv_path_odbc_lib"
            fi

            LDFLAGS="$LDFLAGS -l$oasys_cv_lib_odbc"
            AC_EXTLIB_SAVE
        else
            LIBODBC_ENABLED=0
        fi

    fi
])

dnl
dnl Find odbc
dnl
AC_DEFUN(AC_FIND_ODBC, [

    oasys_cv_path_odbc_h=
    oasys_cv_path_odbc_lib=
    oasys_cv_lib_odbc=

    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"
    ac_save_LIBS="$LIBS"


    if test "$ac_use_odbc" = yes -o \
            "$ac_use_odbc" = try -o \
            "$ac_use_odbc" = "" ; 
    then
        ac_odbcincdirs="/usr/include     /usr/local/include"
        ac_odbclibdirs="/usr/lib     /usr/local/lib"
    else
        ac_odbcincdirs="$ac_use_odbc/include"
        ac_odbclibdirs="$ac_use_odbc/lib"
    fi

    for ac_odbcincdir in $ac_odbcincdirs; do
    for ac_odbclibdir in $ac_odbclibdirs; do
        ac_odbclib="odbc"

        CPPFLAGS="-I$ac_odbcincdir"
        LDFLAGS="-L$ac_odbclibdir"
        if test x"$STATIC" = x"extlibs" ; then
            LIBS="-Wl,-Bstatic -l$ac_odbclib -Wl,-Bdynamic $ac_save_LIBS"
        else
            LIBS="-l$ac_odbclib $ac_save_LIBS"
        fi

        AC_MSG_CHECKING([for ODBC in $ac_odbcincdir, $ac_odbclibdir, -l$ac_odbclib])
        AC_LINK_IFELSE(
            AC_LANG_PROGRAM(
                [
                #include "$ac_odbcincdir/sql.h"
                #include "$ac_odbcincdir/sqlext.h"
                #include "$ac_odbcincdir/sqltypes.h"
                #include "$ac_odbcincdir/sqlucode.h"
                ],
                [
                SQLHENV henv;
                SQLAllocHandle( SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv );
                SQLFreeHandle( SQL_HANDLE_ENV, henv );
                ]),

                [
                AC_MSG_RESULT([yes])
                oasys_cv_path_odbc_h=$ac_odbcincdir
                oasys_cv_path_odbc_lib=$ac_odbclibdir
                oasys_cv_lib_odbc=$ac_odbclib
                # break 4
                ],
                [
                AC_MSG_RESULT([no])
                oasys_cv_path_odbc_h=
                oasys_cv_path_odbc_lib=
                oasys_cv_lib_odbc=
                break 
                ])
        AC_MSG_CHECKING([ODBC detected has version >= 3.51 in $ac_odbcincdir, $ac_odbclibdir, -l$ac_odbclib])
        AC_LINK_IFELSE(
            AC_LANG_PROGRAM(
                [
                #include "$ac_odbcincdir/sql.h"
                
                #if (ODBCVER < 0x0351)
                #error "ODBC version is too early"
                #endif
                ],
                [
                ]),

                [
                AC_MSG_RESULT([yes])
                break 4
                ],
                [
                AC_MSG_RESULT([no])
                oasys_cv_path_odbc_h=
                oasys_cv_path_odbc_lib=
                oasys_cv_lib_odbc=
                break
                ])
    done
    done

    CPPFLAGS="$ac_save_CPPFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
    LIBS="$ac_save_LIBS"

    AC_MSG_CHECKING([whether libodbc was found])
    if test ! x$oasys_cv_path_odbc_h = x ; then
        AC_MSG_RESULT(yes)
    elif test $ac_use_odbc = try ; then
        AC_MSG_RESULT(no)
    else
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([can't find usable odbc installation])
    fi
])
dnl
dnl    Copyright 2005-2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl
dnl Autoconf support for finding postgres
dnl

dnl
dnl Main macro for finding a usable postgres installation 
dnl
AC_DEFUN(AC_CONFIG_POSTGRES, [
    ac_postgresdir='no'

    AC_ARG_WITH(postgres,
        AC_HELP_STRING([--with-postgres=DIR],
    		   [location of a postgres installation (default none)]),
        ac_postgresdir=$withval) 

    dnl
    dnl First make sure we even want it
    dnl
    if test "$ac_postgresdir" = no ; then
    POSTGRES_ENABLED=0
    else

    POSTGRES_ENABLED=1
    AC_DEFINE_UNQUOTED(POSTGRES_ENABLED, 1, 
        [whether postgres support is enabled])

    dnl
    dnl Now check if we have a cached value, unless the user specified
    dnl something explicit with the --with-postgres= argument, in
    dnl which case we force it to redo the checks (i.e. ignore the
    dnl cached values)
    dnl
    if test "$ac_postgresdir" = yes -a ! x$oasys_cv_path_postgres_h = x ; then
        echo "checking for postgres installation... (cached) $oasys_cv_path_postgres_h/libpq-fe.h, $oasys_cv_path_postgres_lib -lpq"
    else
        AC_OASYS_FIND_POSTGRES
    fi

    if test ! $oasys_cv_path_postgres_h = /usr/include ; then
        EXTLIB_CFLAGS="$EXTLIB_CFLAGS -I$oasys_cv_path_postgres_h"
    fi

    if test ! $oasys_cv_path_postgres_lib = /usr/lib ; then
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -L$oasys_cv_path_postgres_lib"
    fi

    EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -lpq"
    
    fi # POSTGRES_ENABLED
])

dnl
dnl Find postgres
dnl
AC_DEFUN(AC_OASYS_FIND_POSTGRES, [
    oasys_cv_path_postgres_h=
    oasys_cv_path_postgres_lib=

    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"
    ac_save_LIBS="$LIBS"

    if test "$ac_postgresdir" = system -o \
            "$ac_postgresdir" = yes -o \
            "$ac_postgresdir" = "" ; 
    then
        ac_postgresincdirs="/usr/include /usr/include/postgresql \
                            /usr/local/pgsql/include"
        ac_postgreslibdirs="/usr/lib /usr/lib/pgsql /usr/local/pgsql/lib"
    else
        ac_postgresincdirs="$ac_postgresdir/include"
        ac_postgreslibdirs="$ac_postgresdir/lib"
    fi

    for ac_postgresincdir in $ac_postgresincdirs; do

	CPPFLAGS="$ac_save_CPPFLAGS -I$ac_postgresincdir"
	LDFLAGS="$ac_save_LDFLAGS"
	LIBS="$ac_save_LIBS"

	dnl
	dnl First check the version in the header file. If there's a match, 
	dnl fall through to the other check to make sure it links.
	dnl If not, then we can break out of the two inner loops.
	dnl
        AC_MSG_CHECKING([for postgres header libpq-fe.h in $ac_postgresincdir])
	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <libpq-fe.h>
            ],
            
            [
            ]),
          [ 
	      AC_MSG_RESULT([yes])
              oasys_cv_path_postgres_h=$ac_postgresincdir
	      break
          ],
          [
              AC_MSG_RESULT([no])
	      continue
          ])
    done

    if test x$oasys_cv_path_postgres_h = x ; then
        AC_MSG_ERROR([can't find usable postgres installation])
    fi

    for ac_postgreslibdir in $ac_postgreslibdirs; do

	LDFLAGS="$ac_save_LDFLAGS -L$ac_postgreslibdir"
	LIBS="$ac_save_LIBS -lpq"

        AC_MSG_CHECKING([for postgres library libpq in $ac_postgreslibdir])
	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <libpq-fe.h>
            ],
            
            [
	        PGconn *conn = PQconnectStart("");
            ]),

          [
              AC_MSG_RESULT([yes])
              oasys_cv_path_postgres_lib=$ac_postgreslibdir
              break
          ],
          [
              AC_MSG_RESULT([no])
          ])
    done

    CPPFLAGS="$ac_save_CPPFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
    LIBS="$ac_save_LIBS"

    if test x$oasys_cv_path_postgres_lib = x ; then
        AC_MSG_ERROR([can't find usable postgres library])
    fi
])
##### http://autoconf-archive.cryp.to/ax_with_python.html
#
# SYNOPSIS
#
#   AX_WITH_PYTHON([minimum-version], [value-if-not-found], [path])
#
# DESCRIPTION
#
#   Locates an installed Python binary, placing the result in the
#   precious variable $PYTHON. Accepts a present $PYTHON, then
#   --with-python, and failing that searches for python in the given
#   path (which defaults to the system path). If python is found,
#   $PYTHON is set to the full path of the binary; if it is not found,
#   $PYTHON is set to VALUE-IF-NOT-FOUND, which defaults to 'python'.
#
#   Example:
#
#     AX_WITH_PYTHON(2.2, missing)
#
# LAST MODIFICATION
#
#   2007-07-29
#
# COPYLEFT
#
#   Copyright (c) 2007 Dustin J. Mitchell <dustin@cs.uchicago.edu>
#
#   Copying and distribution of this file, with or without
#   modification, are permitted in any medium without royalty provided
#   the copyright notice and this notice are preserved.

AC_DEFUN([AX_WITH_PYTHON],
[
  AC_ARG_VAR([PYTHON])

  dnl unless PYTHON was supplied to us (as a precious variable)
  if test -z "$PYTHON"
  then
    AC_MSG_CHECKING(for --with-python)
    AC_ARG_WITH(python,
                AC_HELP_STRING([--with-python=PYTHON],
                               [absolute path name of Python executable]),
                [ if test "$withval" != "yes"
                  then
                    PYTHON="$withval"
                    AC_MSG_RESULT($withval)
                  else
                    AC_MSG_RESULT(no)
                  fi
                ],
                [ AC_MSG_RESULT(no)
                ])
  fi

  dnl if it's still not found, check the paths, or use the fallback
  if test -z "$PYTHON"
  then
    AC_PATH_PROG([PYTHON], python, m4_ifval([$2],[$2],[python]), $3)
  fi

  dnl check version if required
  m4_ifvaln([$1], [
    dnl do this only if we didn't fall back
    if test "$PYTHON" != "m4_ifval([$2],[$2],[python])"
    then
      AC_MSG_CHECKING($PYTHON version >= $1)
      if test `$PYTHON -c ["import sys; print sys.version[:3] >= \"$1\" and \"OK\" or \"OLD\""]` = "OK"
      then
        AC_MSG_RESULT(ok)
      else
        AC_MSG_RESULT(no)
        PYTHON="$2"
      fi
    fi])
])

dnl
dnl Simple autoconf test for whether distutils is properly set up to
dnl build an extension module. 
dnl
AC_DEFUN(AC_PYTHON_BUILD_EXT, [
    
    if test ! -z "$PYTHON" ; then

    AC_MSG_CHECKING([whether python distutils can build an extension module])

    cat > conftest.c <<_ACEOF
[
#include <Python.h>

int main(int argc, char** argv)
{
    Py_InitModule("conftest", NULL);
}
]
_ACEOF

    cat > conftest.py <<_ACEOF
[
from distutils.core import setup, Extension
setup(name='conftest',
      version='1.0',
      ext_modules=[Extension('_conftest', ['conftest.c'])]
      )
]
_ACEOF

     # Current versions of autoconf access config.log 
     # through file descriptor 5, so use it here to
     # redirect output from the build operation.
     _ACCMD="$PYTHON conftest.py build -b conftest_build"
     echo "$as_me:$LINENO: running $_ACCMD... output:" >&5
     $_ACCMD >&5 2>&5
     ac_python_build=$?
     
     if test $ac_python_build = 0 ; then
     	AC_MSG_RESULT([yes])
	PYTHON_BUILD_EXT=yes
     	AC_SUBST(PYTHON_BUILD_EXT)
     else
        AC_MSG_RESULT([no])
     fi
     
     rm -rf conftest_build conftest.py conftest.c
     
     fi # PYTHON != ""
])
dnl
dnl    Copyright 2005-2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl
dnl Autoconf support for configuring the various storage options
dnl that really just dispatches to the other macros
dnl

AC_DEFUN(AC_CONFIG_STORAGE, [
    AC_CONFIG_DB
    AC_CONFIG_MYSQL
    AC_CONFIG_POSTGRES
    AC_CONFIG_EXTERNAL_DS

    dnl
    dnl Figure out if at least one sql option is enabled.
    dnl
    if test "$MYSQL_ENABLED" = "1" -o "$POSTGRES_ENABLED" = "1" ; then
        AC_DEFINE_UNQUOTED(SQL_ENABLED, 1,
             [whether some sql storage system is enabled])
    fi
])
dnl
dnl    Copyright 2005-2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl -------------------------------------------------------------------------
dnl Checks for system programs
dnl -------------------------------------------------------------------------
AC_DEFUN(AC_OASYS_SYSTEM_PROGRAMS, [
    AC_PROG_AWK
    AC_PROG_INSTALL
    AC_PROG_MAKE_SET
    AC_PROG_RANLIB
])

dnl -------------------------------------------------------------------------
dnl Checks for system libraries
dnl -------------------------------------------------------------------------
AC_DEFUN(AC_OASYS_SYSTEM_LIBRARIES, [
    AC_EXTLIB_PREPARE

    AC_SEARCH_LIBS(pthread_create, pthread, [], 
      AC_MSG_ERROR([can't find required library function (pthread_create)]))

    AC_SEARCH_LIBS(pthread_yield, [rt pthread],
                   AC_DEFINE_UNQUOTED(HAVE_PTHREAD_YIELD, 1, [wether pthread_yield exists]),
                   [])

    AC_SEARCH_LIBS(sched_yield, [rt],
                   AC_DEFINE_UNQUOTED(HAVE_SCHED_YIELD, 1, [wether sched_yield exists]),
                   [])

    AC_SEARCH_LIBS(pthread_setspecific, pthread, 
                   AC_DEFINE_UNQUOTED(HAVE_PTHREAD_SETSPECIFIC, 1, 
                                      [whether pthread_setspecific is defined]), 
                   [])

    AC_SEARCH_LIBS(socket, socket, [],
      AC_MSG_ERROR([can't find required library function (socket)]))

    AC_SEARCH_LIBS(gethostbyname, [socket nsl xnet],
                   AC_DEFINE_UNQUOTED(HAVE_GETHOSTBYNAME, 1, [wether gethostbyname exists]),
                   [])

    AC_SEARCH_LIBS(gethostbyname_r, [socket nsl],
                   AC_DEFINE_UNQUOTED(HAVE_GETHOSTBYNAME_R, 1, [wether gethostbyname_r exists]),
                   [])

    AC_SEARCH_LIBS(xdr_int, rpc, [],
      AC_MSG_ERROR([can't find required library function (xdr_int)]))

    AC_SEARCH_LIBS(xdr_u_int64_t, rpc,
                   AC_DEFINE_UNQUOTED(HAVE_XDR_U_INT64_T, 1, [whether xdr_u_int64_t exists]),
                   [])

    AC_SEARCH_LIBS(xdr_u_quad_t, rpc,
                   AC_DEFINE_UNQUOTED(HAVE_XDR_U_QUAD_T, 1, [whether xdr_u_quad_t exists]),
                   [])

    AC_SEARCH_LIBS(inet_aton, [nsl resolv socket],
                   AC_DEFINE_UNQUOTED(HAVE_INET_ATON, 1, [wether inet_aton exists]),
                   [])

    AC_SEARCH_LIBS(inet_pton, [nsl resolv socket],
                   AC_DEFINE_UNQUOTED(HAVE_INET_PTON, 1, [wether inet_pton exists]),
                   [])

    AC_EXTLIB_SAVE
])

dnl -------------------------------------------------------------------------
dnl Checks for header files.
dnl -------------------------------------------------------------------------
AC_DEFUN(AC_OASYS_SYSTEM_HEADERS, [
    AC_HEADER_STDC
    AC_CHECK_HEADERS([err.h execinfo.h stdint.h string.h synch.h sys/cdefs.h sys/types.h])
])

dnl -------------------------------------------------------------------------
dnl Checks for typedefs, structures, and compiler characteristics.
dnl -------------------------------------------------------------------------
AC_DEFUN(AC_OASYS_SYSTEM_TYPES, [
    AC_C_CONST
    AC_C_INLINE
    AC_C_VOLATILE
    AC_TYPE_MODE_T
    AC_MSG_CHECKING([value for _FILE_OFFSET_BITS preprocessor symbol])
    ac_file_offset_bits=64
    AC_ARG_WITH(file-offset-bits,
	AC_HELP_STRING([--with-file-offset-bits=N],
		       [value for _FILE_OFFSET_BITS (default 64)]),
		       ac_file_offset_bits=$withval)
    AC_MSG_RESULT($ac_file_offset_bits)
    if test $ac_file_offset_bits = 64 ; then
        SYS_CFLAGS="-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
    else 
        SYS_CFLAGS="-D_FILE_OFFSET_BITS=$ac_file_offset_bits"
    fi
    AC_SUBST(SYS_CFLAGS)
    oasys_ac_cppflags_save="$CPPFLAGS"
    CPPFLAGS="$SYS_CFLAGS $CPPFLAGS"
    AC_CHECK_SIZEOF(off_t)
    CPPFLAGS="$oasys_ac_cppflags_save"
    AC_TYPE_SIGNAL
    AC_TYPE_SIZE_T
    AC_CHECK_TYPES([ptrdiff_t])
    AC_CHECK_TYPES([uint32_t])
    AC_CHECK_TYPES([u_int32_t])
])

dnl -------------------------------------------------------------------------
dnl Checks for library functions.
dnl -------------------------------------------------------------------------
AC_DEFUN(AC_OASYS_SYSTEM_FUNCTIONS, [
    # XXX/demmer get rid of me
    AC_CHECK_FUNCS([getaddrinfo getopt_long cfmakeraw cfsetspeed])
])                                


AC_DEFUN(AC_OASYS_SYSTEM_FDATASYNC, [
    AC_CACHE_CHECK([working fdatasync],[ac_cv_func_fdatasync],[
     ÊAC_LANG_PUSH(C++)
     ÊAC_RUN_IFELSE([AC_LANG_PROGRAM([[
    #include <unistd.h>
     Ê Ê]],[[
    fdatasync(4);
     Ê Ê]])],
     Ê[ac_cv_func_fdatasync=yes],
     Ê[ac_cv_func_fdatasync=no])
     ÊAC_LANG_POP()
    ])
    AS_IF([test "x${ac_cv_func_fdatasync}" = "xyes"],
     Ê[AC_DEFINE([HAVE_FDATASYNC],[1],[If the system has a working fdatasync])])
])                                

dnl -------------------------------------------------------------------------
dnl Check all the system requirements
dnl -------------------------------------------------------------------------
AC_DEFUN(AC_OASYS_CONFIG_SYSTEM, [
    AC_OASYS_SYSTEM_PROGRAMS
    AC_OASYS_SYSTEM_LIBRARIES
    AC_OASYS_SYSTEM_HEADERS
    AC_OASYS_SYSTEM_TYPES
    AC_OASYS_SYSTEM_FUNCTIONS
])

dnl
dnl    Copyright 2005-2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl
dnl Autoconf support for finding tcl
dnl

dnl
dnl Main macro for finding a usable tcl installation 
dnl
AC_DEFUN(AC_CONFIG_TCL, [
    ac_tclvers='8.6 8.5 8.4 8.3'
    ac_tcldir='system'

    AC_ARG_WITH(tcl,
        AC_HELP_STRING([--with-tcl=DIR],
    		   [location of a tcl installation (default system)]),
        ac_tcldir=$withval) 

    AC_ARG_WITH(tcldir,
        AC_HELP_STRING([--with-tcldir=DIR],
    		   [same as --with-tcl]),
        ac_tcldir=$withval) 

    AC_ARG_WITH(tclver,
        AC_HELP_STRING([--with-tclver=VERSION],
    		   [tcl version to use (default 8.5 - 8.3)]),
        ac_tclvers=$withval)

    dnl
    dnl We don't accept --without-tcl
    dnl
    if test $ac_tcldir = no ; then
       AC_MSG_ERROR([tcl MUST be installed to configure])
    fi

    dnl
    dnl Tcl requires other libraries
    dnl
    AC_EXTLIB_PREPARE
    AC_SEARCH_LIBS(pow, m, [],
      AC_MSG_ERROR([can't find standard C library function (pow) required by tcl]))
    AC_SEARCH_LIBS(dlopen, dl, [],
      AC_MSG_ERROR([can't find standard library function (dlopen) required by tcl]))
    AC_EXTLIB_SAVE

    dnl
    dnl First check if we have a cached value, unless the user
    dnl specified something with --with-tcl, in which case we force 
    dnl it to redo the checks (i.e. ignore the cached values).
    dnl
    if test $ac_tcldir = system -a ! x$oasys_cv_path_tcl_h = x ; then \
	echo "checking for tcl installation... (cached) $oasys_cv_path_tcl_h/tcl.h, $oasys_cv_path_tcl_lib -l$oasys_cv_lib_tcl"
    else
        AC_FIND_TCL
    fi

    TCL_CFLAGS=        
    if test ! $oasys_cv_path_tcl_h = /usr/include ; then
	TCL_CFLAGS=-I$oasys_cv_path_tcl_h
        EXTLIB_CFLAGS="$EXTLIB_CFLAGS $TCL_CFLAGS"
    fi

    TCL_LDFLAGS=
    if test ! x"$oasys_cv_path_tcl_lib" = x"/usr/lib" ; then
	TCL_LDFLAGS="-L$oasys_cv_path_tcl_lib -l$oasys_cv_lib_tcl"
    else
	TCL_LDFLAGS=-l$oasys_cv_lib_tcl
    fi

    EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS $TCL_LDFLAGS"
    AC_SUBST(TCL_LDFLAGS)

    dnl
    dnl Check for some APIs
    dnl
    AC_SEARCH_LIBS(Tcl_GetErrorLine, tcl,
            [AC_DEFINE([HAVE_TCL_GETERRORLINE], [1], [Define if Tcl has Tcl_GetErrorLine function.])])
])

dnl
dnl Find tcl
dnl
AC_DEFUN(AC_FIND_TCL, [
    oasys_cv_path_tcl_h=

    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"
    ac_save_LIBS="$LIBS"

    if test $ac_tcldir = system -o $ac_tcldir = yes -o $ac_tcldir = "" ; then
        ac_tcldir=system
        ac_tclincdirs="/usr/include /usr/local/include"
	for ac_tclver in $ac_tclvers ; do
           ac_tclincdirs="$ac_tclincdirs /usr/include/tcl$ac_tclver /usr/local/include/tcl$ac_tclver"
           ac_tclincdirs="$ac_tclincdirs"
       done
        ac_tcllibdirs="/usr/lib /usr/local/lib"
    else
        ac_tclincdirs=$ac_tcldir/include
        ac_tcllibdirs="$ac_tcldir/lib"
    fi

    for ac_tclver in $ac_tclvers ; do	
        echo ""
        AC_MSG_NOTICE([checking for tcl installation (version $ac_tclver)])

    for ac_tclincdir in $ac_tclincdirs; do

        ac_tclver_major=`echo $ac_tclver | cut -d . -f1`
	ac_tclver_minor=`echo $ac_tclver | cut -d . -f2`
	CPPFLAGS="$ac_save_CPPFLAGS -I$ac_tclincdir"
	LDFLAGS="$ac_save_LDFLAGS"
	LIBS="$ac_save_LIBS"
	
	dnl
	dnl First check the version in the header file. If there's a match, 
	dnl fall through to the other check to make sure it links.
	dnl If not, then we can break out of the two inner loops.
	dnl
        AC_MSG_CHECKING([for tcl.h (version $ac_tclver) in $ac_tclincdir])

	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <tcl.h>
           
                #if (TCL_MAJOR_VERSION != ${ac_tclver_major}) || \
                    (TCL_MINOR_VERSION != ${ac_tclver_minor})
                #error "incorrect version"
                #endif
            ],
            
            [
            ]),

          [
              AC_MSG_RESULT([yes])
          ],
          [
              AC_MSG_RESULT([no])
              continue
          ])

      for ac_tcllibdir in $ac_tcllibdirs; do
      for ac_tcllib in tcl$ac_tclver tcl$ac_tclver_major$ac_tclver_minor tcl; do

	LDFLAGS="$ac_save_LDFLAGS -L$ac_tcllibdir"
    	if test x"$STATIC" = x"extlibs" ; then
		LIBS="-Wl,-Bstatic -l$ac_tcllib -Wl,-Bdynamic -ldl -lm $ac_save_LIBS"
	else
		LIBS="-l$ac_tcllib $ac_save_LIBS"
	fi

        AC_MSG_CHECKING([for tcl library in $ac_tcllibdir: $LIBS])

	AC_LINK_IFELSE(
	  AC_LANG_PROGRAM(
	    [
                #include <tcl.h>
           
                #if (TCL_MAJOR_VERSION != ${ac_tclver_major}) || \
                    (TCL_MINOR_VERSION != ${ac_tclver_minor})
                #error "incorrect version"
                #endif
            ],
            
            [
                Tcl_Interp* interp = Tcl_CreateInterp();
            ]),

          [
              AC_MSG_RESULT([yes])
              oasys_cv_path_tcl_h=$ac_tclincdir
              oasys_cv_path_tcl_lib=$ac_tcllibdir
              oasys_cv_lib_tcl=$ac_tcllib

              #Buggy 
              #if test ! x"$STATIC" = x"yes" ; then
                  #oasys_cv_path_tcl_lib="${ac_tcllibdir} -Wl,-rpath,${ac_tcllibdir}"
                  #oasys_cv_lib_tcl="${ac_tcllib} -Wl,-rpath,${ac_tcllibdir}"
              #fi

              break 4
          ],
          [
              AC_MSG_RESULT([no])
          ])
    done
    done
    done
    done

    CPPFLAGS="$ac_save_CPPFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
    LIBS="$ac_save_LIBS"

    if test x$oasys_cv_path_tcl_h = x ; then
        AC_MSG_ERROR([Please install the tcl development files headers and libraries. Can't find usable tcl.h ])
    fi
])
dnl
dnl    Copyright 2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl
dnl Autoconf support for tclreadline
dnl

dnl
dnl help 
dnl
AC_DEFUN(AC_TCLRL_HELP, [
cat <<EOF

Configure error with tclreadline...

If you do not want tclreadline support at all, you can specify
--without-tclreadline.

Download tclreadline from http://tclreadline.sourceforge.net/

Before trying to compile tclreadline you should do the following things:

    (a) Make sure you have tcl 8.0 or higher.
        tclreadline relies on a proper tcl installation:
        It uses the tclConfig.sh file, which should reside somewhere
        in /usr/local/lib/ or /usr/local/lib/tcl8.0/...

    (b) Make sure you have gnu readline 2.2 or higher.
        tclreadline uses the gnu readline callback handler

    (c) The usual ./configure; make; make install sequence should do the rest.


EOF

])
dnl
dnl Main macro for finding a usable db installation 
dnl
AC_DEFUN(AC_CONFIG_TCLREADLINE, [
#ac_tclreadlinedir='system'

    AC_ARG_WITH(tclreadline,
        AC_HELP_STRING([--with-tclreadline],
    		   [enable or disable tclreadline (default try)]),
        [ac_tclreadline=$withval],
        [ac_tclreadline=try])

    dnl
    dnl First make sure we even want it
    dnl
	AC_MSG_CHECKING([whether tclreadline support should be enabled])
    if test "$ac_tclreadline" = no ; then
    AC_MSG_RESULT([no])
    TCLREADLINE_ENABLED=0

    else

    dnl
    dnl If we do, tell the user what we're about to do
    dnl    
    if test "$ac_tclreadline" = try ; then
       AC_MSG_RESULT([try])
    else
       AC_MSG_RESULT([yes])
    fi

    dnl
    dnl Now check for an installation of libreadline (except on Cygwin)
    dnl
    if test "$BUILD_SYSTEM" = 'CYGWIN' ; then
        TCLREADLINE_ENABLED=0
    else 
    dnl	
    dnl look for the library and the header
    dnl
	AC_EXTLIB_PREPARE
        AC_MSG_CHECKING([searching for the readline library development headers])
        AC_CHECK_HEADERS([readline/readline.h], TCLREADLINE_ENABLED=1, TCLREADLINE_ENABLED=0)
	AC_SEARCH_LIBS(readline, readline, TCLREADLINE_ENABLED=1, TCLREADLINE_ENABLED=0)
        AC_EXTLIB_SAVE
    fi

    dnl
    dnl Find out if we're using the BSD libedit implementation of 
    dnl readline functionality.
    dnl
    if test "$TCLREADLINE_ENABLED" = 1; then
         AC_CACHE_CHECK([whether readline is GNU readline or BSD editline],
	  	        [oasys_cv_readline_is_editline],
 		        [AC_LINK_IFELSE([AC_LANG_CALL([], [rl_extend_line_buffer])],
 		       		        [oasys_cv_readline_is_editline=GNU],
 		       		        [oasys_cv_readline_is_editline=editline])])
        if test $oasys_cv_readline_is_editline = editline ; then
            AC_DEFINE_UNQUOTED(READLINE_IS_EDITLINE, 1,
                               [whether readline is actually BSD's libedit])
        fi

	dnl Finally, spit out an informative message
	AC_MSG_CHECKING([whether tclreadline support was found])
        AC_MSG_RESULT([yes])

    else

	dnl Finally, spit out an informative message
	AC_MSG_CHECKING([whether tclreadline support was found])
	AC_MSG_RESULT([no])
	if test "$ac_tclreadline" = yes; then
	AC_TCLRL_HELP
	AC_MSG_ERROR([can't find usable tclreadline])
	fi
    fi

    fi # ac_tclreadline != no

    AC_DEFINE_UNQUOTED(TCLREADLINE_ENABLED, $TCLREADLINE_ENABLED,
                       [whether tclreadline is enabled])
])
dnl 
dnl   Copyright 2006-2007 The MITRE Corporation
dnl
dnl   Licensed under the Apache License, Version 2.0 (the "License");
dnl   you may not use this file except in compliance with the License.
dnl   You may obtain a copy of the License at
dnl
dnl       http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl   Unless required by applicable law or agreed to in writing, software
dnl   distributed under the License is distributed on an "AS IS" BASIS,
dnl   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl   See the License for the specific language governing permissions and
dnl   limitations under the License.
dnl
dnl   The US Government will not be charged any license fee and/or royalties
dnl   related to this software. Neither name of The MITRE Corporation; nor the
dnl   names of its contributors may be used to endorse or promote products
dnl   derived from this software without specific prior written permission.
dnl

dnl
dnl Autoconf support for finding xerces-c
dnl

AC_DEFUN(AC_CONFIG_XERCES, [
    dnl
    dnl Handle --with-xerces-c=DIR
    dnl
    AC_ARG_WITH(xerces-c,
        AC_HELP_STRING([--with-xerces-c=DIR],
            [location of a xerces-c installation (default try)]),
        [ac_with_xerces_c=$withval],
        [ac_with_xerces_c=try])

    AC_MSG_CHECKING([whether xerces-c support should be enabled])

    dnl
    dnl Disable xerces-c if requested
    dnl
    if test "$ac_with_xerces_c" = no; then
        AC_MSG_RESULT([no])
        XERCES_C_ENABLED=0
	echo ""
	AC_MSG_NOTICE([Proceeding without xerces-c support as requested])
	echo ""
    else

    dnl
    dnl Find the xerces-c installation
    dnl
    if test "$ac_with_xerces_c" = try \
         -o "$ac_with_xerces_c" = yes \ 
         -o "$ac_with_xerces_c" = "" ; then
        AC_MSG_RESULT([try])
        ac_xerces_inst_dirs="/usr /usr/local"
    else
        AC_MSG_RESULT([yes (dir $ac_with_xerces_c)])
        ac_xerces_inst_dirs="$ac_with_xerces_c"
    fi

    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"
    ac_save_LIBS="$LIBS"

    AC_MSG_CHECKING([whether xerces-c (>= v2.6.0 and < v3.0.0) was found])
    AC_CACHE_VAL(oasys_cv_path_xerces_c,
    [
        for ac_xerces_inst_dir in $ac_xerces_inst_dirs; do
            if test -d "$ac_xerces_inst_dir"; then
                AC_LANG([C++])
                CPPFLAGS="-I$ac_xerces_inst_dir/include"
                LDFLAGS="-L$ac_xerces_inst_dir/lib"
                LIBS="-lxerces-c $LIBS"

                AC_LINK_IFELSE(
                    AC_LANG_PROGRAM(
                        [
                            #include <xercesc/util/PlatformUtils.hpp>
                            #include <xercesc/util/XMLString.hpp>
                            #include <xercesc/dom/DOM.hpp>
                        ],
    
                        [
			    #if _XERCES_VERSION >= 30000
				#error
			    #else
				#if _XERCES_VERSION >= 20600

				xercesc::XMLPlatformUtils::Initialize();
				{
				    xercesc::DOMImplementation* impl
					= xercesc::DOMImplementationRegistry::getDOMImplementation
							(xercesc::XMLString::transcode("XML 1.0"));
				}
				xercesc::XMLPlatformUtils::Terminate();

				#else
					#error
				#endif
                            #endif
                        ]),
                    [
                        oasys_cv_path_xerces_c="$ac_xerces_inst_dir"
                        break
                    ],
                    [
                        oasys_cv_path_xerces_c=
                    ]
                )
            fi
        done
    ])

    CPPFLAGS="$ac_save_CPPFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
    LIBS="$ac_save_LIBS"

    if test -z "$oasys_cv_path_xerces_c"; then
        AC_MSG_RESULT([no])
        XERCES_C_ENABLED=0
    else
	AC_MSG_RESULT([yes])
        XERCES_C_ENABLED=1
        AC_DEFINE(XERCES_C_ENABLED, 1, [whether xerces support is enabled])
        if test ! "$oasys_cv_path_xerces_c" = /usr ; then
            EXTLIB_CFLAGS="$EXTLIB_CFLAGS -I$oasys_cv_path_xerces_c/include"
            EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -L$oasys_cv_path_xerces_c/lib"
        fi
        EXTLIB_LDFLAGS="$EXTLIB_LDFLAGS -lxerces-c"
    fi

    fi
])
dnl Copyright 2004-2006 BBN Technologies Corporation
dnl
dnl Licensed under the Apache License, Version 2.0 (the "License"); you may not
dnl use this file except in compliance with the License. You may obtain a copy
dnl of the License at http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
dnl WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl
dnl See the License for the specific language governing permissions and
dnl limitations under the License.
dnl

dnl
dnl Autoconf support for finding xsd
dnl

AC_DEFUN(AC_CONFIG_XSD, [
    dnl
    dnl Handle --with-xsd-tool=xsd
    dnl
    AC_ARG_WITH(xsd-tool,
        AC_HELP_STRING([--with-xsd-tool=xsd],
            [name or path of the xsd tool (default: xsd)]),
        [ac_with_xsd_tool=$withval],
        [ac_with_xsd_tool=xsd])

    AC_CHECK_TOOL(XSD_TOOL, $ac_with_xsd_tool)
    if test -z "$XSD_TOOL" ; then
       AC_MSG_NOTICE([Cannot find a working xsd tool])
       AC_MSG_NOTICE([   You will not be able to regenerate XML schema bindings])
       AC_MSG_NOTICE([   if you make changes to the .xsd file])
       AC_MSG_NOTICE([   Use --with-xsd-tool=(name) to specify the location of this tool])
    fi
])
dnl
dnl    Copyright 2006 Intel Corporation
dnl 
dnl    Licensed under the Apache License, Version 2.0 (the "License");
dnl    you may not use this file except in compliance with the License.
dnl    You may obtain a copy of the License at
dnl 
dnl        http://www.apache.org/licenses/LICENSE-2.0
dnl 
dnl    Unless required by applicable law or agreed to in writing, software
dnl    distributed under the License is distributed on an "AS IS" BASIS,
dnl    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl    See the License for the specific language governing permissions and
dnl    limitations under the License.
dnl

dnl 
dnl Autoconf support for configuring whether Zlib is available
dnl on the system
dnl

AC_DEFUN(AC_CONFIG_ZLIB, [

    AC_ARG_WITH(zlib,
      [AC_HELP_STRING([--with-zlib],
                      [compile in zlib support (default try)])],
      [ac_use_zlib=$withval],
      [ac_use_zlib=try])
    
    AC_MSG_CHECKING([whether zlib support should be enabled])

    if test "$ac_use_zlib" = "no"; then
        AC_MSG_RESULT(no)

    else
        AC_MSG_RESULT($ac_use_zlib)

        dnl
        dnl Look for the compress() and compressBound() functions in libz
        dnl
        AC_EXTLIB_PREPARE
        AC_SEARCH_LIBS(compress, z, ac_has_libz="yes") 
        AC_SEARCH_LIBS(compressBound, z, ac_zlib_has_compressBound="yes") 
        AC_EXTLIB_SAVE

        dnl
        dnl Print out whether or not we found the libraries
        dnl
        AC_MSG_CHECKING([whether zlib support was found])

        dnl
        dnl Check which defines, if any, are set
        dnl
        if test "$ac_has_libz" = yes ; then
          AC_DEFINE(OASYS_ZLIB_ENABLED, 1,
              [whether zlib support is enabled])
          AC_MSG_RESULT(yes)

	  if test "$ac_zlib_has_compressBound" = yes ; then
	      AC_DEFINE(OASYS_ZLIB_HAS_COMPRESS_BOUND, 1,
                  [whether zlib contains compressBound])
	  fi

	elif test "$ac_use_zlib" = "try" ; then
          AC_MSG_RESULT(no)

        else
          AC_MSG_ERROR([can't find zlib library])
        fi
    fi
])
