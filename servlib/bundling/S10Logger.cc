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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef S10_ENABLED


#include <string.h>

#include <oasys/debug/DebugUtils.h>
#include <oasys/thread/SpinLock.h>

#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleList.h"
#include "ExpirationTimer.h"

#include "storage/GlobalStore.h"

#include "S10Logger.h"

static const char *flagstr[]= {
		"",
		"BFLAG_TXCACK",
		"BFLAG_RXCACK",
		"BFLAG_PINGACK",
		"BFLAG_TRACEACK"
};

// S10 event strings
static const char *eventstr[] = {
				"",
				"FROMAPP",
				"FROMDB",
				"DELIVERED",
				"TAKECUST",
				"RELCUST",
				"EXPIRY",
				"TX",
				"RX",
				"SPARE",
				"TXADMIN",
				"RXADMIN",
				"DUP",
				"CONTUP",
				"CONTDOWN",
				"STARTING",
				"EXITING",
				"OHCRAPBADEVENT"};

static const char *contstr[] ={
	"",
	"ALWAYSON",
	"ONDEMAND",
	"SCHEDULED",
	"OPPORTUNISTIC"
};

// Field width for log info
#define LOGFW 256

// ok, I basically write C code, so sue me:-)
// This is the generic CSV structure
typedef struct logrep_struct {
	char	rfc3339date[LOGFW];
	time_t	eventTime;   // maybe not needed?
	int		eventUsec;
	char	loggingNode[LOGFW];
	int		event;
	size_t	size;
	char	srcEID[LOGFW];
	char 	bundleID[LOGFW];
	char	destEID[LOGFW];
	char	expiry[LOGFW];
	int		bFlags; // dunno yat
	char	peer[LOGFW];
	char	otherBundleSrc[LOGFW];
	char	otherBundleID[LOGFW];
	time_t	otherTime;
	int		otherUsec;
	int		contType;
	int		duration;
	const	char *cmt;
} logrep_t;

int logLr(logrep_t	*lrp){

	static bool inited=false;
	if (!inited) {
		log_info_p("S10","rfc3339,time_t.usec,node,event,size,src,id,dest,expiry,flags,peer,ob-src,ob-id,otime.ousec,conttype,duration,cmt");
		inited=true;
	}
	log_info_p("S10","%s,%lu.%06d,%s,%s,%zu,%s,%s,%s,%s,%s,%s,%s,%s,%lu.%06d,%s,%d,%s\n",
		lrp->rfc3339date, lrp->eventTime, lrp->eventUsec,
		lrp->loggingNode, 
		eventstr[lrp->event], // need to protect this index
		lrp->size,lrp->srcEID,lrp->bundleID,lrp->destEID,
		lrp->expiry,flagstr[lrp->bFlags],
		lrp->peer,lrp->otherBundleSrc,lrp->otherBundleID,
		lrp->otherTime,lrp->otherUsec,contstr[lrp->contType], lrp->duration,
		lrp->cmt?lrp->cmt:"");
	return(0);
}

static bool localset=false;
static char localeid[LOGFW];

int s10_setlocal(const char *eid)
{
	if (localset) return(1);
	snprintf(localeid,LOGFW,"%s",eid);
	localset=true;
	return(0);
}

int basicLr(logrep_t	*lrp)
{
	memset(lrp,0,sizeof(logrep_t));
    struct timeval tv;
    gettimeofday(&tv, 0);
	lrp->eventTime=tv.tv_sec;
    lrp->eventUsec=tv.tv_usec;
	const char *fmtstr="%Y-%m-%d %H:%M:%S";
	struct tm *tmval;
	tmval=gmtime(&lrp->eventTime);
	strftime(lrp->rfc3339date,1024,fmtstr,tmval);
	if (localset) snprintf(lrp->loggingNode,LOGFW,"%s",localeid);
	else gethostname(lrp->loggingNode,LOGFW);
	return(0);
}

int s10_daemon(int event) {
	if (event==S10_STARTING) {
		log_notice_p("/S10", "Lines marked \"S10\" are log lines added to make parsing");
		log_notice_p("/S10", "the logs easier after extended tests. These were added for");
		log_notice_p("/S10", "the summer 2010 N4C summer trial and are mainly new lines");
		log_notice_p("/S10", "that allow tracing bundles across multiple hops.");
		log_notice_p("/S10", "To extract these do the following...");
		log_notice_p("/S10", "\t grep \"S10 info\" <dtnd-log-files> | grep -v grep | awk '{print $4,$5}' | sort -n");

	}
	logrep_t lr;
	basicLr(&lr);
	if (event >0 && event <= S10_MAXEVENT) lr.event=event;
	else lr.event=event; 
	logLr(&lr);
	return(0);
}

int s10_bundle(
		int event, 
		dtn::Bundle* bp, 
		const char *peer, 
		time_t ot,
		int ousec,
		dtn::Bundle* otherBundle,
		const char *cmt)
{
        (void) ot;
        (void) ousec;

	logrep_t lr;
	basicLr(&lr);
	if (event >0 && event <= S10_MAXEVENT) lr.event=event;
	else lr.event=S10_OHCRAP; 
	if (cmt) lr.cmt=cmt;

	// set src
	snprintf(lr.srcEID,LOGFW,"%s",bp->source().c_str());
	snprintf(lr.bundleID,LOGFW,"%" PRIu64 ".%" PRIu64,bp->creation_ts().seconds_,bp->creation_ts().seqno_);
	snprintf(lr.destEID,LOGFW,"%s",bp->dest().c_str());
	lr.size=bp->payload().length();
	snprintf(lr.expiry,LOGFW,"%" PRIu64,bp->expiration());
	if (peer) snprintf(lr.peer,LOGFW,"%s",peer);
	if (otherBundle) {
		snprintf(lr.otherBundleSrc,LOGFW,"%s",otherBundle->source().c_str());
		snprintf(lr.otherBundleID,LOGFW,"%" PRIu64 ".%" PRIu64,
			otherBundle->creation_ts().seconds_,otherBundle->creation_ts().seqno_);
	}
	logLr(&lr);
	return(0);
}


int s10_contact(
		int event,
		dtn::Contact *ct,
		const char *cmt)
{
	logrep_t lr;
	basicLr(&lr);
	if (event != S10_CONTUP && event != S10_CONTDOWN) lr.event=S10_OHCRAP;
	else lr.event=event; 
	if (cmt) lr.cmt=cmt;
	if (event==S10_CONTDOWN) {
		lr.otherTime=ct->start_time().sec_;
		lr.otherUsec=ct->start_time().usec_;
		lr.duration=lr.eventTime-lr.otherTime;
	}
	snprintf(lr.peer,LOGFW,"%s",ct->link()->nexthop());
	lr.contType=ct->link()->type();
	logLr(&lr);
	return(0);
	return(0);
}

#endif  // S10_ENABLED
