/*
 *    Copyright 2006 Intel Corporation
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
 *    Modifications made to this file by the patch file oasys_mfs-33289-1.patch
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
#  include <oasys-config.h>
#endif

#include "Singleton.h"
#include "debug/Log.h"
#include <string.h>

namespace oasys {

#define MAX_SINGLETONS 64

SingletonBase** SingletonBase::all_singletons_ = 0;
int             SingletonBase::num_singletons_ = 0;
bool            SingletonBase::destroy_singletons_on_exit_ = false;
SingletonBase::Fini SingletonBase::fini_;

//----------------------------------------------------------------------
SingletonBase::SingletonBase()
{
    if (all_singletons_ == 0) {
        all_singletons_ = (SingletonBase**)malloc(MAX_SINGLETONS * sizeof(SingletonBase*));
        memset(all_singletons_, 0, (MAX_SINGLETONS * sizeof(SingletonBase*)));
    }

    if (num_singletons_ < MAX_SINGLETONS)
        all_singletons_[num_singletons_++] = this;
}


//----------------------------------------------------------------------
SingletonBase::~SingletonBase()
{
//    printf("In ~SingletonBase\n");
}

//----------------------------------------------------------------------
SingletonBase::Fini::~Fini()
{
    //printf("In ~Fini\n");

    //dz debug     if (getenv("OASYS_CLEANUP_SINGLETONS"))
    if (destroy_singletons_on_exit_  || getenv("OASYS_CLEANUP_SINGLETONS"))

    {
//    printf("We're supposed to cleanup singletons\n");
        for (int i = SingletonBase::num_singletons_ - 1; i >= 0; --i)
        {
            log_debug_p("/debug",
                        "deleting singleton %d (%p)",
                        i, SingletonBase::all_singletons_[i]);
            
            if(SingletonBase::all_singletons_[i] != NULL) {
                delete SingletonBase::all_singletons_[i];
            }
            SingletonBase::all_singletons_[i] = NULL;
        }
    }

    oasys::Log::shutdown();

    free(all_singletons_);
    all_singletons_ = NULL;
}

} // namespace oasys
