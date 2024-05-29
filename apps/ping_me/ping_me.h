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

#ifndef _PING_ME_H_
#define _PING_ME_H_

typedef struct {
    char      ping[8]; // ping_me!
    uint32_t seqno;
    uint32_t nonce;
    uint32_t time;
} ping_payload_t;

#define PING_STR "ping_me!"

#define TIMEVAL_DIFF_MSEC(t1, t2) \
    ((unsigned long int)(((t1).tv_sec  - (t2).tv_sec)  * 1000) + \
     (((t1).tv_usec - (t2).tv_usec) / 1000))

#define DTNTIME_OFFSET 946684800

#define DTNTIMEVAL_DIFF_MSEC(t1, t2) \
    ((unsigned long int)(((t1).secs + DTNTIME_OFFSET - (t2).tv_sec)))

#endif /* _PING_ME_H_ */
