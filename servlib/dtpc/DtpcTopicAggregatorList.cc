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

#ifdef DTPC_ENABLED

#include <algorithm>
#include <stdlib.h>
#include <oasys/thread/SpinLock.h>

#include "DtpcTopicAggregator.h"
#include "DtpcTopicAggregatorList.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcTopicAggregatorList::DtpcTopicAggregatorList(
                       const std::string& name, oasys::SpinLock* lock)
    : Logger("DtpcTopicAggregatorList", "/dtpc/topic/aggregator/list/%s", name.c_str()),
      name_(name), notifier_(NULL)
{
    if (lock != NULL) {
        lock_     = lock;
        own_lock_ = false;
    } else {
        lock_     = new oasys::SpinLock();
        own_lock_ = true;
    }

    list_size_ = 0;
}

//----------------------------------------------------------------------
void
DtpcTopicAggregatorList::set_name(const std::string& name)
{
    name_ = name;
    logpathf("/dtpc/topic/aggregator/list/%s", name.c_str());
}

//----------------------------------------------------------------------
DtpcTopicAggregatorList::~DtpcTopicAggregatorList()
{
    clear();

    if (own_lock_) {
        delete lock_;
    }
    lock_ = NULL;
}

//----------------------------------------------------------------------
void
DtpcTopicAggregatorList::add_aggregator(DtpcTopicAggregator* aggregator, const iterator& pos)
{
    ASSERT(lock_->is_locked_by_me());
    
    list_.insert(pos, aggregator);

    ++list_size_;
 
    if (notifier_ != 0) {
        notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
DtpcTopicAggregatorList::push_back(DtpcTopicAggregator* aggregator)
{
    oasys::ScopeLock l(lock_, "push_back");
    add_aggregator(aggregator, list_.end());
}
        
//----------------------------------------------------------------------
DtpcTopicAggregator*
DtpcTopicAggregatorList::pop_front(bool used_notifier)
{
    DtpcTopicAggregator* ret = NULL;

    oasys::ScopeLock l(lock_, "pop_front");

    if (list_size_ > 0) {
        iterator pos = list_.begin();
        ret = *pos;
        list_.erase(pos);
        --list_size_;

        // drain one element from the semaphore
        if (notifier_ && !used_notifier) {
            notifier_->drain_pipe(1);
        }
    }
    
    return ret;
}

//----------------------------------------------------------------------
void
DtpcTopicAggregatorList::move_contents(DtpcTopicAggregatorList* other)
{
    oasys::ScopeLock l1(lock_, "move_contents");
    oasys::ScopeLock l2(other->lock_, "move_contents");

    DtpcTopicAggregator* aggregator;
    while (!list_.empty()) {
        aggregator = pop_front();
        other->push_back(aggregator);
    }
}

//----------------------------------------------------------------------
void
DtpcTopicAggregatorList::clear()
{
    oasys::ScopeLock l(lock_, "clear");
    
    DtpcTopicAggregator* aggregator;
    while (!list_.empty()) {
        aggregator = pop_front();
        //this is not the master list so do not delete the aggregator
    }
    list_size_ = 0;
}


//----------------------------------------------------------------------
size_t
DtpcTopicAggregatorList::size() const
{
    oasys::ScopeLock l(lock_, "size");
    return list_size_;
}

//----------------------------------------------------------------------
bool
DtpcTopicAggregatorList::empty() const
{
    oasys::ScopeLock l(lock_, "empty");
    return (0 == list_size_);
}

//----------------------------------------------------------------------
BlockingDtpcTopicAggregatorList::BlockingDtpcTopicAggregatorList(const std::string& name, oasys::SpinLock* lock)
    : DtpcTopicAggregatorList(name, lock)
{
    notifier_ = new oasys::Notifier(logpath_);
}

//----------------------------------------------------------------------
BlockingDtpcTopicAggregatorList::~BlockingDtpcTopicAggregatorList()
{
    clear();

    delete notifier_;
    notifier_ = NULL;
}

//----------------------------------------------------------------------
DtpcTopicAggregator*
BlockingDtpcTopicAggregatorList::pop_blocking(int timeout)
{
    ASSERT(notifier_);

    log_debug("pop_blocking on list %p [%s]", this, name_.c_str());
    
    DtpcTopicAggregator* ret = NULL;

    lock_->lock("pop_blocking");

    bool used_wait;
    if (empty()) {
        used_wait = true;
        bool notified = notifier_->wait(lock_, timeout);
        ASSERT(lock_->is_locked_by_me());

        // if the timeout occurred, wait returns false, so there's
        // still nothing on the list
        if (!notified) {
            lock_->unlock();
            log_debug("pop_blocking timeout on list %p", this);

            return ret;
        }
    } else {
        used_wait = false;
    }
    
    // This can't be empty if we got notified, unless there is another
    // thread waiting on the queue - which is an error.
    ASSERT(!empty());
    
    ret = pop_front(used_wait);
    
    lock_->unlock();

    log_debug("pop_blocking got topic aggregator %p from list %p", 
              ret, this);
    
    return ret;
}

} // namespace dtn

#endif // DTPC_ENABLED
