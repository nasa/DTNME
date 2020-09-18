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

#ifndef _BUNDLE_LIST_STRMAP_H_
#define _BUNDLE_LIST_STRMAP_H_

#include <map>
#include <string>
#include <oasys/compat/inttypes.h>
#include <oasys/thread/Notifier.h>
#include <oasys/serialize/Serialize.h>

#include "BundleListBase.h"
#include "BundleRef.h"

namespace oasys {
class SpinLock;
}

namespace dtn {

class Bundle;
struct BundleTimestamp;

/**
 * Map<string, Bundle*> structure for handling bundles  
 * by a string key. The expected purpose is to speed up finding
 * bundles by the GbofId key string and possibly as a priority queue.
 *
 * All list operations are protected with a spin lock so they are
 * thread safe. In addition, the lock itself is exposed so a routine
 * that needs to perform more complicated functions (like scanning the
 * list) must lock the list before doing so.
 *
 * NOTE: It is important that no locks be held on any bundles that
 * might be contained within the list when calling list manipulation
 * functions. Doing so could cause a potential deadlock due to a lock
 * ordering violation, as the list manipulation routines first lock
 * the list, and then lock the bundle(s) being added or removed. 
 *
 * The internal data structure is an STL map of Bundle pointers. The
 * list is also derived from Notifier, and the various push() calls
 * will call notify() if there is a thread blocked on an empty list
 * waiting for notification.
 *
 * List methods also maintain mappings (i.e. "back pointers") in each
 * Bundle instance to the set of lists that contain the bundle.
 *
 * Lists follow the reference counting rules for bundles. In
 * particular, the push*() methods increment the reference count, and
 * erase() decrements it. In particular, the pop() variants (as well
 * as the other accessors) return instances of the BundleRef classes
 * that forces the caller to use the BundleRef classes as well in
 * order to properly maintain the reference counts.
 *
 */
class BundleListStrMap : public BundleListBase {
private:
    /**
     * Type for the list itself (private since it's irrelevant to the
     * outside).
     */
    typedef std::map<std::string, Bundle*> List;
    typedef std::pair<std::string, Bundle*> MPair;

public:
    /**
     * Type for an iterator, which just wraps an stl iterator. We
     * don't ever use the stl const_iterator type since list mutations
     * are protected via this class' methods.
     */
    typedef List::iterator iterator;
    typedef List::const_iterator const_iterator;
    typedef std::pair<iterator,bool> IPair;

    /**
     * Constructor
     */
    BundleListStrMap(const std::string& name, oasys::SpinLock* lock = NULL,
                          const std::string& ltype="BundleListStrMap", 
                          const std::string& subpath="/liststrmap/");

    /**
     * Destructor -- clears the list.
     */
    virtual ~BundleListStrMap();

    /**
     * Peek at the first bundle on the list.
     *
     * @return the bundle or NULL if the list is empty
     */
    virtual BundleRef front() const;

    /*
     * Serializes the list of (internal) bundleIDs; on deserialization,
     * tries to hunt down those bundles in the all_bundles list and add
     * them to the list.
     */
    virtual void serialize(oasys::SerializeAction *a);

    /**
     * Peek at the last bundle on the list.
     *
     * @return the bundle or NULL if the list is empty
     */
    virtual BundleRef back() const;

    /**
     * Insert a new bundleref into the list by Bundle ID
     */
    virtual void insert(Bundle* bundle);

    virtual void insert(const BundleRef& bref) {
        insert(bref.object());
    }

    /**
     * Insert method compatible with BundleList
     */
    virtual void push_back(Bundle* bundle) {
        insert(bundle);
    }

    /**
     * Insert a new bundleref into the list using key
     */
    virtual void insert(std::string key, Bundle* bundle);

    virtual void insert(std::string key, const BundleRef& bref) {
        insert(key, bref.object());
    }

    /**
     * Remove (and return) a reference to the first bundle on the list.
     *
     * @param used_notifier Popping off of the BundleListStrMap after coming 
     *     off of a notifier. This will drain one item off of the 
     *     notifier queue.
     *                    
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    virtual BundleRef pop_front(bool used_notifier = false);

    /**
     * Remove (and return) the last bundle on the list.
     *
     * Note (as explained above) that this does not decrement the
     * bundle reference count.
     *
     * @param used_notifier Popping off of the BundleListStrMap after coming 
     *     off of a notifier. This will drain one item off of the 
     *     notifier queue.
     *
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    virtual BundleRef pop_back(bool used_notifier = false);

    /**
     * Remove (and return) the first bundle on the list identified by key.
     *
     * Note (as explained above) that this does not decrement the
     * bundle reference count.
     *
     * @param used_notifier Popping off of the BundleListStrMap after coming 
     *     off of a notifier. This will drain one item off of the 
     *     notifier queue.
     *
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    virtual BundleRef pop_key(std::string key, bool used_notifier = false);

    /**
     * Remove the given bundle from the list. Returns true if the
     * bundle was successfully removed, false otherwise.
     *
     * Unlike the pop() functions, this does remove the list's
     * reference on the bundle.
     */
    virtual bool erase(Bundle* bundle, bool used_notifier = false);

    /**
     * Remove the bundle at the given list position.
     */
    virtual void erase(iterator& iter, bool used_notifier = false);
    
    /**
     * Remove all bundles identified by key.
     */
    virtual bool erase(std::string key, bool used_notifier = false);

    /**
     * Search the list for the given bundle by GbofId string
     * 
     * @return true if found, false if not
     */
    virtual bool contains(Bundle* bundle) const;

    /**
     * Search the list for the given bundle by GbofId string
     * 
     * @return true if found, false if not
     */
    virtual bool contains(const BundleRef& bref) const
    {
        return contains(bref.object());
    }

    /**
     * Search the list for the given bundle by key.
     *
     * @return true if found, false if not
     */
    virtual bool contains(std::string key) const;

    /**
     * Search the list for a bundle with the given key.
     *
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    virtual BundleRef find(std::string key) const;

    /**
     * Move all bundles from this list to another.
     */
    virtual void move_contents(BundleListStrMap* other);

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
     * Iterator used to iterate through the list. Iterations _must_ be
     * completed while holding the list lock, and this method will
     * assert as such.
     */
    virtual iterator begin() const;
    
    /**
     * Iterator used to mark the end of the list. Iterations _must_ be
     * completed while holding the list lock, and this method will
     * assert as such.
     */
    virtual iterator end() const;

    /**
     * Set the name (useful for classes that are unserialized).
     * Also sets the logpath
     */
    virtual void set_name(const std::string& name);

private:
    /**
     * Helper routine to add a bundle.
     */
    virtual void add_bundle(Bundle* bundle, const std::string key);
    
    /**
     * Helper routine to remove a bundle from the indicated position.
     *
     * @param pos	    Position to delete
     * @param used_notifier Popping off of the BundleListStrMap after coming
     *                      off of a notifier. This will drain one item
     *                      off of the notifier queue.
     *
     * @returns the bundle that, before this call, was at the position
     */
    virtual Bundle* del_bundle(const iterator& pos, bool used_notifier);
    
    List             list_;	///< underlying list data structure (a map in this case)
    
protected:
};

/**
 * A simple derivative to the BundleListStrMap class that hooks in an oasys
 * Notifier, which thereby allows inter-thread signalling via a
 * pop_blocking() method. This allows one thread to block until
 * another has added a bundle to the list.
 */
class BlockingBundleListStrMap : public BundleListStrMap {
public:
    BlockingBundleListStrMap(const std::string& name, oasys::SpinLock* lock = NULL,
                       const std::string& ltype="BlockingBundleListStrMap", 
                       const std::string& subpath="/blockingliststrmap/");

    virtual ~BlockingBundleListStrMap();
    
    /**
     * Remove (and return) the first bundle on the list, blocking
     * (potentially limited by the given timeout) if there are none.
     *
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    BundleRef pop_blocking(int timeout = -1);

    /**
     * Accessor for the internal notifier.
     */
    oasys::Notifier* notifier() { return notifier_; }
};

}

#endif /* _BUNDLE_LIST_STRMAP_H_ */
