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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

//#include <algorithm>
//#include <stdlib.h>
#include <oasys/thread/SpinLock.h>

#include "BundleListBase.h"

namespace dtn {

//----------------------------------------------------------------------
BundleListBase::BundleListBase(const std::string& name, oasys::SpinLock* lock,
                       const std::string& ltype, const std::string& subpath)
    : Logger(ltype.c_str(), "/dtn/bundle%s%s", subpath.c_str(), name.c_str()),
      name_(name), ltype_(ltype), notifier_(NULL)
{
    if (lock != NULL) {
        lock_     = lock;
        own_lock_ = false;
    } else {
        lock_     = new oasys::SpinLock();
        own_lock_ = true;
    }
}

//----------------------------------------------------------------------
void
BundleListBase::set_name(const std::string& name)
{
    name_ = name;
    logpathf("/dtn/bundle/list/%s", name.c_str());
}

//----------------------------------------------------------------------
BundleListBase::~BundleListBase()
{
    if (own_lock_) {
        delete lock_;
    }
    lock_ = NULL;
}


} // namespace dtn
