/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef DTN_ERRNO_H
#define DTN_ERRNO_H

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * DTN API error codes
 */
#define DTN_SUCCESS 	0 		/* ok */
#define DTN_ERRBASE 	128		/* Base error code */
#define DTN_EINVAL 	(DTN_ERRBASE+1) /* invalid argument */
#define DTN_EXDR 	(DTN_ERRBASE+2) /* error in xdr routines */
#define DTN_ECOMM 	(DTN_ERRBASE+3) /* error in ipc communication */
#define DTN_ECONNECT 	(DTN_ERRBASE+4) /* error connecting to server */
#define DTN_ETIMEOUT 	(DTN_ERRBASE+5) /* operation timed out */
#define DTN_ESIZE 	(DTN_ERRBASE+6) /* payload / eid too large */
#define DTN_ENOTFOUND 	(DTN_ERRBASE+7) /* not found (e.g. reg) */
#define DTN_EINTERNAL 	(DTN_ERRBASE+8) /* misc. internal error */
#define DTN_EINPOLL 	(DTN_ERRBASE+9) /* illegal op. called after dtn_poll */
#define DTN_EBUSY 	(DTN_ERRBASE+10) /* registration already in use */
#define DTN_EVERSION    (DTN_ERRBASE+11) /* ipc version mismatch */
#define DTN_EMSGTYPE    (DTN_ERRBASE+12) /* unknown message type */
#define DTN_ENOSPACE	(DTN_ERRBASE+13) /* no storage space */
#define DTN_ERRMAX 255

/**
 * Get a string value associated with the dtn error code.
 */
char* dtn_strerror(int err);

#ifdef  __cplusplus
}
#endif

#endif /* DTN_ERRNO_H */
