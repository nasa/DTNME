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

#ifndef _CUSTODY_LIST_H_
#define _CUSTODY_LIST_H_

#include <memory>
#include <map>
#include <string>

#include "BundleListIntMap.h"
#include "BundleRef.h"

namespace oasys {
class SpinLock;
}

namespace dtn {

class Bundle;

/**
 * Wrapper around a collection for CustodyList's maintained 
 * for each destination.
 */
class CustodyList : public oasys::Logger {
protected:
    /**
     * Types used internally
     */

    struct DestinationList {
        uint64_t              last_custody_id_ = 0;
        SPtr_BundleListIntMap dest_list_;
    };

    typedef std::shared_ptr<struct DestinationList> SPtr_DestinationList;

    typedef std::map<std::string, SPtr_DestinationList> List;
    typedef List::iterator Iterator;

public:
    /**
     * Type for an iterator, which just wraps an stl iterator. We
     * don't ever use the stl const_iterator type since list mutations
     * are protected via this class' methods.
     */
//    typedef BundleListIntMap::List::iterator iterator;
//    typedef BundleListIntMap::List::const_iterator const_iterator;
//    typedef std::pair<iterator,bool> IPair;

    /**
     * Constructor
     */
    CustodyList();

    /**
     * Destructor -- clears the list.
     */
    virtual ~CustodyList();

    /**
     * Gets the last assigned CustodyID for a given destination EID
     */
    virtual uint64_t last_custody_id(std::string& dest);

    /**
     * Insert a bundle into the apprpriate custody list
     * and assigns the next CustodyID if needed
     */
    virtual void insert(Bundle* bundle);

    virtual void insert(const BundleRef& bref) {
        insert(bref.object());
    }

    /**
     * Remove the given bundle from the list. Returns true if the
     * bundle was successfully removed, false otherwise.
     *
     * Unlike the pop() functions, this does remove the list's
     * reference on the bundle.
     */
    virtual bool erase(Bundle* bundle);
    virtual bool erase(std::string& dest, bundleid_t custody_id);

    /**
     * Retrieve a bundle from the list
     */
    virtual BundleRef find(std::string& dest, bundleid_t custody_id);

    /**
     * Clear out all of the lists
     */
    virtual void clear();

    /**
     * Return the size of all of the lists
     */
    virtual uint64_t size();

    /**
     * Return whether or not all of the lists are empty.
     */
    virtual bool empty();

protected:
    
    List             list_;	///< List of lists
    
    oasys::SpinLock lock_;	///< lock for coordinated access

};

}

#endif /* _CUSTODY_LIST_H_ */
