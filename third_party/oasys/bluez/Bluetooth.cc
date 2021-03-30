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

#include <cerrno>
#include <cstdlib>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
extern int errno;

#include "Bluetooth.h"
#include "debug/Log.h"

namespace oasys {

int
Bluetooth::hci_devid(const char* hcidev, const char* log)
{
    int dd = ::hci_devid(hcidev);
    if(log)
    {
        logf(log, LOG_DEBUG, "hci_devid %s: dd %d", hcidev, dd);
    }
    return dd;
}

int
Bluetooth::hci_inquiry(int dev_id, int len, int nrsp, const uint8_t *lap, 
                       inquiry_info **ii, long flags, const char* log)
{
    int err = ::hci_inquiry(dev_id,len,nrsp,lap,ii,flags);
    if(log)
    {
        logf(log, LOG_DEBUG, 
             "hci_inquiry(hci%d): len %d, nrsp %d, lap %p, info %p, flags 0x%lx",
             dev_id,len,nrsp,lap,ii,flags);
    }
    return err;
}

int
Bluetooth::hci_open_dev(int dev_id, const char* log)
{
    int fd = ::hci_open_dev(dev_id);
    if(log)
    {
        logf(log, LOG_DEBUG, "hci_open_dev(hci%d): fd %d",dev_id,fd);
    }
    return fd;
}

int
Bluetooth::hci_close_dev(int dd, const char* log)
{
    int err = ::hci_close_dev(dd);
    if(log)
    {
        logf(log, LOG_DEBUG, "hci_close_dev(%d): err %d",dd,err);
    }
    return err;
}

int
Bluetooth::hci_read_remote_name(int dd, const bdaddr_t *bdaddr, int len, 
                                char *name, int to, const char* log)
{
    int err = ::hci_read_remote_name(dd,bdaddr,len,name,to);
    if(log)
    { 
        bdaddr_t ba;
        baswap(&ba,bdaddr);
        logf(log, LOG_DEBUG, 
             "hci_read_remote_name(%d): [%s] %s len %d to %d",
             dd,bd2str(ba),name,len,to);
    }
    return err;
}

void
Bluetooth::hci_get_bdaddr(bdaddr_t* bdaddr,
                          const char * log)
{
    struct hci_dev_info di;
    memset(&di,0,sizeof(struct hci_dev_info));

    // open socket to HCI control interface
    int fd=socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (fd<0)
    {
        if (log) logf(log, LOG_ERR, "can't open HCI socket");
        return;
    }
    int dev_id = hci_get_route(NULL);
    if (dev_id < 0)
    {
        if (log)
        {
            logf(log, LOG_DEBUG,
                 "bad device id");
            return;
        }
    }
    di.dev_id = dev_id;
    if (ioctl(fd, HCIGETDEVINFO, (void *) &di) < 0)
    {
        if(log) logf(log, LOG_ERR, "can't get device info");
        return;
    }
    bacpy(bdaddr,&di.bdaddr);
    close(fd);
}

int
Bluetooth::hci_dev_up(int dd, const char * hcidev, const char *log)
{
    int dev_id=-1;
    if (strncmp(hcidev,"hci",3) == 0 && strlen(hcidev) >= 4)
    {
        dev_id = atoi(hcidev+3);
    }
    if (dev_id<0)
    {
        if (log) logf(log, LOG_ERR, "badly formatted HCI device name: %s",hcidev);
        return -1;
    }
    if (ioctl(dd, HCIDEVUP, dev_id) < 0)
    {
        if (log) logf(log, LOG_ERR, "failed to init device hci%d: %s (%d)",
                      dev_id, strerror(errno), errno);
        return -1;
    }
    return 0;
}

/* Modified from BlueZ's bluetooth.c */
char *
Bluetooth::_batostr(const bdaddr_t* ba, char *str, size_t str_size)
{
    if (!str)
        return NULL;

    memset(str,0,str_size);

    snprintf(str, str_size, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
             ba->b[5], ba->b[4], ba->b[3], 
             ba->b[2], ba->b[1], ba->b[0]);

    return str;
}

/* Modified from BlueZ's bluetooth.c */
bdaddr_t *
Bluetooth::strtoba(const char *str, bdaddr_t *addr)
{
    const char *ptr = str;
    int i;

    if (!addr)
        return NULL;
    bdaddr_t bd;
    uint8_t *ba = (uint8_t*) &bd;

    for(i = 0; i < 6; i++) {
        ba[i] = (uint8_t) strtol(ptr, NULL, 16);
        if (i != 5 && !(ptr = strchr(ptr,':')))
            ptr = ":00:00:00:00:00";
        ptr++;
    }

    baswap(addr,(bdaddr_t *)ba);
    return addr;
}

void
Bluetooth::baswap(bdaddr_t *dst, const bdaddr_t *src)
{
    unsigned char *d = (unsigned char *) dst;
    const unsigned char *s = (const unsigned char *) src;
    int i;
    for (i = 0; i < 6; i++)
        d[i] = s[5-i];
}


} // namespace oasys
#endif /* OASYS_BLUETOOTH_ENABLED */
