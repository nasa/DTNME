/*
 *    Copyright 2004-2006 Intel Corporation
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
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef DTN_IPC_H
#define DTN_IPC_H

#include <rpc/rpc.h>

#ifdef __CYGWIN__
#include <stdio.h>
#include <string.h>
#include <cygwin/socket.h>
#endif

#ifdef  __cplusplus
extern "C" {
#endif

/*************************************************************
 *
 * Internal structures and functions that implement the DTN
 * IPC layer. Generally, this is not exposed to applications.
 *
 *************************************************************/

/**
 * DTN IPC version. Just a simple number for now; we can refine it to
 * a major/minor version later if desired.
 *
 * This currently cannot exceed 16 bits in width.
 *
 * Make sure to bump this when changing any data structures, message
 * types, adding functions, etc.
 */
#define DTN_IPC_VERSION 7

/**
 * Default api ports. The handshake port is used for initial contact
 * with the daemon to establish a session, and the latter is used for
 * individual sessions.
 */
#define DTN_IPC_PORT 5010

/**
 * The maximum IPC message size (in bytes). Used primarily for
 * efficiency in buffer allocation since the transport uses TCP.
 */
#define DTN_MAX_API_MSG 65536

/**
 * State of a DTN IPC channel.
 */
struct dtnipc_handle {
    int sock;				///< Socket file descriptor
    int err;				///< Error code
    int in_poll;			///< Flag to register a call to poll()
    int debug;				///< Debug flag for channel
    char buf[DTN_MAX_API_MSG];		///< send/recv buffer
    XDR xdr_encode;			///< XDR encoder
    XDR xdr_decode;			///< XDR decoder
    unsigned int total_sent;		///< Counter for debugging
    unsigned int total_rcvd;		///< Counter for debugging
};

typedef struct dtnipc_handle dtnipc_handle_t;

/**
 * Type codes for api messages.
 */
typedef enum {
    DTN_OPEN	    		= 1,
    DTN_CLOSE			= 2,
    DTN_LOCAL_EID		= 3,
    DTN_REGISTER		= 4,
    DTN_UNREGISTER		= 5,
    DTN_FIND_REGISTRATION	= 6,
    DTN_CHANGE_REGISTRATION	= 7,
    DTN_BIND			= 8,
    DTN_UNBIND			= 9,
    DTN_SEND			= 10,
    DTN_RECV			= 11,
    DTN_ACK			= 12,
    DTN_BEGIN_POLL		= 13,
    DTN_CANCEL_POLL		= 14,
    DTN_CANCEL          	= 15,
    DTN_SESSION_UPDATE         	= 16,
    DTN_FIND_REGISTRATION_WTOKEN= 17,
    DTN_PEEK                    = 18,
    DTN_RECV_RAW		= 19,

#ifdef DTPC_ENABLED
    DTPC_REGISTER		= 20,
    DTPC_UNREGISTER		= 21,
    DTPC_SEND                   = 22,
    DTPC_RECV                   = 23,
    DTPC_ELISION_RESPONSE       = 24,
#endif

} dtnapi_message_type_t;

/**
 * Type code to string conversion routine.
 */
const char* dtnipc_msgtoa(u_int8_t type);

/*
 * Initialize the handle structure and a new ipc session with the
 * daemon.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_open(dtnipc_handle_t* handle);

/*
 * Initialize the handle structure and a new ipc session with the
 * daemon.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_open_with_IP(char *daemon_api_IP,short daemon_api_port,dtnipc_handle_t* handle);

/*
 * Clean up the handle. dtnipc_open must have already been called on
 * the handle.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_close(dtnipc_handle_t* handle);
    
/*
 * Send a message over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_send(dtnipc_handle_t* handle, dtnapi_message_type_t type);

/*
 * Receive a message response on the ipc channel. May block if there
 * is no pending message.
 *
 * Sets status to the server-returned status code and returns the
 * length of any reply message on success, returns -1 on internal
 * error.
 */
int dtnipc_recv(dtnipc_handle_t* handle, int* status);

/*
 * Receive a message response on the ipc channel. May block if there
 * is no pending message.
 *
 * Sets status to the server-returned status code and returns the
 * length of any reply message on success, returns -1 on internal
 * error.
 */
int dtnipc_recv_raw(dtnipc_handle_t* handle, int* status);

/**
 * Send a message and wait for a response over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_send_recv(dtnipc_handle_t* handle, dtnapi_message_type_t type);

/**
 * Send a message and wait for a response over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_send_recv_raw(dtnipc_handle_t* handle, dtnapi_message_type_t type);

#ifdef  __cplusplus
}
#endif

#endif /* DTN_IPC_H */
