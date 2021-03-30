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

#include "Bluetooth.h"
#include "RFCOMMClient.h"
#include <cerrno>

extern int errno;

namespace oasys {

int
RFCOMMClient::rc_connect(bdaddr_t remote_addr)
{
    int res = -1;

    set_remote_addr(remote_addr);

    for (channel_ = 1; channel_ <= 30; channel_++) {

        if ((res = bind()) != 0) { 

            close();

            if (errno != EADDRINUSE) {
                // something is borked
                if ( params_.silent_connect_ == false )
                    log_err("error binding to %s:%d: %s",
                            bd2str(local_addr_), channel_, strerror(errno));

                // unrecoverable
                if (errno == EBADFD) {
                    //return -1;
                }

                break;
            }

            if ( params_.silent_connect_ == false )
                log_debug("can't bind to %s:%d: %s",
                          bd2str(local_addr_), channel_, strerror(errno));

        } else {

            // local bind succeeded, now try remote connect

            if ((res = connect()) == 0) {

                // success!
                if ( params_.silent_connect_ == false )
                    log_debug("connected to %s:%d",
                              bd2str(remote_addr_), channel_);

                return res;

            } else {

                close();

                // failed to connect; report it and move on
                if ( params_.silent_connect_ == false )
                    log_debug("can't connect to %s:%d: %s",
                              bd2str(remote_addr_), channel_, strerror(errno)); 

                // unrecoverable
                if (errno == EBADFD) {
                    //return -1;
                }

            }
        }
    }

    log_err("Scanned all RFCOMM channels but unable to connect to %s",
            bd2str(remote_addr_));
    return -1;
}

int
RFCOMMClient::rc_connect()
{
    ASSERT(bacmp(&remote_addr_,BDADDR_ANY)!=0);
    return rc_connect(remote_addr_);
}


}  // namespace oasys

#endif /* OASYS_BLUETOOTH_ENABLED */
