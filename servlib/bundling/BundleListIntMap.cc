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
#include <oasys/thread/SpinLock.h>

#include "Bundle.h"
#include "BundleListIntMap.h"
#include "BundleMappings.h"
#include "BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
BundleListIntMap::BundleListIntMap(const std::string& name, oasys::SpinLock* lock,
                         const std::string& ltype, const std::string& subpath)
    : BundleListBase(name, lock, ltype, subpath)
{
}

//----------------------------------------------------------------------
void
BundleListIntMap::set_name(const std::string& name)
{
    name_ = name;
    logpathf("/dtn/bundle/liststrmultmap/%s", name.c_str());
}

//----------------------------------------------------------------------
BundleListIntMap::~BundleListIntMap()
{
    clear();
}

//----------------------------------------------------------------------
BundleRef
BundleListIntMap::front() const
{
    BundleRef ret("BundleListIntMap::front() temporary");

    oasys::ScopeLock l(lock_, "BundleListIntMap::front");
    
    if (list_.empty())
    {
        return ret;
    }

    ret = list_.begin()->second;
    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleListIntMap::back() const
{
    BundleRef ret("BundleListIntMap::back() temporary");

    oasys::ScopeLock l(lock_, "BundleListIntMap::back");

    if (list_.empty())
    {
        return ret;
    }

    ret = (--list_.end())->second;
    return ret;
}

//----------------------------------------------------------------------
void
BundleListIntMap::add_bundle(Bundle* bundle, const bundleid_t key)
{
    ASSERT(lock_->is_locked_by_me());
    ASSERT(bundle->lock()->is_locked_by_me());
    
    if (bundle->is_freed()) {
    	log_info("add_bundle called with pre-freed bundle; ignoring");
    	return;
    }

    if (bundle->is_queued_on((BundleListBase*) this)) {
        log_err("ERROR in add bundle: "
                "bundle id %" PRIbid " already on list [%s]",
                bundle->bundleid(), name_.c_str());
        
        return;
    }

    // insert the bundle and add a mapping
    IPair result = list_.insert(MPair(key, bundle));

    // only add a mapping if it was actually inserted
    if ( result.second == true ) {
        SPV blpos ( new iterator(result.first) );
        SPBMapping bmap ( new BundleMapping(this, blpos) );
        bundle->mappings()->push_back(bmap);
        bundle->add_ref(ltype_.c_str(), name_.c_str());
    } else {
        log_crit("Error in add_bundle in list %p", this);
        ASSERTF(false, "Error in add_bundle in list %p", this);
    }
        
    if (notifier_ != 0) {
        notifier_->notify();
    }

    //log_debug("bundle id %" PRIbid " add mapping [%s] to list %p",
    //              bundle->bundleid(), name_.c_str(), this);
}

//----------------------------------------------------------------------
void
BundleListIntMap::insert(Bundle* bundle)
{
    oasys::ScopeLock l(lock_, "BundleListIntMap::insert");
    oasys::ScopeLock bl(bundle->lock(), "BundleListIntMap::insert");
    add_bundle(bundle, bundle->bundleid());
}

//----------------------------------------------------------------------
void
BundleListIntMap::insert(bundleid_t key, Bundle* bundle)
{
    oasys::ScopeLock l(lock_, "BundleListIntMap::insert (with key)");
    oasys::ScopeLock bl(bundle->lock(), "BundleListIntMap::insert (with key)");
    add_bundle(bundle, key);
}

//----------------------------------------------------------------------
Bundle*
BundleListIntMap::del_bundle(const iterator& pos, bool used_notifier)
{
    Bundle* bundle = pos->second;
    ASSERT(lock_->is_locked_by_me());
    
    // lock the bundle
    oasys::ScopeLock l(bundle->lock(), "BundleListIntMap::del_bundle");

    // remove the mapping
    //log_debug("bundle id %" PRIbid " del_bundle: deleting mapping [%s]",
    //          bundle->bundleid(), name_.c_str());

    BundleMappings::iterator mapping = bundle->mappings()->find(this);
    if (mapping == bundle->mappings()->end()) {
        log_err("ERROR in del bundle: "
                "bundle id %" PRIbid " has no mapping for list [%s]",
                bundle->bundleid(), name_.c_str());
    } else {
        // retrieve the iterator from the mapping wrapper
        SPBMapping bmap ( *mapping );
        ASSERT(bmap->list() == this);
        SPV vpos = bmap->position();
        iterator* bitr = (iterator*) vpos.get();
        ASSERT(*bitr == pos);
        // everything checks out so remove the mapping
        bundle->mappings()->erase(mapping);
    }
    
    // remove the bundle from the list
    list_.erase(pos);
    
    // drain one element from the semaphore
    if (notifier_ && !used_notifier) {
        notifier_->drain_pipe(1);
    }

    // note that we explicitly do _not_ decrement the reference count
    // since the reference is passed to the calling function
    
    return bundle;
}

//----------------------------------------------------------------------
BundleRef
BundleListIntMap::pop_front(bool used_notifier)
{
    BundleRef ret("BundleListIntMap::pop_front() temporary");

    oasys::ScopeLock l(lock_, "pop_front");

    if (list_.empty()) {
        return ret;
    }
    
    ASSERT(!empty());

    // Assign the bundle to a temporary reference, then remove the
    // list reference on the bundle and return the temporary
    ret = del_bundle(list_.begin(), used_notifier);
    ret->del_ref(ltype_.c_str(), name_.c_str());
    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleListIntMap::pop_back(bool used_notifier)
{
    BundleRef ret("BundleListIntMap::pop_back() temporary");

    oasys::ScopeLock l(lock_, "pop_back");

    if (list_.empty()) {
        return ret;
    }

    // Assign the bundle to a temporary reference, then remove the
    // list reference on the bundle and return the temporary
    ret = del_bundle(--list_.end(), used_notifier);
    ret->del_ref(ltype_.c_str(), name_.c_str());
    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleListIntMap::pop_key(bundleid_t key, bool used_notifier)
{
    BundleRef ret("BundleListIntMap::pop_key() temporary");

    oasys::ScopeLock l(lock_, "pop_key");

    if (list_.empty()) {
        return ret;
    }

    iterator pos = list_.find(key);

    if ( pos != list_.end() )
    {
        // Assign the bundle to a temporary reference, then remove the
        // list reference on the bundle and return the temporary
        ret = del_bundle(pos, used_notifier);
        ret->del_ref(ltype_.c_str(), name_.c_str());
    }

    return ret;
}

//----------------------------------------------------------------------
bool
BundleListIntMap::erase(Bundle* bundle, bool used_notifier)
{
    if (bundle == NULL) {
        return false;
    }

    // The bundle list lock must always be taken before the
    // to-be-erased bundle lock.
    ASSERTF(!bundle->lock()->is_locked_by_me(),
            "bundle cannot be locked before calling erase "
            "due to potential deadlock");
    
    oasys::ScopeLock l(lock_, "BundleListIntMap::erase");

    // Now we need to take the bundle lock in order to search through
    // its mappings
    oasys::ScopeLock bl(bundle->lock(), "BundleListIntMap::erase");
    
    BundleMappings::iterator mapping = bundle->mappings()->find(this);
    if (mapping == bundle->mappings()->end()) {
        return false;
    }

    // retrieve the iterator from the mapping wrapper
    SPBMapping bmap ( *mapping );
    ASSERT(bmap->list() == this);
    SPV vpos = bmap->position();
    iterator* itr = (iterator*) vpos.get();
    ASSERT((*itr)->second == bundle);

    Bundle* b = del_bundle(*itr, used_notifier);
    ASSERT(b == bundle);
    
    bundle->del_ref(ltype_.c_str(), name_.c_str()); 
    return true;
}

//----------------------------------------------------------------------
bool
BundleListIntMap::erase(bundleid_t key, bool used_notifier)
{
    bool result = false;

    // first find the bundle in the list
    BundleRef bref(find(key));
    Bundle* bundle = bref.object();

    // delete all bundles with this key
    while ( NULL != bundle ) {
        result |= erase ( bundle, used_notifier );
        bref = find(key);
        bundle = bref.object();
    }

    return result;
}

//----------------------------------------------------------------------
void
BundleListIntMap::erase(iterator& iter, bool used_notifier)
{
    Bundle* bundle = iter->second;
    ASSERTF(!bundle->lock()->is_locked_by_me(),
            "bundle cannot be locked in erase due to potential deadlock");
    
    oasys::ScopeLock l(lock_, "BundleListIntMap::erase (by iter)");
    
    Bundle* b = del_bundle(iter, used_notifier);
    ASSERT(b == bundle);
    
    bundle->del_ref(ltype_.c_str(), name_.c_str()); 
}

//----------------------------------------------------------------------
bool
BundleListIntMap::contains(Bundle* bundle) const
{
    bool ret = false;
    if (bundle != NULL) {
        oasys::ScopeLock l(lock_, "BundleListIntMap::contains");
    
        ret = bundle->is_queued_on((BundleListBase*) this);

#undef DEBUG_MAPPINGS
#ifdef DEBUG_MAPPINGS
        bool ret2 = false;
        for (const_iterator i=list_.begin(); i!=list_.end(); ++i) {
            Bundle* b = i->second;
            if ( b == bundleref ) {
              ret2 = true;
              break;
            }
        }
        ASSERT(ret == ret2);
#endif
    }

    return ret;
}

//----------------------------------------------------------------------
bool
BundleListIntMap::contains(bundleid_t key) const
{
    oasys::ScopeLock l(lock_, "BundleListIntMap::contains");

    return (list_.find(key) != list_.end());
}

//----------------------------------------------------------------------
BundleRef
BundleListIntMap::find(bundleid_t key) const
{
    BundleRef ret("BundleListIntMap::find() temporary (by key)");

    oasys::ScopeLock l(lock_, "BundleListIntMap::find");

    const_iterator itr = list_.find(key);
    if ( itr != list_.end() )
    {
        ret = itr->second;
    }

    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleListIntMap::find_next(bundleid_t key) const
{
    BundleRef ret("BundleListIntMap::find() temporary (by key)");

    oasys::ScopeLock l(lock_, "BundleListIntMap::find");

    const_iterator itr = list_.upper_bound(key);
    if ( itr != list_.end() )
    {
        ret = itr->second;
    }

    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleListIntMap::find_for_storage(bundleid_t key) const
{
    BundleRef ret("BundleListIntMap::find() temporary (by key)");

    oasys::ScopeLock l(lock_, "BundleListIntMap::findi_for_storage");

    const_iterator itr = list_.find(key);
    if ( itr != list_.end() )
    {
        oasys::ScopeLock bl(itr->second->lock(), "BundleListIntMap::find_for_storage");
        if (!itr->second->is_freed()) {
            ret = itr->second;
        }
    }

    return ret;
}

//----------------------------------------------------------------------
void
BundleListIntMap::move_contents(BundleListIntMap* other)
{
    bundleid_t key;
    Bundle* bundle;

    oasys::ScopeLock l1(lock_, "BundleListIntMap::move_contents");
    oasys::ScopeLock l2(other->lock_, "BundleListIntMap::move_contents");
    
    while (!list_.empty()) {
        iterator itr = list_.begin();
        key = itr->first;
        bundle = itr->second;

        del_bundle(itr, false);
        
        other->insert(key, bundle);

        bundle->del_ref(ltype_.c_str(), name_.c_str()); 
    }
}

//----------------------------------------------------------------------
void
BundleListIntMap::clear()
{
    oasys::ScopeLock l(lock_, "BundleListIntMap::clear");
    
    BundleRef bref("BundleListIntMap::clear temporary");
    while (!list_.empty()) {
        bref = pop_front();
    }
}


//----------------------------------------------------------------------
size_t
BundleListIntMap::size() const
{
    oasys::ScopeLock l(lock_, "BundleListIntMap::size");
    return list_.size();
}

//----------------------------------------------------------------------
bool
BundleListIntMap::empty() const
{
    oasys::ScopeLock l(lock_, "BundleListIntMap::empty");
    return list_.empty();
}

//----------------------------------------------------------------------
BundleListIntMap::iterator
BundleListIntMap::begin() const
{
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleListIntMap before using iterator");

    // since all list accesses are protected via the BundleListIntMap class
    // const/non-const nature, there's no reason to use the stl
    // const_iterator type, so we need to cast away constness
    return const_cast<BundleListIntMap*>(this)->list_.begin();
}

//----------------------------------------------------------------------
BundleListIntMap::iterator
BundleListIntMap::end() const
{
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleListIntMap before using iterator");

    // see above
    return const_cast<BundleListIntMap*>(this)->list_.end();
}

//----------------------------------------------------------------------
void
BundleListIntMap::serialize(oasys::SerializeAction *a)
{
    BundleRef bref("BundleListIntMap::serialize temporary");
    Bundle* bundle;
    bundleid_t key;
    bundleid_t bid;
    size_t sz = size();

    oasys::ScopeLock l2(lock_, "serialize");

    a->process("size", &sz);

    if (a->action_code() == oasys::Serialize::MARSHAL || \
        a->action_code() == oasys::Serialize::INFO) {

        BundleListIntMap::iterator i;

        for (i=list_.begin();
             i!=list_.end();
             ++i) {
            key = i->first;
            bundle = i->second;
            //log_debug("List %s Marshaling bundle id %" PRIbid,
           // 		name().c_str(), bundle->bundleid());
            bid = bundle->bundleid();

            a->process("element", &bid);
            a->process("key", &key);
        }
    }

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        for ( size_t i=0; i<sz; i++ ) {
            a->process("element", &bid);
            a->process("key", &key);

            //log_debug("Unmarshaling bundle id %" PRIbid, bid);
            bref = BundleDaemon::instance()->all_bundles()->find(bid);
            bundle = bref.object();
            if ( bundle == NULL ) {
                //log_debug("bundle is NULL for id %" PRIbid " [key: %" PRIbid "]; not on all_bundles list", bid, key);
            }
            else if ( bundle->bundleid()==bid ) {
                //log_debug("Found bundle id %" PRIbid " [key: %" PRIbid "] on all_bundles; adding.", bid, key);
                insert(key, bundle);
            }
            //log_debug("Done with UNMARSHAL %" PRIbid, bid);
        }
        //log_debug("Done with UNMARSHAL");
    }
}

//----------------------------------------------------------------------
BlockingBundleListIntMap::BlockingBundleListIntMap(
                       const std::string& name, oasys::SpinLock* lock,
                       const std::string& ltype, const std::string& subpath)
    : BundleListIntMap(name, lock, ltype, subpath)
{
    notifier_ = new oasys::Notifier(logpath_);
}

//----------------------------------------------------------------------
BlockingBundleListIntMap::~BlockingBundleListIntMap()
{
    delete notifier_;
    notifier_ = NULL;
}

//----------------------------------------------------------------------
BundleRef
BlockingBundleListIntMap::pop_blocking(int timeout)
{
    ASSERT(notifier_);

    BundleRef bref("BlockingBundleListIntMap::pop_blocking temporary");

    //log_debug("pop_blocking on list %p [%s]", this, name().c_str());
    
    lock_->lock("BlockingBundleListIntMap::pop_blocking");

    bool used_wait;
    if (empty()) {
        used_wait = true;
        bool notified = notifier_->wait(lock_, timeout);
        ASSERT(lock_->is_locked_by_me());

        // if the timeout occurred, wait returns false, so there's
        // still nothing on the list
        if (!notified) {
            lock_->unlock();
            //log_debug("pop_blocking timeout on list %p", this);

            return bref;
        }
    } else {
        used_wait = false;
    }
    
    // This can't be empty if we got notified, unless there is another
    // thread waiting on the queue - which is an error.
    ASSERT(!empty());
    
    bref = pop_front(used_wait);
    
    lock_->unlock();

    //log_debug("pop_blocking got bundle %p from list %p", 
    //          bref.object(), this);
    
    return bref;
}


} // namespace dtn
