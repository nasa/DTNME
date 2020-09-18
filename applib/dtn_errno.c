/*
 *    Copyright 2007 Intel Corporation
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

#include <stdio.h>
#include "dtn_errno.h"

//----------------------------------------------------------------------
char*
dtn_strerror(int err)
{
    switch(err) {
    case DTN_SUCCESS: 	return "success";
    case DTN_EINVAL: 	return "invalid argument";
    case DTN_EXDR: 	return "error in xdr routines";
    case DTN_ECOMM: 	return "error in ipc communication";
    case DTN_ECONNECT: 	return "error connecting to server";
    case DTN_ETIMEOUT: 	return "operation timed out";
    case DTN_ESIZE: 	return "payload too large";
    case DTN_ENOTFOUND: return "not found";
    case DTN_EINTERNAL: return "internal error";
    case DTN_EINPOLL:   return "illegal operation called after dtn_poll";
    case DTN_EBUSY:     return "registration already in use";
    case DTN_EVERSION:  return "ipc version mismatch";
    case DTN_EMSGTYPE:  return "unknown ipc message type";
    case DTN_ENOSPACE:	return "no storage space";
    case -1:            return "(invalid error code -1)";
    }

    // there's a small race condition here in case there are two
    // simultaneous calls that will clobber the same buffer, but this
    // should be rare and the worst that happens is that the output
    // string is garbled
    static char buf[128];
    snprintf(buf, sizeof(buf), "(unknown error %d)", err);
    return buf;
}

