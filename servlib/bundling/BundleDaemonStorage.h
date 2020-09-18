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

#ifndef _BUNDLE_DAEMON_STORAGE_H_
#define _BUNDLE_DAEMON_STORAGE_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <map>

#include <oasys/compat/inttypes.h>
#include <oasys/debug/Log.h>
#include <oasys/tclcmd/IdleTclExit.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/thread/Timer.h>
#include <oasys/thread/Thread.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/Time.h>

#include "AggregateCustodySignal.h"
#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "BundleEventHandler.h"
#include "BundleListIntMap.h"
#include "BundleProtocol.h"
#include "BundleActions.h"
#include "BundleStatusReport.h"
#include "naming/EndpointID.h"
#include "reg/APIRegistration.h"

namespace dtn {

/**
 * Class that handles the basic event / action mechanism. All events
 * are queued and then forwarded to the active router module. The
 * router then responds by calling various functions on the
 * BundleActions class that it is given, which in turn effect all the
 * operations.
 */
class BundleDaemonStorage : public oasys::Singleton<BundleDaemonStorage, false>,
                     public BundleEventHandler,
                     public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    BundleDaemonStorage();

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~BundleDaemonStorage();

    /**
     * Virtual initialization function, overridden in the simulator to
     * install the modified event queue (with no notifier) and the
     * SimBundleActions class.
     */
    virtual void do_init();
    
    /**
     * Boot time initializer.
     */
    static void init();
    
    /**
     * Return the number of events currently waiting for processing.
     * This is overridden in the simulator since it doesn't use a
     * MsgQueue.
     */
    virtual size_t event_queue_size()
    {
    	return eventq_->size();
    }

    /**
     * Updates stats for a reloaded bundle
     */
    void reloaded_bundle()
    {
        ++stats_.bundles_reloaded_;
        ++stats_.bundles_in_db_;
    }

    /**
     * Updates stats for a reloaded registration
     */
    void reloaded_registration()
    {
        ++stats_.regs_reloaded_;
        ++stats_.regs_in_db_;
    }

    /**
     * Updates stats for a reloaded link
     */
    void reloaded_link()
    {
        ++stats_.links_reloaded_;
        ++stats_.links_in_db_;
    }

    /**
     * Updates stats for a reloaded Pending ACS
     */
    void reloaded_pendingacs()
    {
        ++stats_.pacs_reloaded_;
        ++stats_.pacs_in_db_;
    }

    /**
     * Queues the event at the tail of the queue for processing by the
     * daemon thread.
     */
    static void post(BundleEvent* event);
 
    /**
     * Queues the event at the head of the queue for processing by the
     * daemon thread.
     */
    static void post_at_head(BundleEvent* event);
    
    /**
     * Post the given event and wait for it to be processed by the
     * daemon thread or for the given timeout to elapse.
     */
    static bool post_and_wait(BundleEvent* event,
                              oasys::Notifier* notifier,
                              int timeout = -1, bool at_back = true);
    
   /**
     * Virtual post_event function, overridden by the Node class in
     * the simulator to use a modified event queue.
     */
    virtual void post_event(BundleEvent* event, bool at_back = true);

    /**
     * Format the given StringBuffer with the current bundle
     * statistics.
     */
    void get_stats(oasys::StringBuffer* buf);

    /**
     * Format the given StringBuffer with the current internal
     * statistics value.
     */
    void get_daemon_stats(oasys::StringBuffer* buf);

    /**
     * Reset all internal stats.
     */
    void reset_stats();

    /**
     * Adds a bundle to the list of bundles to be added/updated in the data store
     */
    void bundle_add_update(const BundleRef& bref);
    void bundle_add_update(Bundle* bundle);

    /**
     * Adds a bundle to the list of bundles to be deleted from the data store
     */
    void bundle_delete(Bundle* bundle);

    /**
     * Adds a registration to the list of registrations to be added/updated the data store
     */
    void registration_add_update(APIRegistration* reg);

    /**
     * Empties the event queue and updates the datastores one final time 
     * while shutting down
     */
    void commit_all_updates();

    /**
     * General daemon parameters
     */
    struct Params {
        /// Default constructor
        Params();
        
        /// number of milliseconds between database updates
        u_int32_t db_storage_ms_interval_;
        
        /// number of milliseconds between database updates
        bool db_log_auto_removal_;
        
        /// allow database writing to be disabled
        bool db_storage_enabled_;
    };

    static Params params_;

    /**
     * Typedef for a shutdown procedure.
     */
    typedef void (*ShutdownProc) (void* args);
    
    /**
     * Set an application-specific shutdown handler.
     */
    void set_app_shutdown(ShutdownProc proc, void* data)
    {
        app_shutdown_proc_ = proc;
        app_shutdown_data_ = data;
    }

    /**
     * Main event handling function.
     */
    void handle_event(BundleEvent* event);
    void handle_event(BundleEvent* event, bool closeTransaction);

protected:
    friend class BundleActions;

    /**
     * Main thread function that dispatches events.
     */
    void run();

    /// @{
    /**
     * Event type specific handlers.
     */
    void handle_store_bundle_update(StoreBundleUpdateEvent* event);
    void handle_store_bundle_delete(StoreBundleDeleteEvent* event);
    void handle_store_registration_update(StoreRegistrationUpdateEvent* event);
    void handle_store_registration_delete(StoreRegistrationDeleteEvent* event);
    void handle_store_link_update(StoreLinkUpdateEvent* event);
    void handle_store_link_delete(StoreLinkDeleteEvent* event);

#ifdef ACS_ENABLED 
    void handle_store_pendingacs_update(StorePendingAcsUpdateEvent* event);
    void handle_store_pendingacs_delete(StorePendingAcsDeleteEvent* event);
#endif // ACS_ENABLED 

    ///@}

    /// @{
    void event_handlers_completed(BundleEvent* event);
    /// @}

    /// @{
    /**
     * Internal processing methods.
     */
    void update_database();
    void update_database_bundles();
    void update_database_registrations();
    void update_database_pendingacs();
    void update_database_links();
    ///@}

    /// The BundleDaemon instance
    BundleDaemon* daemon_;
 
    /// The list of all bundles in the system
    all_bundles_t* all_bundles_;

    /// Lock to control access to the bundle lists
//dz debug    oasys::SpinLock bundle_lock_;

    // typedefs for lists of bundles
    typedef std::map<bundleid_t, int64_t> DeleteBundleMap;
    typedef std::pair<bundleid_t, int64_t> DeleteBundleMapPair;
    typedef DeleteBundleMap::iterator DeleteBundleMapIterator;

    /// The list of bundles to be added or updated
//dz debug    BundleListIntMap* add_update_bundles_;
    DeleteBundleMap* add_update_bundles_;

    /// The list of bundles to be deleted
    DeleteBundleMap* delete_bundles_;

    // typedefs for lists of registrations
    typedef std::map<u_int32_t, APIRegistration*> RegistrationMap;
    typedef std::pair<u_int32_t, APIRegistration*> RegMapPair;
    typedef RegistrationMap::iterator RegMapIterator;
    typedef std::pair<RegMapIterator, bool> RegMapInsertResult;

    /// Lock to control access to the registration lists
    oasys::SpinLock registration_lock_;

    /// The list of Registrations to be updated
    RegistrationMap* add_update_registrations_;

    /// The list of Registrations to be deleted
    RegistrationMap* delete_registrations_;

    /// Lock to control access to the Pending ACS lists
    oasys::SpinLock pendingacs_lock_;

    // typedefs for lists of links
    typedef std::map<std::string, Link*> LinkMap;
    typedef std::pair<std::string, Link*> LinkMapPair;
    typedef LinkMap::iterator LinkMapIterator;
    typedef std::pair<LinkMapIterator, bool> LinkMapInsertResult;

    /// Lock to control access to the registration lists
    oasys::SpinLock link_lock_;

    /// The list of Registrations to be updated
    LinkMap* add_update_links_;

    /// The list of Registrations to be deleted
    LinkMap* delete_links_;


#ifdef ACS_ENABLED 
    /// The list of pending ACSs to be updated
    PendingAcsMap* add_update_pendingacs_;

    /// The list of pending ACSs to be deleted
    PendingAcsMap* delete_pendingacs_;
#endif // ACS_ENABLED 


    /// The event queue
    oasys::MsgQueue<BundleEvent*>* eventq_;

    /// Statistics structure definition
    struct Stats {
        uint64_t events_processed_;
        uint64_t bundles_in_db_;
        uint64_t bundles_added_;
        uint64_t bundles_updated_;
        uint64_t bundles_combinedupdates_;
        uint64_t bundles_deleted_;          // # deletes performed
        uint64_t bundles_delupdates_;       // # updates deleted from the add_update_bundles_ list
        uint64_t bundles_delbeforeupdates_; // # updates not processed because deleting
        uint64_t bundles_delbeforeadd_;     // # adds not processed because deleting
        uint64_t bundles_deletesskipped_;   // # deletes not posted because not in datastore yet
        uint64_t bundles_reloaded_;

        uint64_t regs_in_db_;
        uint64_t regs_added_;
        uint64_t regs_updated_;
        uint64_t regs_combinedupdates_;
        uint64_t regs_deleted_;
        uint64_t regs_delupdates_;
        uint64_t regs_reloaded_;

        uint64_t links_in_db_;
        uint64_t links_added_;
        uint64_t links_updated_;
        uint64_t links_combinedupdates_;
        uint64_t links_deleted_;
        uint64_t links_delupdates_;
        uint64_t links_reloaded_;

        uint64_t pacs_in_db_;
        uint64_t pacs_added_;
        uint64_t pacs_updated_;
        uint64_t pacs_combinedupdates_;
        uint64_t pacs_deleted_;
        uint64_t pacs_delupdates_;
        uint64_t pacs_reloaded_;
    };

    /// Stats instance
    Stats stats_;

    /// Application-specific shutdown handler
    ShutdownProc app_shutdown_proc_;
 
    /// Application-specific shutdown data
    void* app_shutdown_data_;

    /// Flags to control an orderly shutdown and database update
    bool run_loop_terminated_;
    bool last_commit_completed_;

    /// Time value when the last event was handled
    oasys::Time last_event_;

    /// Time value when the last event was handled
    oasys::Time last_db_update_;

    /// Sync Payload Timer
    oasys::Time sync_payload_timer_;

};

} // namespace dtn

#endif /* _BUNDLE_DAEMON_STORAGE_H_ */
