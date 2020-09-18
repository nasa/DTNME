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

#ifndef _DTPC_PROFILE_TABLE_H_
#define _DTPC_PROFILE_TABLE_H_

#include <string>
#include <list>

#include <oasys/debug/DebugUtils.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>

#include "DtpcProfile.h"
#include "naming/EndpointID.h"


namespace dtn {

class DtpcProfile;

/**
 * Class for the DTPC Profile table.
 */
class DtpcProfileTable : public oasys::Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static DtpcProfileTable* instance() {
        if (instance_ == NULL) {
            instance_ = new DtpcProfileTable();
        }
        return instance_;
    }

    static void shutdown();

    /**
     * Constructor
     */
    DtpcProfileTable();

    /**
     * Destructor
     */
    virtual ~DtpcProfileTable();

    /**
     * Perform processing after storage has been initialized
     */
    virtual void storage_initialized();

    /**
     * Parse and set the parameters supplied for a transmission profile.
     */
    virtual bool parse_params(DtpcProfile* profile,
                              int argc, const char** argv,
                              const char** invalidp);
    /**
     * Add a new transmission profile to the table. Returns true if the profile
     * is successfully added, false if the profile specification is
     * invalid (or it already exists).
     */
    virtual bool add(oasys::StringBuffer *errmsg, const u_int32_t profile_id, 
                     int argc, const char* argv[]);
    
    /**
     * Add a ereloaded transmission profile to the table. Returns true if the profile
     * is successfully added, false if the profile specification is
     * invalid (or it already exists).
     */
    virtual bool add_reloaded_profile(DtpcProfile* profile);
    
    /**
     * Remove the specified profile
     */
    virtual bool del(const u_int32_t profile_id);
    
    /**
     * List the current profiles.
     */
    virtual void list(oasys::StringBuffer *buf);


    /**
     * Public method to determine if a profile is defined
     */
    virtual bool find(const u_int32_t profile_id);

    /**
     * Public method to get a profile object
     */
    virtual DtpcProfile* get(const u_int32_t profile_id);

protected:
    static DtpcProfileTable* instance_;

    /// Map of Profiles using the ID as the key
    DtpcProfileMap profile_list_;

    /// Lock to serialize access
    oasys::SpinLock lock_;

    /// Storage initialized flag
    bool storage_initialized_;


    /**
     * Internal method to find the location of the given profile ID
     */
    virtual bool find(const u_int32_t profile_id, DtpcProfileIterator* iter);

};

} // namespace dtn

#endif /* _DTPC_PROFILE_TABLE_H_ */
