/*
 *    Copyright 2022 United States Government as represented by NASA
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

#ifndef _IMC_CONFIG_H_
#define _IMC_CONFIG_H_

#include <mutex>

#include "third_party/oasys/util/StringBuffer.h"

#include "IMCRegionGroupRec.h"

#include "bundling/CborUtilBP7.h"

namespace dtn {

class Bundle;

typedef std::map<size_t, size_t> IMC_GROUP_JOINS_MAP;   /// key = IMC Group number

#define IMC_GROUP_0_SYNC_REQ 0


/**
 * General daemon parameters
 */
struct IMCConfig {
public:
    /// Default constructor
    IMCConfig();
    
    /// Map of IMCRegionMaps to track all nodes in a given region
    MAP_OF_IMC_REGION_MAPS map_of_region_maps_;

    /// Map of IMCGroupMaps to track all nodes wanting a copy of 
    /// bundles with a given IMC Group [destination]
    MAP_OF_IMC_GROUP_MAPS map_of_group_maps_;

    /// Map of groups for tracking outer region node petitions???
    /// -- if receive a join petiition from outer region then propogate join for self to home region
    MAP_OF_IMC_GROUP_MAPS map_of_group_maps_for_outer_regions_;

    /// Map of IMCGroupMaps to track all passageway nodes wanting a copy of 
    /// bundles with a given IMC Group [destination]
    MAP_OF_IMC_GROUP_MAPS map_of_passageway_group_maps_;

    /// Map of the manual join recs in the database;
    /// key is the corresponding IMC EID string
    IMCRecsMap manual_joins_map_;

    /// Get the node's home region number
    size_t home_region();

    /// Get IMC Router Node designation
    bool imc_router_node_designation();

    /// Whether the IMC Router is enabled
    bool imc_router_enabled_ = false;

    /// Whether the IMC Router is currently active/running
    bool imc_router_active_ = false;

    /// Whether startup config issued clear_imc_region_db command
    bool clear_imc_region_db_on_startup_ = false;

    /// The ID# specified in a startup config clear_imc_region_db command
    size_t clear_imc_region_db_id_ = 0;

    /// Whether startup config issued clear_imc_group_db command
    bool clear_imc_group_db_on_startup_ = false;

    /// The ID# specified in a startup config clear_imc_group_db command
    size_t clear_imc_group_db_id_ = 0;

    /// Whether startup config issued clear_imc_manual_join_db command
    bool clear_imc_manual_join_db_on_startup_ = false;

    /// The ID# specified in a startup config clear_imc_manual_join_db command
    size_t clear_imc_manual_join_db_id_ = 0;

    /// Whether to request IMC Group Petitions from all region nodes on start up (BPv7)
    bool imc_sync_on_startup_ = false;

    /// Flag to indicate a sync was manually initiated
    bool imc_do_manual_sync_ = false;

    /// Node number to manually sync with or zero to sync with all nodes
    size_t imc_manual_sync_node_num_ = 0;

    /// Whether to issue BPv6 IMC group join/unjoin bundles
    bool imc_issue_bp6_joins_ = false;

    /// Whether to issue BPv7 IMC group join/unjoin bundles
    bool imc_issue_bp7_joins_ = true;

    /// Whether to delete bundles if all dest nodes are currently unrouteable
    /// as opposed to holding them in hopes of an opportunistic connection
    bool delete_unrouteable_bundles_ = true;

    /// Whether to treat opportunistic but currently unreachable nodes as unrouteable
    bool treat_unreachable_nodes_as_unrouteable_ = true;

    /// Mapping of group numbers with how many local registrations have joined it
    IMC_GROUP_JOINS_MAP local_imc_group_joins_;   /// key = IMC Group number

protected:
    /// The node's home region number
    size_t home_region_ = 0;

    /// Whether the IMC Router is enabled
    bool is_imc_router_node_ = true;

    /// Record structure for maintinaing the Home Region in the database
    SPtr_IMCRegionGroupRec sptr_home_region_rec_;

    /// Record structure for maintinaing the Clear Region DB ID in the database
    SPtr_IMCRegionGroupRec sptr_clear_region_db_rec_;

    /// Record structure for maintinaing the Clear Group DB ID in the database
    SPtr_IMCRegionGroupRec sptr_clear_group_db_rec_;

    /// Record structure for maintinaing the Clear Hold_Group_Service DB ID in the database
    SPtr_IMCRegionGroupRec sptr_clear_manual_join_db_rec_;
     
    /// Mutex to serialize access
    std::recursive_mutex lock_;


public:
    /// Set the IMC Home Region for this node
    void set_home_region(size_t region_num, oasys::StringBuffer& buf, bool db_update=true);

    /// Determine whether node is in home region
    bool is_node_in_home_region(size_t node_num);

    /// Determine whether node is in specified region
    bool is_node_in_region(size_t node_num, size_t region_num);

    /// Set IMC Router Node designation
    void set_imc_router_node_designation(bool designation, bool db_update=true);

    /// Clear all IMC Region info stored in the database if id_num does not match the last
    /// recorded id_num or always if zero. This allows a startup config to initially clear 
    /// the database and load a default set of definition that can be overridden by manual
    /// updates or via AMP that do not get cleared if the node is recycled without changing
    /// the id_num specified in the config file.
    void clear_imc_region_db(size_t id_num, std::string& result_str);

    /// Clear all IMC Group info stored in the database if id_num does not match the last
    /// recorded id_num or always if zero. This allows a startup config to initially clear 
    /// the database and load a default set of definition that can be overridden by manual
    /// updates or via AMP that do not get cleared if the node is recycled without changing
    /// the id_num specified in the config file.
    void clear_imc_group_db(size_t id_num, std::string& result_str);

    /// Clear all IMC Manual Join info stored in the database if id_num does not match the last
    /// recorded id_num or always if zero. This allows a startup config to initially clear 
    /// the database and load a default set of definition that can be overridden by manual
    /// updates or via AMP that do not get cleared if the node is recycled without changing
    /// the id_num specified in the config file.
    void clear_imc_manual_join_db(size_t id_num, std::string& result_str);

    /// Record that all nodes in the specified range are members of the specified region number
    void add_node_range_to_region(size_t region_num, bool is_router_node, size_t node_num_start, 
                                  size_t node_num_end, std::string& result_str, bool db_update=true);

    /// Remove all nodes in the specified range from the specified region number
    void del_node_range_from_region(size_t region_num, size_t node_num_start, 
                                    size_t node_num_end, std::string& result_str, bool db_update=true);

    /// Record that all nodes in the specified range are members of the specified group number
    void add_node_range_to_group(size_t group, size_t node_num_start, 
                                 size_t node_num_end, std::string& result_str, bool db_update=true);

    /// Remove all nodes in the specified range from the specified group number for outer regions
    void del_node_range_from_group(size_t group, size_t node_num_start, 
                                   size_t node_num_end, std::string& result_str, bool db_update=true);

    /// Record that all nodes in the specified range are members of the specified group number
    void add_node_range_to_group_for_outer_regions(size_t group, size_t node_num_start, 
                                                   size_t node_num_end, std::string& result_str);

    /// Remove all nodes in the specified range from the specified group number for outer regions
    void del_node_range_from_group_for_outer_regions(size_t group, size_t node_num_start, 
                                                     size_t node_num_end, std::string& result_str);

    /// List all nodes in specified region number or all nodes if zero specified
    void imc_region_dump(size_t region_num, oasys::StringBuffer& buf);

    /// List all nodes in specified group number or all nodes if zero specified
    void imc_group_dump(size_t group_num, oasys::StringBuffer& buf);

    /// Load and apply updates from the datastore
    void load_imc_config_overrides();

    /// Issue IMC petition messages for the manual joins from the startup config or database
    void issue_manual_joins_after_router_active();

    /// Update the IMC destination nodes of a bundle if the home region has not been processed
    void update_bundle_imc_dest_nodes(Bundle* bundle);

    /// Update the IMC destination nodes of a bundle with all nodes in the home region
    void update_bundle_imc_dest_nodes_with_all_nodes_in_home_region(Bundle* bundle);

    /// Update the IMC destination nodes of a bundle with all known IMC router nodes
    void update_bundle_imc_dest_nodes_with_all_router_nodes(Bundle* bundle);

    /// Update the IMC alternate destination nodes of a bundle with all IMC router nodes in the home region
    void update_bundle_imc_alternate_dest_nodes_with_router_nodes_in_home_region(Bundle* bundle);

    /**
     * Parse and apply the payload of a BPv6 IMC Group Petition Bundle
     * (dest EID = ipn:x.0 with admin type 5)
     */
    virtual bool process_bpv6_imc_group_petition(Bundle* bundle,
                                                 const u_char* buf_ptr, size_t buf_len);

    /**
     * Parse and apply the payload of an IMC Group Petition Bundle
     * (dest EID = imc:0.0)
     */
    virtual bool process_imc_group_petition(Bundle* bundle);

    /**
     * Increment the count of local registrations listening for a specified IMC Group
     */
    virtual void add_to_local_group_joins_count(size_t imc_group);

    /**
     * Decrement from the count of registrations listening for a specified IMC Group
     * Returns the number of registrations still joined to the group
     */
    virtual size_t subtract_from_local_group_joins_count(size_t imc_group);

    /**
     * Process ION compatible Administrative IMC Briefing bundle
     */
    virtual bool process_ion_compatible_imc_briefing(Bundle* bundle, 
                                                     CborValue& cvPayloadElements,
                                                     CborUtilBP7& cborutil);

    /**
     * Process DTNME compatible Administrative IMC Briefing bundle
     */
    virtual bool process_dtnme_imc_briefing(Bundle* bundle, 
                                            CborValue& cvPayloadElements,
                                            CborUtilBP7& cborutil);

    /**
     * Parse and apply the payload of an ION Contact Plan Sync bundle
     * (dest EID = imc:0.1)
     */
    virtual bool process_ion_contact_plan_sync(Bundle* bundle);

    /**
     * Generate join IMC Group Petitions
     */
    virtual void issue_startup_imc_group_petitions();

    /**
     * Create and send a BPv7 IMC join request (to imc:0.0)
     */
    virtual void issue_bp7_imc_join_petition(size_t imc_group);

    /**
     * Create and send a BPv7 IMC unjoin request (to imc:0.0)
     */
    virtual void issue_bp7_imc_unjoin_petition(size_t imc_group);

    /**
     * Create an IMC Group Join petition bundle
     * (dest EID = imc:0.0)
     */
    virtual Bundle* create_bp7_imc_join_petition(size_t imc_group);

    /**
     * Create and send BPv6 IMC join requests (to each node in region)
     */
    virtual void issue_bp6_imc_join_petitions(size_t imc_group);

    /**
     * Create and send BPv6 IMC unjoin requests (to each node in region)
     */
    virtual void issue_bp6_imc_unjoin_petitions(size_t imc_group);

    /// Add a manual_join EID/record
    void add_manual_join(size_t group_num, size_t service_num,
                         std::string& result_str, bool db_update=true);

    /// Delete a manual_join EID/record
    void del_manual_join(size_t group_num, size_t service_num,
                         std::string& result_str, bool db_update=true);

    /// WHether or not the specified IMC EID has a manual join
    /// to determine whether or not to keep the bundle until it can be delivered
    bool is_manually_joined_eid(SPtr_EID dest_eid);

    /// List all IMC EIDs to hold for delivery
    void imc_manual_join_dump(oasys::StringBuffer& buf);

protected:
    /// Format the list of nodes in a specific region
    void dump_single_imc_region(size_t region_num, SPtr_IMC_REGION_MAP& sptr_region_map, 
                                oasys::StringBuffer& buf);

    /// Format the list of nodes in a specific group
    void dump_single_imc_group(size_t group_num, SPtr_IMC_GROUP_MAP& sptr_group_map, 
                               oasys::StringBuffer& buf);

    /// Format the list of outer regions nodes in a specific group
    void dump_single_imc_group_for_outer_regions(size_t group_num, SPtr_IMC_GROUP_MAP& 
                                                 sptr_group_map, oasys::StringBuffer& buf);

    /// Add or update record in datastore
    void add_or_update_in_datastore(SPtr_IMCRegionGroupRec& sptr_rec);

    /// Delete record from datastore
    void delete_from_datastore(std::string key_str);

    /// Apply an override for a region record
    void apply_override_region_rec(SPtr_IMC_REGION_MAP& sptr_region_map, SPtr_IMCRegionGroupRec& sptr_rec);

    /// Apply an override for a group record
    void apply_override_group_rec(SPtr_IMC_GROUP_MAP& sptr_group_map, SPtr_IMCRegionGroupRec& sptr_rec);

    /// Apply an override for a manual join record
    void apply_override_manual_join_rec(SPtr_IMCRegionGroupRec& sptr_rec);

    /// Determine if a clear region db command was given in the startup script and apply it if applicable
    void check_for_clear_region_db_at_startup();

    /// Determine if a clear group db command was given in the startup script and apply it if applicable
    void check_for_clear_group_db_at_startup();

    /// Determine if a clear manual_join db command was given in the startup script and apply it if applicable
    void check_for_clear_manual_join_db_at_startup();

    /// Get the region map for the specifed region_num creating it if necesary
    SPtr_IMC_REGION_MAP get_region_map(size_t region_num);

    /// Get the region map for the specifed region_num creating it if necesary
    SPtr_IMC_GROUP_MAP get_group_map(size_t group_num);

    /// Get the region map for the specifed region_num creating it if necesary
    SPtr_IMC_GROUP_MAP get_group_map_for_outer_regions(size_t group_num);

    /// Delete all of the region override records from the datastore
    void delete_all_region_recs_from_datastore();

    /// Delete all of the group override records from the datastore
    void delete_all_group_recs_from_datastore();

    /// Delete all of the manual_join override records from the datastore
    void delete_all_manual_join_recs_from_datastore();

    /// Delete all records that start with "key_match" from the datastore
    void delete_rec_type_from_datastore(const char* key_match);

    /// Scan througn all records in the given map setting the in_datastore_ flag to false
    void clear_in_datastore_flags(SPtr_IMC_REGION_MAP& sptr_region_map);

    /// Update the IMC destination nodes of a bundle with all known IMC router nodes in home region
    void update_bundle_imc_dest_nodes_with_all_router_nodes_in_home_region(Bundle* bundle);

    /// Update the IMC destination nodes of a bundle with all known IMC router nodes in outer regions
    void update_bundle_imc_dest_nodes_with_all_router_nodes_in_outer_regions(Bundle* bundle);


    /// Update the IMC destination nodes of a bundle with all known IMC router nodes in a single region
    void update_bundle_imc_dest_nodes_with_router_nodes_from_single_region(Bundle* bundle, 
                                                                           SPtr_IMC_REGION_MAP& sptr_region_map);

    /// Update the IMC destination nodes of a bundle with all nodes in a single region
    void update_bundle_imc_dest_nodes_with_all_nodes_from_single_region(Bundle* bundle, SPtr_IMC_REGION_MAP& sptr_region_map);

    /// Update the IMC alternate destination nodes of a bundle with all known IMC router nodes in a single region
    void update_bundle_imc_alternate_dest_nodes_with_router_nodes_from_single_region(Bundle* bundle, 
                                                                                     SPtr_IMC_REGION_MAP& sptr_region_map);

    /// Update the IMC destination nodes will all joined nodes in the home region
    void update_imc_dest_nodes_from_home_region_group_list(Bundle* bundle, size_t group_num);

    /// Update the IMC destination nodes will all joined nodes (possibly proxies) in the outer regions
    void update_imc_dest_nodes_from_outer_regions_group_list(Bundle* bundle, size_t group_num);

    /// Generate an ION compatible IMC Briefing administrative bundle
    void generate_bp7_ion_compatible_imc_briefing_bundle(Bundle* orig_bundle);

    /// CBOR encode the payload for an ION compatible IMC Briefing administrative bundle 
    int64_t encode_bp7_ion_compatible_imc_briefing(uint8_t* buf, int64_t buflen);

    /// Generate a DTNME IMC Briefing administrative bundle
    void generate_dtnme_imc_briefing_bundle(Bundle* orig_bundle);

    /// CBOR encode the payload for a DTNME IMC Briefing administrative bundle
    int64_t encode_dtnme_imc_briefing(uint8_t* buf, int64_t buflen);

    /// CBOR encode the region array for a DTNME IMC Briefing administrative bundle
    CborError encode_dtnme_imc_briefing_home_region_array(CborEncoder& rptArrayEncoder);

    /// CBOR encode the array iof groups arays for a DTNME IMC Briefing administrative bundle
    CborError encode_dtnme_imc_briefing_group_arrays(CborEncoder& rptArrayEncoder);

    /// CBOR decode the array of regions for a DTNME IMC Briefing Admin bundle
    bool decode_dtnme_imc_briefing_region_array(CborValue& cvRptArray, CborUtilBP7& cborutil,
                                                size_t remote_home_region, size_t remote_node_num);

    /// CBOR decode the array of group arrays for a DTNME IMC Briefing Admin bundle
    bool decode_dtnme_imc_briefing_array_of_groups(CborValue& cvRptArray, CborUtilBP7& cborutil, 
                                                   size_t remote_home_region, size_t remote_node_num);


    /**
     * Create an IMC Group UnJoin petition bundle
     * (dest EID = imc:0.0)
     */
    virtual Bundle* create_bp7_imc_unjoin_petition(size_t imc_group);

    /**
     * Create an IMC Group petition bundle
     * (dest EID = imc:0.0)
     */
    virtual Bundle* create_bp7_imc_group_petition_bundle(size_t imc_group, size_t join_or_unjoin);

    /**
     * Encode the Payload for an IMC Join/UnJoin bundle
     */
    virtual int64_t encode_imc_group_petition(uint8_t* buf, int64_t buflen, 
                                      size_t imc_group, size_t join_or_unjoin);

    /**
     * Whether or not any outer regions have IMC router nodes
     */
    bool have_outer_region_router_nodes();

    /**
     * Whether or not any any regions have IMC router nodes other than the local node
     */
    bool have_any_other_router_nodes(size_t skip_node_num);

    /**
     * Create and send a BPv6 IMC Group petitiion to all IMC routers in the provided region map
     */
    virtual void issue_bp6_imc_group_petitions_to_all_routers_in_region(SPtr_IMC_REGION_MAP& sptr_region_map,
                                                                  size_t imc_group, size_t join_or_unjoin);

    /**
     * Create and send a BPv6 IMC Group petition bundle to the administrative IPN EID for the specified node_num
     */
    virtual void issue_bp6_imc_group_petition_to_node(size_t node_num, size_t imc_group, size_t join_or_unjoin);
};


} // namespace dtn

#endif /* _IMC_CONFIG_H_ */
