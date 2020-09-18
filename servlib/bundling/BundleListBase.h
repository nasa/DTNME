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

#ifndef _BUNDLE_LIST_BASE_H_
#define _BUNDLE_LIST_BASE_H_

#include <oasys/compat/inttypes.h>
#include <oasys/thread/Notifier.h>
#include <oasys/serialize/Serialize.h>

#include "BundleRef.h"

namespace oasys {
class SpinLock;
}

namespace dtn {

class Bundle;
struct BundleTimestamp;

/**
 * This is the base class for BundleLists which can be of various 
 * flavors such as a std::list, std::map, etc. This class implements
 * the common elements of the lists.
 *
 * The internal data structure of Bundle pointers will vary in the
 * derived classes. The list is derived from Notifier, and the various 
 * push() calls in the derived classes must call notify() if there is 
 * a thread blocked on an empty list waiting for notification.
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
class BundleListBase : public oasys::Logger,  public oasys::SerializableObject {
private:
    /**
     * Type for the list itself (private since it's irrelevant to the
     * outside).
     * Derived clases should declare a private list type. Examples:
     *    //typedef std::list<const BundleRef*> List;
     *    //typedef std::map<u_int32_t, const BundleRef*> List;
     */

public:
    /**
     * Type for an iterator, which just wraps an stl iterator. We
     * don't ever use the stl const_iterator type since list mutations
     * are protected via this class' methods.
     */
    //typedef List::iterator iterator;

    /**
     * Constructor
     */
    BundleListBase(const std::string& name, oasys::SpinLock* lock = NULL,
               const std::string& ltype="BundleListBase", const std::string& subpath="/listbase");

    /**
     * Destructor -- clears the list.
     */
    virtual ~BundleListBase();

    /*
     * Serializes the list of (internal) bundleIDs; on deserialization,
     * tries to hunt down those bundles in the all_bundles list and add
     * them to the list.
     */
    virtual void serialize(oasys::SerializeAction *a) = 0;

    /**
     * Clear out the list.
     */
    virtual void clear() = 0;

    /**
     * Return the size of the list.
     */
    virtual size_t size() const = 0;

    /**
     * Return whether or not the list is empty.
     */
    virtual bool empty() const = 0;

    /**
     * Return the internal lock on this list.
     */
    oasys::SpinLock* lock() const { return lock_; }

    /**
     * Return the identifier name of this list.
     */
    const std::string& name() const { return name_; }

    /**
     * Set the name (useful for classes that are unserialized).
     * Also sets the logpath
     */
    virtual void set_name(const std::string& name);
    
    /**
     * As above, but sets ONLY the name, not the logpath.
     */
    void set_name_only(const std::string& name);

private:
    // Derived classes should declare the List structure here
    //List             list_;	///< underlying list data structure
    
protected:
    std::string      name_;	///< name of the list
    std::string      ltype_;	///< list type (path for the logger)

    oasys::SpinLock* lock_;	///< lock for notifier
    bool             own_lock_; ///< bit to define lock ownership
    oasys::Notifier* notifier_; ///< notifier for blocking list
};

} // namespace dtn

#endif /* _BUNDLE_LIST_BASE_H_ */
