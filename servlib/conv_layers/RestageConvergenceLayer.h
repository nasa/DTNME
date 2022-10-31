/*
 *    Copyright 2021 United States Government as represented by NASA
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

#ifndef _RESTAGE_CONVERGENCE_LAYER_H_
#define _RESTAGE_CONVERGENCE_LAYER_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if BARD_ENABLED


#include <third_party/oasys/thread/Thread.h>

#include "ConvergenceLayer.h"

#include "bundling/BundleDaemon.h"
#include "bundling/BARDRestageCLIF.h"


namespace dtn {

class RestageConvergenceLayer : public ConvergenceLayer {
public:
    /**
     * Constructor.
     */
    RestageConvergenceLayer();
        
    /**
     * List valid options for an option
     * @param buf
     */
    void list_interface_opts(oasys::StringBuffer& buf) override;

    /**
     * Bring up a new interface.
     * @param argc
     * @param argv
     */
    bool interface_up(Interface* iface, int argc, const char* argv[]) override;

    /**
     * Activate an interface. Start it receiving 
     * @param iface
     */
    void interface_activate(Interface* iface) override;

    /**
     * Bring down the interface.
     * @param iface
     */
    bool interface_down(Interface* iface) override;
    
    /**
     * Dump out CL specific interface information.
     * @param iface
     * @param buf
     */
    void dump_interface(Interface* iface, oasys::StringBuffer* buf) override;

    /**
     * List valid options for a link
     * @param buf
     */
    void list_link_opts(oasys::StringBuffer& buf) override;

    /**
     * Create any CL-specific components of the Link.
     * @param link
     */
    bool init_link(const LinkRef& link, int argc, const char* argv[]) override;

    /**
     * Set default link options.
     * @param argc
     * @param argv
     * @param invalidp
     */
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp) override;

    /**
     * Delete any CL-specific components of the Link.
     * @param link
     */
    void delete_link(const LinkRef& link) override;

    /**
     * Dump out CL specific link information.
     * @param link
     * @param buf
     */
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf) override;
    
    /**
     * Open the connection to a given contact and send/listen for 
     * @param contact
     * bundles over this contact.
     */
    bool open_contact(const ContactRef& contact) override;
    
    /**
     * Close the connnection to the contact.
     * @param contact
     */
    bool close_contact(const ContactRef& contact) override;

    /**
     * Post-initialization, parse any CL-specific options for the link.
     */
    virtual bool reconfigure_link(const LinkRef& link,
                                  int argc, const char* argv[]) override;

    virtual void reconfigure_link(const LinkRef& link,
                                  AttributeVector& params) override;

    /**
     * Send the bundle out the link.
     * @param link
     * @param bundle
     */
    void bundle_queued(const LinkRef& link, const BundleRef& bundle) override;

    /**
     * List of email addresses for notifications
     */
    typedef std::map<std::string, bool> EmailList;
    typedef EmailList::iterator EmailListIter;
    typedef struct EmailNotifications {
        oasys::SpinLock lock_;
        std::string from_email_;
        EmailList email_list_;
    } EmailNotifications;
    typedef std::shared_ptr<EmailNotifications> SPtr_EmailNotifications;

    /**
     * Tunable parameter structure.
     *
     * Per-link and per-interface settings are configurable via
     * arguments to the 'link add' and 'interface add' commands.
     *
     * The parameters are stored in each Link's CLInfo slot, as well
     * as part of the Receiver helper class.
     */
    class Params : public CLInfo {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize( oasys::SerializeAction *a );

        std::string storage_path_;                  ///< Top level directory to use for external storage
        bool        mount_point_ = true;            ///< Whether to verify storage path is mounted
        uint64_t    days_retention_ = 7;            ///< Number of days to keep bundles stored
        bool        expire_bundles_ = false;        ///< Whether to delete bundles when they expire
        uint64_t    ttl_override_   = 0;            ///< Minimum number of seconds to set the time to live to when reloading
        uint64_t    auto_reload_interval_ = 3600;   ///< How often in seconds to try to auto reload bundles (0 = never)
        size_t      disk_quota_ = 0;                ///< Maximum amount of disk space to use for storage (0 = no limit other than disk space)
        bool        part_of_pool_ = true;           ///< Whether this instance is part of the BARD pool of storage locations or not
        bool        email_enabled_ = true;          ///< Whether to send email notifications for alerts

        size_t      min_disk_space_available_ = 100000000; ///< Minimum volume space needed to declare state ONLINE vs FULL
        size_t      min_quota_available_ = 1000000;        ///< Minimum quota available to declare state ONLINE after a FULL

        SPtr_EmailNotifications sptr_emails_;     ///< The list of emails to which notifications are to be sent


        std::string  field_separator_ = "_";      ///< Character to use for a field separator; default is an underscore
        std::string  eid_field_separator_ = "-";  ///< Character to use for an EID field separator; default is a dash
        bool         field_separator_set_ = false;     ///< whether the field spearator was specified
        bool         eid_field_separator_set_ = false; ///< whether the EID field spearator was specified
    };
    typedef std::shared_ptr<Params> SPtr_RestageCLParams;
    
    /**
     * Default parameters.
     */
    static Params defaults_;


protected:
    /**
     * Parse and validate the provided parameters
     * @param params
     * @param argc
     * @param argv
     * @param invalidp
     */
    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);


    /**
     * Parse options list for possibility of multiple email addresses
     */
    void parse_params_for_emails(Params* params, int argc, const char** argv);

    /**
     * Generate a new copy of the default parameters that may be overriden by a given instance.
     */
    virtual CLInfo* new_link_params() override;
    virtual SPtr_CLInfo new_link_params_sptr();



    // forward declaration so we can typedef
    class ExternalStorageController;
    typedef std::shared_ptr<ExternalStorageController> SPtr_ExternalStorageController;

    /*
     * Helper class that wraps the sender-side per-contact state.
     */
    class ExternalStorageController : public CLInfo,
                                      public BARDRestageCLIF,
                                      public oasys::Logger,
                                      public oasys::Thread {
    public:
        /**
         * Constructor.
         */
        ExternalStorageController(const ContactRef& contact, std::string link_name);
        
        /**
         * Destructor.
         */
        virtual ~ExternalStorageController();

        /**
         * Shutdown processing.
         */
        virtual void shutdown();

        /**
         * Initialize the internal shared pointer(s) that reference this object
         */
        void set_sptr(SPtr_ExternalStorageController sptr_esc);

        /**
         * Initialize the External Storage Controller
         */
        //dzdebug bool init(Params* params, const std::string& link_name);
        bool init(SPtr_RestageCLParams sptr_params, const std::string& link_name);

        /**
         * Reconfigure the Exernal Storage Controller
         */
        void reconfigure(SPtr_RestageCLParams sptr_params);

        /**
         * Dump out CL specific link information.
         * @param link
         * @param buf
         */
        void dump_link(oasys::StringBuffer* buf);
    
       /**
        * Main processing loop for the Thread
        */
       void run() override; 


       // virtuals from BARDRestageCLIF
       /**
        * Initiate attempt to reload all types of restaged bundes
        * @prarm new_expiration  Adjust bundle expiration to provide a minimum time to live
        */
       virtual int reload_all(size_t new_expiration) override;

       /**
        * Initiate attempt to reload all types of restaged bundes
        * @prarm 
        * @prarm 
        * @prarm 
        * @prarm 
        * @prarm 
        * @prarm 
        * @prarm new_expiration  Adjust bundle expiration to provide a minimum time to live
        * @prarm 
        */
       virtual int reload(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme, std::string& nodename, 
                       size_t new_expiration, std::string& new_dest_eid) override;

       /**
        * Initiates an event to delete the specified bundles
        * @param quota_type   The quota type of the restaged bundles to be reloaded (by source EID or destination EID)
        * @param scheme       The naming scheme of the restaged bundles to be reloaded
        * @param nodename     The node name/number of the restaged bundles to be reloaded
        * @return Number of directories queued for deleting
        */
        virtual int delete_restaged_bundles(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme, 
                                            std::string& nodename) override;

       /**
        * Initiates events to delete all restaged bundles
        * @return Number of directories queued for deleting
        */
        virtual int delete_all_restaged_bundles() override;

       /**
        * Pause restaging and reloading in preparation to do a rescan.
        */
       virtual void pause_for_rescan() override;

       /**
        * Resume restaging and reloading after completing a rescan initiated by the BARD
        */
       virtual void resume_after_rescan() override;

       /**
        * Initiate a rescan of the external storage
        */
       virtual void rescan() override;


    protected:
        struct BundleFileDesc {
            std::string                 filename_;
            bard_quota_type_t            quota_type_ = BARD_QUOTA_TYPE_UNDEFINED;  ///< src/dst set from the directory name
            bard_quota_naming_schemes_t  src_eid_scheme_ = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
            std::string                 src_eid_nodename_;
            size_t                      src_eid_node_number_ = 0;
            std::string                 src_eid_service_;
            bard_quota_naming_schemes_t  dst_eid_scheme_ = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
            std::string                 dst_eid_nodename_;
            size_t                      dst_eid_node_number_ = 0;
            std::string                 dst_eid_service_;
            size_t                      bts_secs_or_millisecs_ = 0;  ///< creation time in seconds (BPv6) or millisecs (BPv7) since 01/01/2000
            size_t                      bts_seq_num_ = 0;
            bool                        is_frag_ = false;
            size_t                      frag_offset_ = 0;
            size_t                      frag_length_ = 0;
            size_t                      orig_payload_length_ = 0;
            size_t                      payload_length_ = 0;        ///< acual length of this bundle's payload (could be the frag_length_)
            size_t                      exp_seconds_ = 0;           ///< expiration time in seconds since 01/01/2000

            size_t                      file_size_ = 0;             ///< size of the file (serialized bundle)
            size_t                      disk_usage_ = 0;            ///< actaul storage usage
            time_t                      file_creation_time_ = 0;    ///< When the bundle was restaged

            size_t                      error_count_ = 0;           ///< number of errors trying to open/read the file
        };

        typedef std::shared_ptr<struct BundleFileDesc> SPtr_BundleFileDesc;

        // map to hold the files in a directory - key is the filename
        typedef std::map<std::string, SPtr_BundleFileDesc> BundleFileDescMap;
        typedef std::pair<std::string, SPtr_BundleFileDesc> BundleFileDescPair;
        typedef BundleFileDescMap::iterator  BundleFileDescMapIter;
        typedef std::shared_ptr<BundleFileDescMap> SPtr_BundleFileDescMap;

        // map to hold a list of BundleFileDescMap objects - key is directory name
        typedef std::map<std::string, SPtr_BundleFileDescMap> RestageDirMap;
        typedef RestageDirMap::iterator RestageDirMapIter;
        

        struct RestageDirStatistics {
            std::string                 dirname_;
            bard_quota_type_t            quota_type_ = BARD_QUOTA_TYPE_UNDEFINED;  // src/dst set from the directory name
            bard_quota_naming_schemes_t  scheme_ = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
            std::string                 nodename_;
            size_t                      node_number_ = 0;

            size_t                      num_files_ = 0;
            size_t                      total_size_ = 0;
        };

        typedef std::shared_ptr<struct RestageDirStatistics> SPtr_RestageDirStatistics;

        // map to hold the statistics by directory - key is the directroy name
        typedef std::map<std::string, SPtr_RestageDirStatistics> RestageDirStatisticsMap;
        typedef RestageDirStatisticsMap::iterator  RestageDirStatisticsMapIter;



        // Polls the Link queue for bundles and processes them to external storage
        class Restager: public Logger,
                        public oasys::Thread
        {
        public:
            Restager(SPtr_ExternalStorageController sptr_esc, const std::string& link_name, 
                     const ContactRef& contact, SPtr_RestageCLParams sptr_params);
            virtual ~Restager();
            void shutdown();
            void pause() { paused_ = true; }
            void resume() { paused_ = false; }
            void run();
        protected:
            bool check_for_bundle();
            ssize_t process_bundle(BundleRef& bref);
        protected:
            bool paused_ = false;

            SPtr_ExternalStorageController sptr_esc_;
            ContactRef contact_;
            LinkRef link_;

            SPtr_RestageCLParams sptr_params_;          ///< Pointer to the link parameters for an instance of a Restage CL

            oasys::FileIOClient file_;             ///< file handle
        };


        // Helper thread to reload bundles from external storage
        class Reloader: public Logger,
                        public oasys::Thread
        {
        public:
            Reloader (const std::string& link_name,
                      SPtr_ExternalStorageController sptr_esc, 
                      SPtr_BundleArchitecturalRestagingDaemon sptr_bard);
            virtual ~Reloader();
            void shutdown();
            void pause() { paused_ = true; }
            void resume() { paused_ = false; }
            void run();
            void post(const std::string& dirname, SPtr_BundleFileDescMap sptr_bfd_map, const std::string& dir_path, 
                      size_t new_expiration, const std::string&  new_dest_eid);
        protected:
            struct ReloadEvent {
                std::string dirname_;
                SPtr_BundleFileDescMap sptr_bfd_map_;
                std::string dir_path_;
                ssize_t new_expiration_; 
                std::string  new_dest_eid_;
            };

        protected:
            bool query_okay_to_reload(const BundleFileDesc* bfd_ptr);

            void process_reload(ReloadEvent* event);
            void process_delete_bundles(ReloadEvent* event);

            Bundle* extract_bundle_from_file(const std::string& fullpath, SPtr_BundleFileDesc sptr_bfd);
        protected:
            bool paused_ = false;

            std::string link_name_;
            SPtr_ExternalStorageController sptr_esc_;
            SPtr_BundleArchitecturalRestagingDaemon sptr_bard_;

            /// The event queue
            oasys::MsgQueue<ReloadEvent*> eventq_;

            oasys::FileIOClient file_;             ///< file handle

            char buffer_[2 * 1024 * 1024];
        };

        // Ephemeral worker thread kicked off to send email messages
        class Emailer: public Logger,
                       public oasys::Thread
        {
        public:
            Emailer (restage_cl_states_t new_state, SPtr_EmailNotifications sptr_emails);
            virtual ~Emailer();
            void run();
        protected:
            restage_cl_states_t new_state_;
            SPtr_EmailNotifications sptr_emails_;
        };

    protected:
        void initialize_separator_constants();

        void pause_all_activity();
        void resume_all_activity();
        void update_bard_usage_stats();

        /**
         * Validate if the storage path is on a mounted drive.
         */
        bool validate_mount_point(std::string& storage_path, std::string& validated_mount_pt);

        /**
         * Validate the current state of the external storage
         */
        void validate_external_storage();

        /**
         * Get the volume size and available bytes
         */
        bool update_volume_stats();

        bool create_storage_path(std::string& path);
        bool create_storage_subdirectory(std::string& dirname, std::string& full_path);

        std::string format_bundle_dir_name(Bundle* bundle);
        std::string format_bundle_dir_name_by_src(Bundle* bundle);
        std::string format_bundle_dir_name_by_dst(Bundle* bundle);
        std::string format_bundle_dir_name(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme, 
                                           std::string& nodename);

        std::string format_bundle_filename(Bundle* bundle);

        void eid_to_tokens(const EndpointID& eid, std::string& scheme, std::string& nodename, std::string& service);
        void bts_to_tokens(Bundle* bundle, std::string& human_readable, std::string& dtn_time, std::string& seq_num);
        void frag_to_tokens(Bundle* bundle, std::string& frag_offset, std::string& frag_len);
        void payload_to_tokens(Bundle* bundle, std::string& payload_len);
        void exp_to_tokens(Bundle* bundle, std::string& human_readable, std::string& dtn_time);


        bool parse_dirname(std::string dirname, bard_quota_type_t& quota_type,
                              bard_quota_naming_schemes_t& scheme,
                              std::string& nodename, size_t& node_number);
        bool parse_dname_quota_type(std::string& dirname, bard_quota_type_t& quota_type);
        bool parse_dname_node(std::string& dirname, bard_quota_naming_schemes_t& scheme,
                                 std::string& nodename, size_t& node_number);


        bool parse_filename(std::string filename, SPtr_BundleFileDesc& sptr_bfd);
        bool parse_fname_src_eid(std::string& filename, BundleFileDesc* bfd_ptr);
        bool parse_fname_dst_eid(std::string& filename, BundleFileDesc* bfd_ptr);
        bool parse_fname_eid(std::string& filename, bard_quota_naming_schemes_t& scheme,
                             std::string& nodename, size_t& node_number, std::string service);
        bool parse_fname_bts(std::string& filename, BundleFileDesc* bfd_ptr);
        bool parse_fname_frag(std::string& filename, BundleFileDesc* bfd_ptr);
        bool parse_fname_payload(std::string& filename, BundleFileDesc* bfd_ptr);
        bool parse_fname_exp(std::string& filename, BundleFileDesc* bfd_ptr);

        bool scan_external_storage();
        void scan_restage_subdirectory(std::string& dirname, bard_quota_type_t quota_type);
        void scan_restage_subdirectory(std::string& dirname, std::string& dirpath, bard_quota_type_t quota_type);

        void add_to_statistics(const std::string& dirname, const BundleFileDesc* bfd_ptr);
        void del_from_statistics(const std::string& dirname, const BundleFileDesc* bfd_ptr);

        void dump_statistics(oasys::StringBuffer* buf);
        void dump_file_list(oasys::StringBuffer* buf);
        const char* quota_type_to_cstr(bard_quota_type_t quota_type);
        const char* naming_scheme_to_cstr(bard_quota_naming_schemes_t scheme);

        void load_bfd_bundle_data(BundleFileDesc* bfd_ptr, Bundle* bundle);
        void load_bfd_eid_src_data(BundleFileDesc* bfd_ptr, const EndpointID& eid);
        void load_bfd_eid_dst_data(BundleFileDesc* bfd_ptr, const EndpointID& eid);

        void update_restage_cl_state(restage_cl_states_t new_state);
        void send_email_notifications(restage_cl_states_t new_state);

        void do_garbage_collection();
        size_t garbage_collect_dir(const std::string& dirname, BundleFileDescMap* bfd_map_ptr, 
                                   std::string dir_path);

        void rescan_external_storage();
    protected:
        // methods used by the Restager
        friend class Restager;
        void bundle_restaged(const std::string& dirname, const std::string& filename, Bundle* bundle, 
                             size_t file_size, size_t disk_usage);
        void bundle_restage_error();
        bool disk_quota_is_available(ssize_t space_needed);

        void set_cl_state(restage_cl_states_t new_state);

    protected:
        // methods used by the Reloader
        friend class Reloader;

        void bundle_reloaded(const std::string& dirname, BundleFileDescMap* bfd_map_ptr,
                             const BundleFileDesc* bfd_ptr);

        void bundle_deleted(const std::string& dirname, BundleFileDescMap* bfd_map_ptr,
                            const BundleFileDesc* bfd_ptr);

    protected:
        oasys::SpinLock lock_;                      ///< Lock to serialize access

        SPtr_RestageCLParams sptr_params_;          ///< Pointer to the link parameters for an instance of a Restage CL

        ContactRef contact_;                        ///< The contact reference that system uses to reference this instance of the Restage CL

        SPtr_RestageCLStatus sptr_clstatus_;        ///< Shared pointer to the current state and status of the storage location (shared with the BARD)

        SPtr_ExternalStorageController sptr_esc_self_; ///< shared pointer to self for sharing with helper thread(s)

        SPtr_BundleArchitecturalRestagingDaemon sptr_bard_;      ///< Shared pointer to the BundleArchitecturalRestagingDaemon

        std::unique_ptr<Restager> qptr_restager_;   ///< Pointer to the Restager object
        std::unique_ptr<Reloader> qptr_reloader_;   ///< Pointer to the Reloader object

        SPtr_EmailNotifications emails_;            ///< List of email address to notify on errors

        RestageDirMap restage_dir_map_;             ///< map of Restage Directories which contain a list of file/bundle descriptions 

        bool registered_with_bard_ = false;         ///< whether the CL instance has registered with the Bundle Restaging Dameon

        bool external_storage_scanned_ = false;     ///< whether the external storage has been successfully scanned

        oasys::FileIOClient file_;                  ///< file handle

        oasys::Time auto_reload_timer_;             ///< Tracks time until next reload attempt

        // track total disk usage and also by directory
        size_t grand_total_num_files_ = 0;
        size_t grand_total_num_bytes_ = 0;
        RestageDirStatisticsMap dir_stats_map_;

        size_t total_restaged_ = 0;                  ///< Count of bundles restaged
        size_t total_restage_dupes_ignored_ = 0;     ///< Count of attemptes ro restage a duplicate bundle
        size_t total_restage_errors_ = 0;            ///< Count of errors encountered while restaging bundles
        size_t total_reloaded_ = 0;                  ///< Count of bundles reloaded
        size_t total_deleted_ = 0;                   ///< Count of bundles deleted

        bool perform_rescan_ = false;                ///< Whether the BARD initiated a rescan
    };


};


} // namespace dtn

#endif // BARD_ENABLED


#endif /* _RESTAGE_CONVERGENCE_LAYER_H_ */
