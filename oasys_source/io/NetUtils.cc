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
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "NetUtils.h"
#include "compat/inet_aton.h"
#include "debug/DebugUtils.h"
#include "debug/Log.h"
#include "thread/SpinLock.h"
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>

#ifndef INADDR_NONE
#  define INADDR_NONE ((unsigned long int) -1)
#endif

namespace oasys {

/*
 * A faster replacement for inet_ntoa().
 * (Copied from the tcpdump source ).
 *
 * Modified to take the buffer as an argument.
 * Returns a pointer within buf where the string starts
 *
 */
const char *
_intoa(u_int32_t addr, char* buf, size_t bufsize)
{
	register char *cp;
	register u_int byte;
	register int n;
        
 	addr = ntohl(addr);
	cp = &buf[bufsize];
	*--cp = '\0';

	n = 4;
	do {
		byte = addr & 0xff;
		*--cp = byte % 10 + '0';
		byte /= 10;
		if (byte > 0) {
			*--cp = byte % 10 + '0';
			byte /= 10;
			if (byte > 0)
				*--cp = byte + '0';
		}
		*--cp = '.';
		addr >>= 8;
	} while (--n > 0);

	return cp + 1;
}

int
gethostbyname(const char* name, in_addr_t* addr)
{
    ASSERT(addr);

    // name is a numerical address
    if (inet_aton(name, (struct in_addr*)addr) != 0) {
        return 0;
    }

#if defined(HAVE_GETHOSTBYNAME_R)
    
    struct hostent h;
    char buf[2048];
    struct hostent* ret = 0;
    (void)ret;
    int h_err;

    
#if defined(__sun__) || defined(__QNXNTO__) // Solaris and QNX v6.x have different arguments
    if (::gethostbyname_r(name, &h, buf, sizeof(buf), &h_err) < 0) {
        logf("/oasys/net", LOG_ERR, "error return from gethostbyname_r(%s): %s",
             name, strerror(h_err));
        return -1;
    }
#else
    if (::gethostbyname_r(name, &h, buf, sizeof(buf), &ret, &h_err) < 0) {
        logf("/oasys/net", LOG_ERR, "error return from gethostbyname_r(%s): %s",
             name, strerror(h_err));
        return -1;
    }
    if (ret == NULL) {
        return -1;
    }
#endif

    *addr = ((struct in_addr**)h.h_addr_list)[0]->s_addr;

    if (*addr == INADDR_NONE) {
        logf("/oasys/net", LOG_ERR, "gethostbyname_r(%s) returned INADDR_NONE", name);
        return -1;
    }
    return 0;

#elif defined(HAVE_GETADDRINFO)

    struct addrinfo hints;
    struct addrinfo *res;
    int              err;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    
    err = getaddrinfo(name, 0, &hints, &res);
    if(err != 0) 
        return -1;
    
    ASSERT(res != 0);
    ASSERT(res->ai_family == PF_INET);
    *addr = ((struct sockaddr_in*) res->ai_addr)->sin_addr.s_addr;
    
    freeaddrinfo(res);

    if (*addr == INADDR_NONE) {
        logf("/oasys/net", LOG_ERR, "getaddrinfo(%s) returned INADDR_NONE", name);
        return -1;
    }
    return 0;
    
#elif defined(HAVE_GETHOSTBYNAME)
    // make it thread-safe by using a global lock
    static SpinLock gethostbyname_lock;
    ScopeLock l(&gethostbyname_lock, "gethostbyname");
    
    struct hostent *hent;
    hent = ::gethostbyname(name);
    if (hent == NULL) {
        logf("/net", LOG_ERR, "error return from gethostbyname(%s): %s",
             name, strerror(h_errno));
        return -1;
    }

    *addr = ((struct in_addr**)hent->h_addr_list)[0]->s_addr;
    
    if (*addr == INADDR_NONE) {
        logf("/oasys/net", LOG_ERR, "gethostbyname(%s) returned INADDR_NONE", name);
        return -1;
    }

    return 0;
    
#else
#error No gethostbyname equivalent available for this platform
#endif    
}

} // namespace oasys
