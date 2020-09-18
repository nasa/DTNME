/*
 *    Copyright 2006 Baylor University
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

#ifndef _OASYS_BT_INQUIRY_H_
#define _OASYS_BT_INQUIRY_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include <bluetooth/bluetooth.h> // for bdaddr_t
#include <bluetooth/hci.h>       // for inquiry_info

#include "../debug/Log.h"

#define MAX_BTNAME_SZ 248

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

namespace oasys {

class BluetoothInquiry : public Logger {

public:

#define BT_INQ_NUM_RESP 20
#define BT_INQ_LENGTH 8

    BluetoothInquiry(const char * logbase = "/dtn/btinquiry");
    virtual ~BluetoothInquiry();

    /*
     * Perform inquiry action
     */
    int inquire();

    /*
     * Enumerate over discovered device adapter addresses
     */
    int first(bdaddr_t&);
    int next(bdaddr_t&);

protected:
    void reset();  // performed internally between inquiries

    // state and iterator over info_
    int num_responses_i_, pos_;
    // buffer for query data
    inquiry_info info_[BT_INQ_NUM_RESP];
    // flags to change default inquiry behavior
    long flags_;
};

}
#endif // OASYS_BLUETOOTH_ENABLED
#endif // _OASYS_BT_INQUIRY_H_
