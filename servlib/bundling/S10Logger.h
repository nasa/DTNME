/*
 *    Copyright 2010 Trinity College Dublin
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
 * The N4C summer 2010 logger code.
 * See README.S10 for details, but basically this logs events in a 
 * consistent format, suitable for importing to CSV files for later
 * analysis of logs.
 */

#ifndef _S10LOGGER_H_
#define _S10LOGGER_H_

#ifdef S10_ENABLED

#include "Bundle.h"

// S10 logging events
#define S10_FROMAPP			1		// done
#define S10_FROMDB			2		// done
#define S10_DELIVERED		3		// done
#define S10_TAKECUST		4		// done
#define S10_RELCUST			5		// done
#define S10_EXPIRY			6
#define S10_TX				7		// done
#define S10_RX				8		// done
#define S10_SPARE			9		
#define S10_TXADMIN			10		// done (partly)
#define S10_RXADMIN			11		// done (partly)
#define S10_DUP				12
#define S10_CONTUP			13
#define S10_CONTDOWN		14
#define S10_STARTING		15		// done
#define S10_EXITING			16		// done
#define S10_OHCRAP			17		// never to be done
#define S10_FROMCACHE       18      // added for BPQ 2011

#define S10_MAXEVENT        S10_OHCRAP

// different contact types - discovered or static for now
#define S10_CONTTYPE_ALWAYSON 		1
#define S10_CONTTYPE_ONDEMAND 		2
#define S10_CONTTYPE_SCHEDULED		3
#define S10_CONTTYPE_OPPORTUNISTIC	4

// distinguish differnt admin bundles
#define S10_BFLAG_TXCACK	1
#define S10_BFLAG_RXCACK	2
#define S10_BFLAG_PINGACK	3
#define S10_BFLAG_TRACEACK	4

// A daemon event (start,stop)
int s10_daemon(int event);

// A bundle event (loads, see above) with
// optional peer, and other time info
// zero/null if irrelevant)
int s10_bundle(
		int event, 
		dtn::Bundle* bp,
		const char *peer,
		time_t	ot,
		int 	ousec,
		dtn::Bundle* otherbundle,
		const char *cmt);

int s10_contact(
		int event,
		dtn::Contact *ct,
		const char *cmt);

int s10_setlocal(const char *eid);

#endif // S10_ENABLED

#endif

