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

#ifndef _CONTACT_PLANNER_H_
#define _CONTACT_PLANNER_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <sstream>
#include <fstream>

#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/thread/SpinLock.h>
#include "Contact.h"
#include "ContactManager.h"
#include "Link.h"

#include "bundling/BundleDaemon.h"
#include "naming/EndpointID.h"

namespace dtn {

/**
 * Class that handles the basic event / action mechanism. All events
 * are queued and then forwarded to the active router module. The
 * router then responds by calling various functions on the
 * BundleActions class that it is given, which in turn effect all the
 * operations.
 */
class ContactPlanner : public oasys::Singleton<ContactPlanner, false>,
                   public oasys::Thread
{
public:

    struct Contact_Entry {
      int              id_;
      SPtr_EID         sptr_eid_;
      std::string      link_name_;
      LinkRef          link_;
      oasys::Time      start_time_;
      u_int32_t        duration_;
    };

    struct Contact_Compare {
      inline bool operator()(const Contact_Entry& i1, const Contact_Entry& i2)
      {
        return (i1.id_ < i2.id_);
      }
    };

    /**
     * Constructor.
     */
    ContactPlanner();

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~ContactPlanner();

    /**
     * Virtual initialization function, possibly overridden by a 
     * future simulator
     */
    virtual void do_init();

    /**
     * Boot time initializer.
     */
    static void init();

    ContactManager*          manager_handle_;
    void set_manager_handle(ContactManager* handle){
      manager_handle_ = handle;
    }  
    
    /**
     * Access Methods
     */
    std::vector<dtn::ContactPlanner::Contact_Entry> get_plan(SPtr_EID& sptr_eid);
    void parse_plan(std::string filename);
    void reset_plan();
    void update_plan();
    void update_plan_orig();

    std::vector<Contact_Entry> get_contact(SPtr_EID& sptr_eid, std::string id_range);
    void add_contact(SPtr_EID& sptr_eid, std::string link_name, std::string start_time, std::string duration);
    void delete_contact(SPtr_EID& sptr_eid, std::string id_range);

    bool export_plan(SPtr_EID& sptr_eid, std::string filename);

    bool should_continue_;

protected:

    /**
     * Main thread function that dispatches events.
     */
    void run();

    std::vector<Contact_Entry>     contactList;
    std::vector<Contact_Entry>     contactPlan;
    
};

} // namespace dtn

#endif /* _CONTACT_PLANNER_H_ */
