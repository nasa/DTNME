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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include <cstdlib>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>

#include "Bluetooth.h"
#include "BluetoothInquiry.h"

namespace oasys {

BluetoothInquiry::BluetoothInquiry(const char * logbase)
    : Logger("BluetoothInquiry", logbase)
{
    reset();
}

BluetoothInquiry::~BluetoothInquiry()
{
}

int
BluetoothInquiry::inquire()
{
    // set dev_id to -1 to have BlueZ autodetermine which device
    // set len = 8 for default 10.24s duration
    // set nrsp = BT_INQ_NUM_RESP to match sizeof(info_)
    // Lower Address Part (lap) is ignored (0)
    // info_ is return array of inquiry data
    // flags_ changes the default inquiry behavior
    inquiry_info *ii = &info_[0];
    num_responses_i_ = Bluetooth::hci_inquiry(-1,BT_INQ_LENGTH,BT_INQ_NUM_RESP,
                                              0,&ii,flags_);
    return num_responses_i_;
}

void
BluetoothInquiry::reset()
{
    pos_ = 0;
    num_responses_i_ = -1;
    memset(&info_[0],0,sizeof(inquiry_info)*BT_INQ_NUM_RESP);
    flags_ |= IREQ_CACHE_FLUSH;
}

int
BluetoothInquiry::first(bdaddr_t& addr)
{
    reset();
    return next(addr);
}

int
BluetoothInquiry::next(bdaddr_t& addr)
{
    if((pos_ >= num_responses_i_) || 
       (bacmp(&(info_[pos_].bdaddr),BDADDR_ANY) == 0))
    {
        reset();
        return -1;
    }
    log_debug("BluetoothInquiry::next(%d)",pos_);
    bacpy(&addr,&(info_[pos_].bdaddr));
    pos_++;
    return 0;
}

} // namespace oasys
#endif /* OASYS_BLUETOOTH_ENABLED */
