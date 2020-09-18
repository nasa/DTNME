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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <oasys/compat/inet_aton.h>
#include <oasys/compat/inttypes.h>

#include "dtn_ipc.h"
#include "dtn_errno.h"
#include "dtn_types.h"

/* exposed globally for testing purposes only */
int dtnipc_version = DTN_IPC_VERSION;

const char*
dtnipc_msgtoa(u_int8_t type)
{
#define CASE(_type) case _type : return #_type; break;
    
    switch(type) {
        CASE(DTN_OPEN);
        CASE(DTN_CLOSE);
        CASE(DTN_LOCAL_EID);
        CASE(DTN_REGISTER);
        CASE(DTN_UNREGISTER);
        CASE(DTN_FIND_REGISTRATION);
        CASE(DTN_CHANGE_REGISTRATION);
        CASE(DTN_BIND);
        CASE(DTN_SEND);
        CASE(DTN_RECV);
        CASE(DTN_BEGIN_POLL);
        CASE(DTN_CANCEL_POLL);
        CASE(DTN_CANCEL);
        CASE(DTN_SESSION_UPDATE);
        CASE(DTN_PEEK);
        CASE(DTN_RECV_RAW);

#ifdef DTPC_ENABLED
        CASE(DTPC_REGISTER);
        CASE(DTPC_UNREGISTER);
        CASE(DTPC_SEND);
        CASE(DTPC_RECV);
        CASE(DTPC_ELISION_RESPONSE);
#endif

    default:
        return "(unknown type)";
    }
    
#undef CASE
}

/*
 * Initialize the handle structure.
 */
int
dtnipc_open(dtnipc_handle_t* handle)
{
    int remote_version, ret;
    char *env, *end;
    struct sockaddr_in sa;
    in_addr_t ipc_addr;
    u_int16_t ipc_port;
    u_int32_t handshake;
    u_int port;

    // zero out the handle
    memset(handle, 0, sizeof(dtnipc_handle_t));

    // check for debugging
    if (getenv("DTNAPI_DEBUG") != 0) {
        handle->debug = 1;
    }
    
    // note that we leave eight bytes free to be used for the framing
    // -- the type code and length for send (which is only five
    // bytes), and the return code and length for recv (which is
    // actually eight bytes)
    xdrmem_create(&handle->xdr_encode, handle->buf + 8,
                  DTN_MAX_API_MSG - 8, XDR_ENCODE);
    
    xdrmem_create(&handle->xdr_decode, handle->buf + 8,
                  DTN_MAX_API_MSG - 8, XDR_DECODE);

    // open the socket
    handle->sock = socket(PF_INET, SOCK_STREAM, 0);
    if (handle->sock < 0)
    {
        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
    }

    // check for DTNAPI environment variables overriding the address /
    // port defaults
    ipc_addr = htonl(INADDR_LOOPBACK);
    ipc_port = DTN_IPC_PORT;
    
    if ((env = getenv("DTNAPI_ADDR")) != NULL) {
        if (inet_aton(env, (struct in_addr*)&ipc_addr) == 0)
        {
            fprintf(stderr, "DTNAPI_ADDR environment variable (%s) "
                    "not a valid ip address\n", env);
            exit(1);
        }
    }

    if ((env = getenv("DTNAPI_PORT")) != NULL) {
        port = strtoul(env, &end, 10);
        if (*end != '\0' || port > 0xffff)
        {
            fprintf(stderr, "DTNAPI_PORT environment variable (%s) "
                    "not a valid ip port\n", env);
            exit(1);
        }
        ipc_port = (u_int16_t)port;
    }

    // connect to the server
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = ipc_addr;
    sa.sin_port = htons(ipc_port);
    
    ret = connect(handle->sock, (const struct sockaddr*)&sa, sizeof(sa));
    if (ret != 0) {
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: error connecting to server: %s\n",
                    strerror(errno));
        }

        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
    }

    if (handle->debug) {
        fprintf(stderr, "dtn_ipc: connected to server: fd %d\n", handle->sock);
    }

    // send the session initiation to the server on the handshake
    // port. it consists of DTN_OPEN in the high 16 bits and IPC
    // version in the low 16 bits
    handshake = htonl(DTN_OPEN << 16 | dtnipc_version);
    ret = write(handle->sock, &handshake, sizeof(handshake));
    if (ret != sizeof(handshake)) {
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: handshake error\n");
        }
        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
    }
    handle->total_sent += ret;

    // wait for the handshake response
    handshake = 0;
    ret = read(handle->sock, &handshake, sizeof(handshake));
    if (ret != sizeof(handshake) || (ntohl(handshake) >> 16) != DTN_OPEN) {
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: handshake error\n");
        }
        dtnipc_close(handle);
        handle->err = DTN_ECOMM;
        return -1;
    }

    handle->total_rcvd += ret;

    remote_version = (ntohl(handshake) & 0x0ffff);
    if (remote_version != dtnipc_version) {
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: version mismatch\n");
        }
        dtnipc_close(handle);
        handle->err = DTN_EVERSION;
        return -1;
    }
    
    return 0;
}

/*
 * Initialize the handle structure.
 */
int
dtnipc_open_with_IP(char *daemon_api_IP,short daemon_api_port,dtnipc_handle_t* handle)
{
    int remote_version, ret;
    //char *env;
    struct sockaddr_in sa;
    in_addr_t ipc_addr;
    u_int16_t ipc_port;
    u_int32_t handshake;
    //u_int port;

    // zero out the handle
    memset(handle, 0, sizeof(dtnipc_handle_t));

    // check for debugging
    if (getenv("DTNAPI_DEBUG") != 0) {
        handle->debug = 1;
    }
    
    // note that we leave eight bytes free to be used for the framing
    // -- the type code and length for send (which is only five
    // bytes), and the return code and length for recv (which is
    // actually eight bytes)
    xdrmem_create(&handle->xdr_encode, handle->buf + 8,
                  DTN_MAX_API_MSG - 8, XDR_ENCODE);
    
    xdrmem_create(&handle->xdr_decode, handle->buf + 8,
                  DTN_MAX_API_MSG - 8, XDR_DECODE);

    // open the socket
    handle->sock = socket(PF_INET, SOCK_STREAM, 0);
    if (handle->sock < 0)
    {
        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
    }

    // check for DTNAPI environment variables overriding the address /
    // port defaults
    ipc_addr = htonl(INADDR_LOOPBACK);
    ipc_port = DTN_IPC_PORT;
    
    if (inet_aton(daemon_api_IP, (struct in_addr*)&ipc_addr) == 0)
        {
            fprintf(stderr, "DTNAPI_ADDR environment variable (%s) "
                    "not a valid ip address\n", daemon_api_IP);
            exit(1);
        }

    // short is only 16 bits 
    //if (daemon_api_port > 0xffff)
    //    {
    //        fprintf(stderr, "DTNAPI_PORT environment variable (%d) "
    //                "not a valid ip port\n", daemon_api_port);
    //        exit(1);
    //    }
    
    ipc_port = (u_int16_t)daemon_api_port;
    

    // connect to the server
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = ipc_addr;
    sa.sin_port = htons(ipc_port);
    
    ret = connect(handle->sock, (const struct sockaddr*)&sa, sizeof(sa));
    if (ret != 0) {
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: error connecting to server: %s\n",
                    strerror(errno));
        }

        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
    }

    if (handle->debug) {
        fprintf(stderr, "dtn_ipc: connected to server: fd %d\n", handle->sock);
    }

    // send the session initiation to the server on the handshake
    // port. it consists of DTN_OPEN in the high 16 bits and IPC
    // version in the low 16 bits
    handshake = htonl(DTN_OPEN << 16 | dtnipc_version);
    ret = write(handle->sock, &handshake, sizeof(handshake));
    if (ret != sizeof(handshake)) {
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: handshake error\n");
        }
        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
    }
    handle->total_sent += ret;

    // wait for the handshake response
    handshake = 0;
    ret = read(handle->sock, &handshake, sizeof(handshake));
    if (ret != sizeof(handshake) || (ntohl(handshake) >> 16) != DTN_OPEN) {
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: handshake error\n");
        }
        dtnipc_close(handle);
        handle->err = DTN_ECOMM;
        return -1;
    }

    handle->total_rcvd += ret;

    remote_version = (ntohl(handshake) & 0x0ffff);
    if (remote_version != dtnipc_version) {
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: version mismatch\n");
        }
        dtnipc_close(handle);
        handle->err = DTN_EVERSION;
        return -1;
    }
    
    return 0;
}











/*
 * Clean up the handle. dtnipc_open must have already been called on
 * the handle.
 */
int
dtnipc_close(dtnipc_handle_t* handle)
{
    int ret;
    
    // first send a close over RPC
    if (handle->err != DTN_ECOMM) {
        ret = dtnipc_send_recv(handle, DTN_CLOSE);
    } else {
        ret = -1;
    }
    
    xdr_destroy(&handle->xdr_encode);
    xdr_destroy(&handle->xdr_decode);

    if (handle->sock > 0) {
        close(handle->sock);
    }

    handle->sock = 0;

    return ret;
}
      

/*
 * Send a message over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int
dtnipc_send(dtnipc_handle_t* handle, dtnapi_message_type_t type)
{
    int ret;
    u_int32_t len, msglen, origlen;
    
    // pack the message code in the fourth byte of the buffer and the
    // message length into the next four. we don't use xdr routines
    // for these since we need to be able to decode the length on the
    // other side to make sure we've read the whole message, and we
    // need the type to know which xdr decoder to call
    handle->buf[3] = type;

    len = xdr_getpos(&handle->xdr_encode);
    msglen = len + 5;
    len = htonl(len);
    memcpy(&handle->buf[4], &len, sizeof(len));
    
    // reset the xdr encoder
    xdr_setpos(&handle->xdr_encode, 0);
    
    // send the message, looping until it's all sent
    origlen = msglen;
    char* bp = &handle->buf[3];
    do {
        ret = write(handle->sock, bp, msglen);
        handle->total_sent += ret;
        
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: send(%s) wrote %d/%d bytes (%s) "
                    "(total sent/rcvd %u/%u)\n",
                    dtnipc_msgtoa(type), ret, origlen,
                    ret == -1 ? strerror(errno) : "success",
                    handle->total_sent, handle->total_rcvd);
        }

        if (ret <= 0) {
            if (errno == EINTR)
                continue;
            
            handle->err = DTN_ECOMM;
            dtnipc_close(handle);
            return -1;
        }

        bp     += ret;
        msglen -= ret;
        
    } while (msglen > 0);

    return 0;
}

/*
 * Receive a message on the ipc channel. May block if there is no
 * pending message.
 *
 * Sets status to the server-returned status code and returns the
 * length of any reply message on success, returns -1 on internal
 * error.
 */
int
dtnipc_recv(dtnipc_handle_t* handle, int* status)
{
    int ret;
    u_int32_t len, nread;
    u_int32_t statuscode;

    // reset the xdr decoder before reading in any data
    xdr_setpos(&handle->xdr_decode, 0);

    // read the status code and length
    // XXX/dz wrapped the read() so it would be interrupt tolerant
    nread = 0;
    while (nread < 8) {
      errno = 0;
      ret = read(handle->sock,
                 &handle->buf[nread], 8 - nread);
      handle->total_rcvd += ret;
      if (handle->debug) {
        fprintf(stderr, "dtn_ipc: recv() read len %d/%d bytes (%s)\n",
                ret, nread, ret == -1 ? strerror(errno) : "success");
      }

      if (ret <= 0) {
        // XXX/dz scenario causes infinite loop:
        // if the dtnd daemon crashes while it is in the
        // middle of a read, ret is returned as zero and errno
        // remains EINTR. added setting errno to zero above
        // and checking that ret is less than zero before continuing.
        if (ret < 0 && errno == EINTR)
          continue;
            
        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
      }
      nread += ret;
    }
    ret = nread;

    // make sure we got it all
    if (ret != 8) {
        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
    }
    
    memcpy(&statuscode, handle->buf, sizeof(statuscode));
    statuscode = ntohl(statuscode);
    *status = statuscode;
    
    memcpy(&len, &handle->buf[4], sizeof(len));
    len = ntohl(len);
    
    if (handle->debug) {
        fprintf(stderr, "dtn_ipc: recv() read %d/8 bytes for status (%s): "
                "status %d len %d (total sent/rcvd %u/%u)\n",
                ret, ret == -1 ? strerror(errno) : "success",
                *status, len, handle->total_sent, handle->total_rcvd);
    }

    // read the rest of the message 
    while (nread < len + 8) {
        errno = 0;
        ret = read(handle->sock,
                   &handle->buf[nread], sizeof(handle->buf) - nread);
        handle->total_rcvd += ret;
        
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: recv() read %d/%d bytes (%s)\n",
                    ret, len, ret == -1 ? strerror(errno) : "success");
        }

        if (ret <= 0) {
            if (ret < 0 && errno == EINTR)
                continue;
            
            handle->err = DTN_ECOMM;
            dtnipc_close(handle);
            return -1;
        }

        nread += ret;
    }

    return len;
}


/*
 * Receive a message on the ipc channel. May block if there is no
 * pending message.
 *
 * Sets status to the server-returned status code and returns the
 * length of any reply message on success, returns -1 on internal
 * error.
 */
int
dtnipc_recv_raw(dtnipc_handle_t* handle, int* status)
{
    int ret;
    u_int32_t len, nread;
    u_int32_t statuscode;

    // reset the xdr decoder before reading in any data
    xdr_setpos(&handle->xdr_decode, 0);

    // read the status code and length
    // dz 2011.10.17 - carry forward patch to 2.7
    // dz 2011.07.28 - wrapped the read() so it would be interrupt tolerant
    nread = 0;
    while (nread < 8) {
      errno = 0;
      ret = read(handle->sock,
                 &handle->buf[nread], 8 - nread);
      handle->total_rcvd += ret;
      if (handle->debug) {
        fprintf(stderr, "dtn_ipc: recv_raw() read len %d/%d bytes (%s)\n",
                ret, nread, ret == -1 ? strerror(errno) : "success");
      }

      if (ret <= 0) {
        // dz 2012.02.14  scenario causes infinite loop:
        // if the dtnd daemon crashes while it is in the
        // middle of a read, ret is returned as zero and errno
        // remains EINTR. added setting errno to zero above
        // and checking that ret is less than zero before continuing.
        if (ret < 0 && errno == EINTR)
          continue;
            
        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
      }
      nread += ret;
    }
    ret = nread;

    // make sure we got it all
    if (ret != 8) {
        handle->err = DTN_ECOMM;
        dtnipc_close(handle);
        return -1;
    }
    
    memcpy(&statuscode, handle->buf, sizeof(statuscode));
    statuscode = ntohl(statuscode);
    *status = statuscode;
    
    memcpy(&len, &handle->buf[4], sizeof(len));
    len = ntohl(len);
    
    if (handle->debug) {
        fprintf(stderr, "dtn_ipc: recv_raw() read %d/8 bytes for status (%s): "
                "status %d len %d (total sent/rcvd %u/%u)\n",
                ret, ret == -1 ? strerror(errno) : "success",
                *status, len, handle->total_sent, handle->total_rcvd);
    }

    // read the rest of the message 
    // dz 2011.07.28 - wrapped the read() so it would be interrupt tolerant
    nread = 8;
    while (nread < len + 8) {
        errno = 0;
        ret = read(handle->sock,
                   &handle->buf[nread], sizeof(handle->buf) - nread);
        handle->total_rcvd += ret;
        
        if (handle->debug) {
            fprintf(stderr, "dtn_ipc: recv_raw() read %d/%d bytes (%s)\n",
                    ret, len, ret == -1 ? strerror(errno) : "success");
        }

        if (ret <= 0) {
            // dz 2012.02.14  scenario causes infinite loop:
            // if the dtnd daemon crashes while it is in the
            // middle of a read, ret is returned as zero and errno
            // remains EINTR. added setting errno to zero above
            // and checking that ret is less than zero before continuing.
            if (ret < 0 && errno == EINTR)
                continue;
            
            handle->err = DTN_ECOMM;
            dtnipc_close(handle);
            return -1;
        }

        nread += ret;
    }

    return len;
}


/**
 * Send a message and wait for a response over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_send_recv(dtnipc_handle_t* handle, dtnapi_message_type_t type)
{
    int status;

    // send the message
    if (dtnipc_send(handle, type) < 0) {
        return -1;
    }

    // wait for a response
    if (dtnipc_recv(handle, &status) < 0) {
        return -1;
    }

    // handle server-side errors
    if (status != DTN_SUCCESS) {
        handle->err = status;
        return -1;
    }

    return 0;
}


/**
 * Send a message and wait for a response over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int dtnipc_send_recv_raw(dtnipc_handle_t* handle, dtnapi_message_type_t type)
{
    int status;

    // send the message
    if (dtnipc_send(handle, type) < 0) {
        return -1;
    }

    // wait for a response
    if (dtnipc_recv_raw(handle, &status) < 0) {
        return -1;
    }

    // handle server-side errors
    if (status != DTN_SUCCESS) {
        handle->err = status;
        return -1;
    }

    return 0;
}
