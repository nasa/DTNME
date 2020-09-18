/*
 *    Copyright 2015 United States Government as represented by NASA
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

#ifndef _DTNECHO_H_
#define _DTNECHO_H_

typedef struct {
    char      ping[8]; // dtn_ping!
    u_int32_t seqno;
    u_int32_t nonce;
    u_int32_t time;
} echo_payload_t;

#define ECHO_STR "dtnecho!"

#define TIMEVAL_DIFF_MSEC(t1, t2) \
    ((unsigned long int)(((t1).tv_sec  - (t2).tv_sec)  * 1000) + \
     (((t1).tv_usec - (t2).tv_usec) / 1000))

#define DTNTIME_OFFSET 946684800

#define DTNTIMEVAL_DIFF_MSEC(t1, t2) \
    ((unsigned long int)(((t1).secs + DTNTIME_OFFSET - (t2).tv_sec)))

#endif /* _DTNECHO_H_ */
