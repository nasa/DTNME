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

#include <algorithm>
#include <stdlib.h>
#include <third_party/oasys/thread/SpinLock.h>

#include "Bundle.h"
#include "CustodyList.h"

namespace dtn {

//----------------------------------------------------------------------
CustodyList::CustodyList()
    : Logger("CustodyList", "/dtn/custodylist")
{
}

//----------------------------------------------------------------------
CustodyList::~CustodyList()
{
    clear();
}

//----------------------------------------------------------------------
void
CustodyList::insert(Bundle* bundle)
{
    SPtr_DestinationList listptr;

    std::string dest = bundle->custody_dest();

    uint64_t custody_id = bundle->custodyid();

    oasys::ScopeLock l(&lock_, __func__);

    Iterator iter = list_.find(dest);

    if (iter == list_.end()) {
        listptr = std::make_shared<DestinationList>();
        listptr->dest_list_ = std::make_shared<BundleListIntMap>("CustodyListForDest", nullptr, 
                                                                 "CustodyList", "/custodylist");

        list_[dest] = listptr;
    } else {
        listptr = iter->second;
    }

    if (custody_id == 0) {
        ++listptr->last_custody_id_;
        bundle->set_custodyid(listptr->last_custody_id_);
    } else if (custody_id > listptr->last_custody_id_) {
        // for reloading from the database
        listptr->last_custody_id_ = custody_id;
    }

    listptr->dest_list_->insert(bundle->custodyid(), bundle);
}

//----------------------------------------------------------------------
bool
CustodyList::erase(Bundle* bundle)
{
    std::string dest = bundle->custody_dest();
    uint64_t custody_id = bundle->custodyid();

    return erase(dest, custody_id);
}

//----------------------------------------------------------------------
bool
CustodyList::erase(std::string& dest, bundleid_t custody_id)
{
    SPtr_DestinationList listptr;

    oasys::ScopeLock l(&lock_, __func__);

    Iterator iter = list_.find(dest);

    if (iter != list_.end()) {
        listptr = iter->second;
        return listptr->dest_list_->erase(custody_id);
    }

    return false;
}

//----------------------------------------------------------------------
BundleRef
CustodyList::find(std::string& dest, bundleid_t custody_id)
{
    BundleRef bref("CustodyList::find");

    SPtr_DestinationList listptr;

    oasys::ScopeLock l(&lock_, __func__);

    Iterator iter = list_.find(dest);

    if (iter != list_.end()) {
        listptr = iter->second;
        bref = listptr->dest_list_->find(custody_id);
    }

    return bref;
}

//----------------------------------------------------------------------
uint64_t
CustodyList::last_custody_id(std::string& dest)
{
    uint64_t last_id = 0;
    SPtr_DestinationList listptr;

    oasys::ScopeLock l(&lock_, __func__);

    Iterator iter = list_.find(dest);

    if (iter != list_.end()) {
        listptr = iter->second;
        last_id = listptr->last_custody_id_;
    }

    return last_id;
}

//----------------------------------------------------------------------
void
CustodyList::clear()
{
    oasys::ScopeLock l(&lock_, __func__);
    
    list_.clear();
}


//----------------------------------------------------------------------
uint64_t
CustodyList::size()
{
    uint64_t count = 0;
    oasys::ScopeLock l(&lock_, __func__);


    Iterator iter = list_.begin();
    while (iter != list_.end()) {
        count += iter->second->dest_list_->size();
        ++iter;
    }

    return count;
}

//----------------------------------------------------------------------
bool
CustodyList::empty()
{
    uint64_t count = size();
    return (count == 0);
}

} // namespace dtn
