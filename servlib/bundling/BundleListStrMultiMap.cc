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
#include "BundleListStrMultiMap.h"
#include "BundleMappings.h"
#include "BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
BundleListStrMultiMap::BundleListStrMultiMap(const std::string& name, oasys::SpinLock* lock,
                         const std::string& ltype, const std::string& subpath)
    : BundleListBase(name, lock, ltype, subpath)
{
}

//----------------------------------------------------------------------
void
BundleListStrMultiMap::set_name(const std::string& name)
{
    name_ = name;
    logpathf("/dtn/bundle/liststrmultmap/%s", name.c_str());
}

//----------------------------------------------------------------------
BundleListStrMultiMap::~BundleListStrMultiMap()
{
    clear();
}

//----------------------------------------------------------------------
BundleRef
BundleListStrMultiMap::front() const
{
    BundleRef ret("BundleListStrMultiMap::front() temporary");

    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::front");
    
    if (list_.empty())
    {
        return ret;
    }

    ret = list_.begin()->second;
    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleListStrMultiMap::back() const
{
    BundleRef ret("BundleListStrMultiMap::back() temporary");

    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::back");

    if (list_.empty())
    {
        return ret;
    }

    ret = (--list_.end())->second;
    return ret;
}

//----------------------------------------------------------------------
void
BundleListStrMultiMap::add_bundle(Bundle* bundle, const std::string key)
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
    SPV blpos ( new iterator(list_.insert(MPair(key, bundle))) );
    SPBMapping bmap ( new BundleMapping(this, blpos) );
    bundle->mappings()->push_back(bmap);
    bundle->add_ref(ltype_.c_str(), name_.c_str());
    
    if (notifier_ != 0) {
        notifier_->notify();
    }

    //log_debug("bundle id %" PRIbid " add mapping [%s] to list %p",
    //              bundle->bundleid(), name_.c_str(), this);
}

//----------------------------------------------------------------------
void
BundleListStrMultiMap::insert(Bundle* bundle)
{
    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::insert");
    oasys::ScopeLock bl(bundle->lock(), "BundleListStrMultiMap::insert");
    add_bundle(bundle, bundle->gbofid_str());
}

//----------------------------------------------------------------------
void
BundleListStrMultiMap::insert(std::string key, Bundle* bundle)
{
    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::insert (with key)");
    oasys::ScopeLock bl(bundle->lock(), "BundleListStrMultiMap::insert (with key)");
    add_bundle(bundle, key);
}

//----------------------------------------------------------------------
Bundle*
BundleListStrMultiMap::del_bundle(const iterator& pos, bool used_notifier)
{
    Bundle* bundle = pos->second;
    ASSERT(lock_->is_locked_by_me());
    
    // lock the bundle
    oasys::ScopeLock l(bundle->lock(), "BundleListStrMultiMap::del_bundle");

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
BundleListStrMultiMap::pop_front(bool used_notifier)
{
    BundleRef ret("BundleListStrMultiMap::pop_front() temporary");

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
BundleListStrMultiMap::pop_back(bool used_notifier)
{
    BundleRef ret("BundleListStrMultiMap::pop_back() temporary");

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
BundleListStrMultiMap::pop_key(std::string key, bool used_notifier)
{
    BundleRef ret("BundleListStrMultiMap::pop_key() temporary");

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
BundleListStrMultiMap::erase(Bundle* bundle, bool used_notifier)
{
    if (bundle == NULL) {
        return false;
    }

    // The bundle list lock must always be taken before the
    // to-be-erased bundle lock.
    ASSERTF(!bundle->lock()->is_locked_by_me(),
            "bundle cannot be locked before calling erase "
            "due to potential deadlock");
    
    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::erase");

    // Now we need to take the bundle lock in order to search through
    // its mappings
    oasys::ScopeLock bl(bundle->lock(), "BundleListStrMultiMap::erase");
    
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
BundleListStrMultiMap::erase(std::string key, bool used_notifier)
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
BundleListStrMultiMap::erase(iterator& iter, bool used_notifier)
{
    Bundle* bundle = iter->second;
    ASSERTF(!bundle->lock()->is_locked_by_me(),
            "bundle cannot be locked in erase due to potential deadlock");
    
    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::erase (by iter)");
    
    Bundle* b = del_bundle(iter, used_notifier);
    ASSERT(b == bundle);
    
    bundle->del_ref(ltype_.c_str(), name_.c_str()); 
}

//----------------------------------------------------------------------
bool
BundleListStrMultiMap::contains(Bundle* bundle) const
{
    bool ret = false;
    if (bundle != NULL) {
        oasys::ScopeLock l(lock_, "BundleListStrMultiMap::contains");
    
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
BundleListStrMultiMap::contains(std::string key) const
{
    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::contains");

    return (list_.find(key) != list_.end());
}

//----------------------------------------------------------------------
BundleRef
BundleListStrMultiMap::find(std::string key) const
{
    BundleRef ret("BundleListStrMultiMap::find() temporary (by key)");

    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::find");

    const_iterator itr = list_.find(key);
    if ( itr != list_.end() )
    {
        ret = itr->second;
    }

    return ret;
}

//----------------------------------------------------------------------
void
BundleListStrMultiMap::move_contents(BundleListStrMultiMap* other)
{
    std::string key;
    Bundle* bundle;

    oasys::ScopeLock l1(lock_, "BundleListStrMultiMap::move_contents");
    oasys::ScopeLock l2(other->lock_, "BundleListStrMultiMap::move_contents");
    
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
BundleListStrMultiMap::clear()
{
    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::clear");
    
    BundleRef bref("BundleListStrMultiMap::clear temporary");
    while (!list_.empty()) {
        bref = pop_front();
    }
}


//----------------------------------------------------------------------
size_t
BundleListStrMultiMap::size() const
{
    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::size");
    return list_.size();
}

//----------------------------------------------------------------------
bool
BundleListStrMultiMap::empty() const
{
    oasys::ScopeLock l(lock_, "BundleListStrMultiMap::empty");
    return list_.empty();
}

//----------------------------------------------------------------------
BundleListStrMultiMap::iterator
BundleListStrMultiMap::begin() const
{
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleListStrMultiMap before using iterator");

    // since all list accesses are protected via the BundleListStrMultiMap class
    // const/non-const nature, there's no reason to use the stl
    // const_iterator type, so we need to cast away constness
    return const_cast<BundleListStrMultiMap*>(this)->list_.begin();
}

//----------------------------------------------------------------------
BundleListStrMultiMap::iterator
BundleListStrMultiMap::end() const
{
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleListStrMultiMap before using iterator");

    // see above
    return const_cast<BundleListStrMultiMap*>(this)->list_.end();
}

//----------------------------------------------------------------------
void
BundleListStrMultiMap::serialize(oasys::SerializeAction *a)
{
    BundleRef bref("BundleListStrMultiMap::serialize temporary");
    Bundle* bundle;
    std::string key;
    bundleid_t bid;
    size_t sz = size();

    oasys::ScopeLock l2(lock_, "serialize");

    a->process("size", &sz);

    if (a->action_code() == oasys::Serialize::MARSHAL || \
        a->action_code() == oasys::Serialize::INFO) {

        BundleListStrMultiMap::iterator i;

        for (i=list_.begin();
             i!=list_.end();
             ++i) {
            key = i->first;
            bundle = i->second;
            //log_debug("List %s Marshaling bundle id %" PRIbid,
            //		name().c_str(), bundle->bundleid());
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
                //log_debug("bundle is NULL for id %" PRIbid " [key: %s]; not on all_bundles list", bid, key.c_str());
            }
            else if ( bundle->bundleid()==bid ) {
                //log_debug("Found bundle id %" PRIbid " [key: %s] on all_bundles; adding.", bid, key.c_str());
                insert(key, bundle);
            }
            //log_debug("Done with UNMARSHAL %" PRIbid, bid);
        }
        //log_debug("Done with UNMARSHAL");
    }
}

//----------------------------------------------------------------------
BlockingBundleListStrMultiMap::BlockingBundleListStrMultiMap(
                       const std::string& name, oasys::SpinLock* lock,
                       const std::string& ltype, const std::string& subpath)
    : BundleListStrMultiMap(name, lock, ltype, subpath)
{
    notifier_ = new oasys::Notifier(logpath_);
}

//----------------------------------------------------------------------
BlockingBundleListStrMultiMap::~BlockingBundleListStrMultiMap()
{
    delete notifier_;
    notifier_ = NULL;
}

//----------------------------------------------------------------------
BundleRef
BlockingBundleListStrMultiMap::pop_blocking(int timeout)
{
    ASSERT(notifier_);

    BundleRef bref("BlockingBundleListStrMultiMap::pop_blocking temporary");

    //log_debug("pop_blocking on list %p [%s]", this, name().c_str());
    
    lock_->lock("BlockingBundleListStrMultiMap::pop_blocking");

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
