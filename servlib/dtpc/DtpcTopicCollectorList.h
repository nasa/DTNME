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

#ifndef _TOPIC_COLLECTOR_LIST_H_
#define _TOPIC_COLLECTOR_LIST_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <list>
#include <oasys/compat/inttypes.h>
#include <oasys/thread/Notifier.h>
#include <oasys/serialize/Serialize.h>


namespace oasys {
class SpinLock;
}

namespace dtn {

class DtpcTopicCollector;
class DtpcApplicationDataItem;

/**
 * List structure for handling DTPC Topic Collectors
 *
 * All list operations are protected with a spin lock so they are
 * thread safe. In addition, the lock itself is exposed so a routine
 * that needs to perform more complicated functions (like scanning the
 * list) must lock the list before doing so.
 *
 * NOTE: It is important that no locks be held on any collectors that
 * might be contained within the list when calling list manipulation
 * functions. Doing so could cause a potential deadlock due to a lock
 * ordering violation, as the list manipulation routines first lock
 * the list, and then lock the collector(s) being added or removed. 
 *
 * The internal data structure is an STL list of DtpcTopicCollector pointers. The
 * list is also derived from Notifier, and the various push() calls
 * will call notify() if there is a thread blocked on an empty list
 * waiting for notification.
 *
 * List methods also maintain mappings (i.e. "back pointers") in each
 * DtpcTopicCollector instance to the set of lists that contain the collector.
 *
 */
class DtpcTopicCollectorList : public oasys::Logger,
                               public oasys::SerializableObject
{
private:
    /**
     * Type for the list itself (private since it's irrelevant to the
     * outside).
     */
    typedef std::list<DtpcTopicCollector*> List;

public:
    /**
     * Type for an iterator, which just wraps an stl iterator. We
     * don't ever use the stl const_iterator type since list mutations
     * are protected via this class' methods.
     */
    typedef List::iterator iterator;

    /**
     * Constructor
     */
    DtpcTopicCollectorList(const std::string& name, oasys::SpinLock* lock = NULL);

    /**
     * Destructor -- clears the list.
     */
    virtual ~DtpcTopicCollectorList();

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);

    /**
     * Add a new collector to the back of the list.
     */
    virtual void push_back(DtpcTopicCollector* collector);

    /**
     * Pop the next available ApplicagtionDataItem and if a
     * TopicCollector is exhausted of items then it gets deleted
     */
    virtual DtpcApplicationDataItem* pop_data_item(bool used_notifier=false);

    /**
     * Remove (and return) a reference to the first collector on the list.
     *
     * @param used_notifier Popping off of the DtpcTopicCollectorList after coming 
     *     off of a notifier. This will drain one item off of the 
     *     notifier queue.
     *                    
     * @return a reference to the collector or a reference to NULL if the
     * list is empty.
     */
    virtual DtpcTopicCollector* pop_front();

    /**
     * Move all collectors from this list to another.
     */
    virtual void move_contents(DtpcTopicCollectorList* other);

    /**
     * Delete all expired TopicCollectors from the list
     */
    virtual void remove_expired_items();

    /**
     * Clear out the list.
     */
    virtual void clear();

    /**
     * Return the size of the list.
     */
    virtual size_t size() const;

    /**
     * Return whether or not the list is empty.
     */
    virtual bool empty() const;

    /**
     * Set the name (useful for classes that are unserialized).
     * Also sets the logpath
     */
    virtual void set_name(const std::string& name);
    
private:
    /**
     * Helper routine to add a collector at the indicated position.
     */
    virtual void add_collector(DtpcTopicCollector* collector, const iterator& pos);
    
protected:    
    List             list_;	///< underlying list data structure
    
    /// num collectors currently in the list
    size_t           list_size_;

    std::string      name_;	///< name of the list
    std::string      ltype_;	///< list type (path for the logger)

    oasys::SpinLock* lock_;	///< lock for notifier
    bool             own_lock_; ///< bit to define lock ownership
    oasys::Notifier* notifier_; ///< notifier for blocking list
};

/**
 * A simple derivative to the DtpcTopicCollectorList class that hooks in an oasys
 * Notifier, which thereby allows inter-thread signalling via a
 * pop_blocking() method. This allows one thread to block until
 * another has added a collector to the list.
 */
class BlockingDtpcTopicCollectorList : public DtpcTopicCollectorList {
public:
    BlockingDtpcTopicCollectorList(const std::string& name, oasys::SpinLock* lock = NULL);

    virtual ~BlockingDtpcTopicCollectorList();
    
    /**
     * Remove (and return) the first collector on the list, blocking
     * (potentially limited by the given timeout) if there are none.
     *
     * @return a reference to the collector or a reference to NULL if the
     * list is empty.
     */
    virtual DtpcApplicationDataItem* pop_blocking(int timeout = -1);

    /**
     * Accessor for the internal notifier.
     */
    virtual oasys::Notifier* notifier() { return notifier_; }
};

} // namespace dtn

#endif /* _TOPIC_COLLECTOR_LIST_H_ */
