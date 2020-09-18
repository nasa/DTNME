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

#include "DtpcTopicCollector.h"
#include "DtpcTopicCollectorList.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcTopicCollectorList::DtpcTopicCollectorList(
                       const std::string& name, oasys::SpinLock* lock)
    : Logger("DtpcTopicCollectorList", "/dtpc/topic/collector/list/%s", name.c_str()),
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
DtpcTopicCollectorList::set_name(const std::string& name)
{
    name_ = name;
    logpathf("/dtpc/topic/collector/list/%s", name.c_str());
}

//----------------------------------------------------------------------
DtpcTopicCollectorList::~DtpcTopicCollectorList()
{
    clear();

    if (own_lock_) {
        delete lock_;
    }
    lock_ = NULL;
}

//----------------------------------------------------------------------
void
DtpcTopicCollectorList::serialize(oasys::SerializeAction* a)
{
    DtpcTopicCollector* collector;

    oasys::ScopeLock l(lock_, "serialize");

    a->process("num_collectors", &list_size_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        size_t num_collectors = list_size_;
        list_size_ = 0;
        for ( size_t ix=0; ix<num_collectors; ++ix ) {
            collector = new DtpcTopicCollector(oasys::Builder::builder());
            collector->serialize(a);
            add_collector(collector, list_.end());  // increments list_size_
        }
    } else {
        // MARSHAL or INFO
        iterator pos = list_.begin();
        while (pos != list_.end()) {
            collector = *pos;
            collector->serialize(a);
            pos++;
        }
    }
}

//----------------------------------------------------------------------
void
DtpcTopicCollectorList::add_collector(DtpcTopicCollector* collector, const iterator& pos)
{
    ASSERT(lock_->is_locked_by_me());
    
    list_.insert(pos, collector);

    ++list_size_;
 
    if (notifier_ != 0) {
        notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
DtpcTopicCollectorList::push_back(DtpcTopicCollector* collector)
{
    oasys::ScopeLock l(lock_, "push_back");
    add_collector(collector, list_.end());
}
        
//----------------------------------------------------------------------
DtpcTopicCollector*
DtpcTopicCollectorList::pop_front()
{
    DtpcTopicCollector* ret = NULL;

    oasys::ScopeLock l(lock_, "pop_front");

    if (list_size_ > 0) {
        iterator pos = list_.begin();
        ret = *pos;
        list_.erase(pos);
        --list_size_;
    }
    
    return ret;
}

//----------------------------------------------------------------------
DtpcApplicationDataItem*
DtpcTopicCollectorList::pop_data_item(bool used_notifier)
{
    DtpcApplicationDataItem* data_item = NULL;

    oasys::ScopeLock l(lock_, "pop_data_item");

    while (NULL == data_item && list_size_ > 0) {
        iterator pos = list_.begin();
        DtpcTopicCollector* collector = *pos;

        // drain one element from the semaphore
        if (notifier_ && !used_notifier) {
            notifier_->drain_pipe(1);
        }

        if (collector->expired()) {
            // delete this collector and try the next one
            list_.erase(pos);
            --list_size_;
            delete collector;
        } else {
            data_item = collector->pop_data_item();

            if (notifier_) {
                // trigger the notifier again if this collector still has data items
                if (collector->item_count() > 0) {
                    notifier_->notify();
                }
            }
            // if all data items removed from the collector then remove and delete it
            if (0 == collector->item_count()) {
                list_.erase(pos);
                --list_size_;
                delete collector;
            }
        }
    }

    return data_item;
}

//----------------------------------------------------------------------
void
DtpcTopicCollectorList::move_contents(DtpcTopicCollectorList* other)
{
    oasys::ScopeLock l1(lock_, "move_contents");
    oasys::ScopeLock l2(other->lock_, "move_contents");

    DtpcTopicCollector* collector;
    while (!list_.empty()) {
        collector = pop_front();
        if (!collector->expired()) {
            other->push_back(collector);
        } else {
            delete collector;
        }
    }
}

//----------------------------------------------------------------------
void
DtpcTopicCollectorList::remove_expired_items()
{
    oasys::ScopeLock l(lock_, "remove_expired_items");
    DtpcTopicCollector* collector;
    iterator del_itr;
    iterator pos = list_.begin();
    while (pos != list_.end()) {
        collector = *pos;
        del_itr = pos++;

        if (collector->expired()) {
            delete collector;
            list_.erase(del_itr);
            --list_size_;
        }
    }
}

//----------------------------------------------------------------------
void
DtpcTopicCollectorList::clear()
{
    oasys::ScopeLock l(lock_, "clear");
    
    DtpcTopicCollector* collector;
    while (!list_.empty()) {
        collector = pop_front();
        delete collector;
    }
    list_size_ = 0;
}


//----------------------------------------------------------------------
size_t
DtpcTopicCollectorList::size() const
{
    oasys::ScopeLock l(lock_, "size");
    return list_size_;
}

//----------------------------------------------------------------------
bool
DtpcTopicCollectorList::empty() const
{
    oasys::ScopeLock l(lock_, "empty");
    return (0 == list_size_);
}

//----------------------------------------------------------------------
BlockingDtpcTopicCollectorList::BlockingDtpcTopicCollectorList(const std::string& name, oasys::SpinLock* lock)
    : DtpcTopicCollectorList(name, lock)
{
    notifier_ = new oasys::Notifier(logpath_);
}

//----------------------------------------------------------------------
BlockingDtpcTopicCollectorList::~BlockingDtpcTopicCollectorList()
{
    clear();

    delete notifier_;
    notifier_ = NULL;
}

//----------------------------------------------------------------------
DtpcApplicationDataItem*
BlockingDtpcTopicCollectorList::pop_blocking(int timeout)
{
    ASSERT(notifier_);

    log_debug("pop_blocking on list %p [%s]", this, name_.c_str());
    
    DtpcApplicationDataItem* ret = NULL;

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
    
    ret = pop_data_item(used_wait);
    
    lock_->unlock();

    log_debug("pop_blocking got data item %p from list %p", 
              ret, this);
    
    return ret;
}

} // namespace dtn

#endif // DTPC_ENABLED
