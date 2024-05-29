/*
 *    Copyright 2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/*
 * This file contains code copied from the FreeBSD distribution's
 * implementation of the generic XDR routines since there they are
 * non-standard for 64 bit integer types.
 */

//----------------------------------------------------------------------

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "xdr_int64_compat.h"

#include <rpc/rpc.h>

/*
 * XDR 64-bit integers
 */
bool_t
xdr_xint64_t(xdrs, llp)
	XDR *xdrs;
	int64_t *llp;
{
	u_long ul[2];

	switch (xdrs->x_op) {
	case XDR_ENCODE:
		ul[0] = (u_long)((u_int64_t)*llp >> 32) & 0xffffffff;
		ul[1] = (u_long)((u_int64_t)*llp) & 0xffffffff;
		if (XDR_PUTLONG(xdrs, (long *)&ul[0]) == FALSE)
			return (FALSE);
		return (XDR_PUTLONG(xdrs, (long *)&ul[1]));
	case XDR_DECODE:
		if (XDR_GETLONG(xdrs, (long *)&ul[0]) == FALSE)
			return (FALSE);
		if (XDR_GETLONG(xdrs, (long *)&ul[1]) == FALSE)
			return (FALSE);
		*llp = (int64_t)
		    (((u_int64_t)ul[0] << 32) | ((u_int64_t)ul[1]));
		return (TRUE);
	case XDR_FREE:
		return (TRUE);
	}
	/* NOTREACHED */
	return (FALSE);
}


/*
 * XDR unsigned 64-bit integers
 */
bool_t
xdr_u_xint64_t(xdrs, ullp)
	XDR *xdrs;
	u_int64_t *ullp;
{
	u_long ul[2];

	switch (xdrs->x_op) {
	case XDR_ENCODE:
		ul[0] = (u_long)(*ullp >> 32) & 0xffffffff;
		ul[1] = (u_long)(*ullp) & 0xffffffff;
		if (XDR_PUTLONG(xdrs, (long *)&ul[0]) == FALSE)
			return (FALSE);
		return (XDR_PUTLONG(xdrs, (long *)&ul[1]));
	case XDR_DECODE:
		if (XDR_GETLONG(xdrs, (long *)&ul[0]) == FALSE)
			return (FALSE);
		if (XDR_GETLONG(xdrs, (long *)&ul[1]) == FALSE)
			return (FALSE);
		*ullp = (u_int64_t)
		    (((u_int64_t)ul[0] << 32) | ((u_int64_t)ul[1]));
		return (TRUE);
	case XDR_FREE:
		return (TRUE);
	}
	/* NOTREACHED */
	return (FALSE);
}
