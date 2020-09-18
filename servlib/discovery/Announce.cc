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
#  include <dtn-config.h>
#endif

#include "IPAnnounce.h"
#include "EthAnnounce.h"
#include "BluetoothAnnounce.h"
#include "Announce.h"

namespace dtn {

Announce* 
Announce::create_announce(const std::string& name, ConvergenceLayer* cl,
                          int argc, const char* argv[])
{
    ASSERT(cl!=NULL);
    (void)name;
    (void)cl;
    (void)argc;
    (void)argv;
    Announce* announce = NULL;
    if ((strncmp(cl->name(),"tcp",3) == 0)  ||
        (strncmp(cl->name(),"udp",3) == 0))
    {
        announce = new IPAnnounce();
    }
#ifdef OASYS_BLUETOOTH_ENABLED
    else
    if (strncmp(cl->name(),"bt",2) == 0)
    {
        announce = new BluetoothAnnounce();
    }
#endif
    else if(strncmp(cl->name(), "eth", 3) == 0) {
        announce = new EthAnnounce();
    } else {
        //no announce implemented for CL type
        return NULL;
    }
    
    if (announce->configure(name,cl,argc-1,argv+1))
    {
        return announce;
    }

    delete announce;
    return NULL;
} 

} // dtn
