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


#ifndef _OASYS_BT_H_
#define _OASYS_BT_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include <fcntl.h>
#include <cstdlib>
#include <sys/uio.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

namespace oasys {

struct Bluetooth {

#ifndef HCIDEVNAMSIZ
#define HCIDEVNAMSIZ 32
#endif

    //@{
    /// System call wrappers (for logging)
    static int hci_devid(const char* hcidev, 
                         const char* log = NULL );
    
    static int hci_inquiry(int dev_id, int len, int nrsp, 
                           const uint8_t *lap, inquiry_info **ii, 
                           long flags, const char* log = NULL );

    static int hci_open_dev(int dev_id,
                            const char* log = NULL );

    static int hci_close_dev(int dd,
                             const char* log = NULL );

    static int hci_read_remote_name(int dd, const bdaddr_t *bdaddr, 
                                    int len, char *name, int to,
                                    const char* log = NULL );

    static void hci_get_bdaddr(bdaddr_t *bdaddr,
                               const char *log = NULL);

    static int hci_dev_up(int dd, const char *hcidev,
                          const char *log = NULL);
    //@}
    
    static char * _batostr(const bdaddr_t *ba, char * str, size_t strsize = 18);

    static bdaddr_t * strtoba(const char *str, bdaddr_t *addr);

    static void baswap(bdaddr_t *dst, const bdaddr_t *src);
    
}; // struct Bluetooth

class Batostr {
public:
    Batostr(bdaddr_t addr) { str_ = Bluetooth::_batostr(&addr,buf_,bufsize_); }
    ~Batostr() { buf_[0] = '\0'; }
    const char * buf() { return str_; }
    static const int bufsize_ = sizeof(":00:00:00:00:00:00");
protected:
    char buf_[bufsize_];
    const char* str_;
};

#define bd2str(addr) oasys::Batostr(addr).buf()

} // namespace oasys

#endif /* OASYS_BLUETOOTH_ENABLED */
#endif /* _OASYS_BT_H_ */
