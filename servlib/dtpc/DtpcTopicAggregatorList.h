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

#ifndef _TOPIC_AGGREGATOR_LIST_H_
#define _TOPIC_AGGREGATOR_LIST_H_

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

class DtpcTopicAggregator;
class DtpcApplicationDataItem;

/**
 * List structure for handling DTPC Topic Aggregators
 *
 * All list operations are protected with a spin lock so they are
 * thread safe. In addition, the lock itself is exposed so a routine
 * that needs to perform more complicated functions (like scanning the
 * list) must lock the list before doing so.
 *
 * The internal data structure is an STL list of DtpcTopicAggregator pointers. The
 * list is also derived from Notifier, and the various push() calls
 * will call notify() if there is a thread blocked on an empty list
 * waiting for notification.
 */
class DtpcTopicAggregatorList : public oasys::Logger {
private:
    /**
     * Type for the list itself (private since it's irrelevant to the
     * outside).
     */
    typedef std::list<DtpcTopicAggregator*> List;

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
    DtpcTopicAggregatorList(const std::string& name, oasys::SpinLock* lock = NULL);

    /**
     * Destructor -- clears the list.
     */
    virtual ~DtpcTopicAggregatorList();

    /**
     * Add a new aggregator to the back of the list.
     */
    virtual void push_back(DtpcTopicAggregator* aggregator);

    /**
     * Remove (and return) a reference to the first aggregator on the list.
     *
     * @param used_notifier Popping off of the DtpcTopicAggregatorList after coming 
     *     off of a notifier. This will drain one item off of the 
     *     notifier queue.
     *                    
     * @return a reference to the aggregator or a reference to NULL if the
     * list is empty.
     */
    virtual DtpcTopicAggregator* pop_front(bool used_notifier = false);

    /**
     * Move all aggregators from this list to another.
     */
    virtual void move_contents(DtpcTopicAggregatorList* other);

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
     * Helper routine to add a aggregator at the indicated position.
     */
    virtual void add_aggregator(DtpcTopicAggregator* aggregator, const iterator& pos);
    
protected:    
    List             list_;	///< underlying list data structure
    
    /// num aggregators currently in the list
    size_t           list_size_;

    std::string      name_;	///< name of the list
    std::string      ltype_;	///< list type (path for the logger)

    oasys::SpinLock* lock_;	///< lock for notifier
    bool             own_lock_; ///< bit to define lock ownership
    oasys::Notifier* notifier_; ///< notifier for blocking list
};

/**
 * A simple derivative to the DtpcTopicAggregatorList class that hooks in an oasys
 * Notifier, which thereby allows inter-thread signalling via a
 * pop_blocking() method. This allows one thread to block until
 * another has added a aggregator to the list.
 */
class BlockingDtpcTopicAggregatorList : public DtpcTopicAggregatorList {
public:
    BlockingDtpcTopicAggregatorList(const std::string& name, oasys::SpinLock* lock = NULL);

    virtual ~BlockingDtpcTopicAggregatorList();
    
    /**
     * Remove (and return) the first aggregator on the list, blocking
     * (potentially limited by the given timeout) if there are none.
     *
     * @return a reference to the aggregator or a reference to NULL if the
     * list is empty.
     */
    virtual DtpcTopicAggregator* pop_blocking(int timeout = -1);

    /**
     * Accessor for the internal notifier.
     */
    virtual oasys::Notifier* notifier() { return notifier_; }
};

} // namespace dtn

#endif /* _TOPIC_AGGREGATOR_LIST_H_ */
