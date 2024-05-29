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

#ifndef _BUNDLE_IMC_STATE_H_
#define _BUNDLE_IMC_STATE_H_

#include <memory>
#include <map>

#include <third_party/oasys/serialize/Serialize.h>
#include <third_party/oasys/util/StringBuffer.h>

namespace dtn {

class BundleIMCState;

typedef std::unique_ptr<BundleIMCState> QPtr_BundleIMCState;

/// list of [IPN] destination nodes to receive an IMC bundle
typedef std::map<size_t, bool> IMC_DESTINATIONS_MAP;
typedef std::pair<size_t, bool> IMC_DESTINATIONS_PAIR;
typedef std::shared_ptr<IMC_DESTINATIONS_MAP> SPtr_IMC_DESTINATIONS_MAP;

/// list of [IPN] destination nodes grouped by link
typedef std::map<std::string, SPtr_IMC_DESTINATIONS_MAP> IMC_DESTINATIONS_PER_LINK_MAP;
typedef std::pair<std::string, SPtr_IMC_DESTINATIONS_MAP> IMC_DESTINATIONS_PER_LINK_PAIR;
typedef std::shared_ptr<IMC_DESTINATIONS_PER_LINK_MAP> SPtr_IMC_DESTINATIONS_PER_LINK_MAP;

/// list of IMC Region numbers that have processed the current IMC bundle
typedef std::map<size_t, bool> IMC_PROCESSED_REGIONS_MAP;
typedef std::pair<size_t, bool> IMC_PROCESSED_REGIONS_PAIR;
typedef std::shared_ptr<IMC_PROCESSED_REGIONS_MAP> SPtr_IMC_PROCESSED_REGIONS_MAP;


/// list of nodes that have processed a proxy group petition
typedef std::map<size_t, bool> IMC_PROCESSED_BY_NODE_MAP;
typedef std::pair<size_t, bool> IMC_PROCESSED_BY_NODE_PAIR;
typedef std::shared_ptr<IMC_PROCESSED_BY_NODE_MAP> SPtr_IMC_PROCESSED_BY_NODE_MAP;


/**
 * The internal representation of the IMC State for a bundle.
 */
class BundleIMCState : public oasys::SerializableObject
{
public:
    /**
     * constructor
     */
    BundleIMCState();

    /**
     * Bundle destructor.
     */
    virtual ~BundleIMCState();

    /**
     * Virtual from SerializableObject
     */
    void serialize(oasys::SerializeAction* a) override;

    /// get the number of IMC destination nodes
    size_t num_imc_dest_nodes() const;

    /// add specified node number to both the original and the working list of IMC destinations
    void add_imc_orig_dest_node(size_t dest_node);

    /// add specified node number to the working list of IMC destinations
    void add_imc_dest_node(size_t dest_node);

    /// mark the specified node as handled (delivered or transmitted)
    void imc_dest_node_handled(size_t dest_node);

    /// get the list of [IPN] destination nodes to receive an IMC bundle
    SPtr_IMC_DESTINATIONS_MAP imc_dest_map() const;

    /// get the list of [IPN] destination nodes for a given link
    SPtr_IMC_DESTINATIONS_MAP imc_dest_map_for_link(std::string& linkname) const;

    /// get the list of [IPN] destination nodes for a given link
    std::string imc_link_name_by_index(size_t index_num);

    /// mark all nodes sent to the specified link as handled
    void imc_link_transmit_success(std::string linkname);

    /// remove the entry for this link to clear the slate for the
    /// next transmit attempt
    void imc_link_transmit_failure(std::string linkname);
  

    /// clear all of the IMC destinations by link lists
    void clear_imc_link_lists();

    SPtr_IMC_PROCESSED_REGIONS_MAP imc_processed_regions_map() const;

    /// Whether or not a specified region has been processed
    bool is_imc_region_processed(size_t region);

    /// Add region num to the list of processed regions
    void add_imc_region_processed(size_t region);

    size_t num_imc_processed_regions() const;

    /// add dest_node to the list of nodes rachable via the specified link
    void add_imc_dest_node_via_link(std::string linkname, size_t dest_node);

    /// copy all dest_nodes to the list of nodes rachable via the specified link
    void copy_all_unhandled_nodes_to_via_link(std::string linkname);

    /// get the number of IMC nodes that have been handled [delivered/transmitted]
    size_t num_imc_nodes_handled() const { return imc_dest_nodes_handled_; }

    /// get the number of IMC nodes that have not been handled [delivered/transmitted]
    size_t num_imc_nodes_not_handled() const { return imc_dest_nodes_not_handled_; }

    /// get whether or not the router has processed an IMC bundle
    bool router_processed_imc() const { return router_processed_imc_; }

    /// set flag indicaiting that the router has processed an IMC bundle
    /// mark this bundle as having been processed by the IMC router
    /// (or accounted for by a non-IMC router)
    void set_router_processed_imc() { router_processed_imc_ = true; }

    /// add specified node number to the list of alternate IMC router destinations
    void add_imc_alternate_dest_node(size_t dest_node);

    /// Clear the list of alternate IMC router destinations
    void clear_imc_alternate_dest_nodes();

    /// Clear the list of alternate IMC router destinations
    size_t imc_alternate_dest_nodes_count();

    /// CHeck for successful delivery to an alternate IMC router destination
    bool is_an_alternate_dest_node_handled();

    /// get the list of [IPN] alternate destination nodes to receive an IMC bundle
    SPtr_IMC_DESTINATIONS_MAP imc_alternate_dest_map() const;

    /// get the list of [IPN] orignal destination nodes
    SPtr_IMC_DESTINATIONS_MAP imc_orig_dest_map() const;

    /// get the list of unrouteable [IPN] destination nodes in outer regions
    SPtr_IMC_DESTINATIONS_MAP imc_unrouteable_dest_map() const;

    /// Clear the list of unrouteable destination nodes
    void clear_imc_unrouteable_dest_nodes();

    /// add specified node number to the list of unrouteable IMC router destinations
    void add_imc_unrouteable_dest_node(size_t dest_node, bool in_home_region);

    /// get the number of IMC nodes in the home region that are unrouteable
    size_t num_imc_home_region_unrouteable() const { return imc_home_region_unroutable_; }

    /// get the number of IMC nodes in the home region that are unrouteable
    size_t num_imc_outer_regions_unrouteable() const { return imc_outer_regions_unroutable_; }

    /// get the list of [IPN] nodes that have processed the proxy request
    SPtr_IMC_PROCESSED_BY_NODE_MAP imc_processed_by_nodes_map() const;

    /// copy the the list of [IPN] nodes from another bundle
    void copy_imc_processed_by_node_list(SPtr_IMC_PROCESSED_BY_NODE_MAP& sptr_other_list);

    /// Add node num to the list of nodes that have processed a proxy petition
    void add_imc_processed_by_node(size_t node_num);

    /// Whether or not a specified region has been processed
    bool imc_has_proxy_been_processed_by_node(size_t node_num);
    
    void set_imc_is_proxy_petition(bool t) { imc_is_proxy_petition_ = t; }
    bool imc_is_proxy_petition() const { return imc_is_proxy_petition_; }

    void set_imc_sync_request(bool t) { imc_sync_request_ = t; }
    bool imc_sync_request() const { return imc_sync_request_; }

    void set_imc_sync_reply(bool t) { imc_sync_reply_ = t; }
    bool imc_sync_reply() const { return imc_sync_reply_; }

    void set_imc_is_dtnme_node(bool t) { imc_is_dtnme_node_ = t; }
    bool imc_is_dtnme_node() const { return imc_is_dtnme_node_; }

    void set_imc_is_router_node(bool t) { imc_is_router_node_ = t; }
    bool imc_is_router_node() const { return imc_is_router_node_; }

    void set_imc_briefing(bool t) { imc_briefing_ = t; }
    bool imc_briefing() const { return imc_briefing_; }

    size_t imc_size_of_sdnv_dest_nodes_array() const;
    size_t imc_size_of_sdnv_processed_regions_array() const;

    ssize_t imc_sdnv_encode_dest_nodes_array(u_char* buf_ptr, size_t buf_len) const;
    ssize_t imc_sdnv_encode_processed_regions_array(u_char* buf_ptr, size_t buf_len) const;

    /// generate output of the orignal [as received] IMC Destinations list
    void format_verbose_imc_orig_dest_map(oasys::StringBuffer* buf);

    /// generate output of the working IMC Destinations list
    void format_verbose_imc_dest_map(oasys::StringBuffer* buf);

    /// generate output of the IMC Destinations sublists by Link
    void format_verbose_imc_dest_nodes_per_link(oasys::StringBuffer* buf);

    /// generate output of the IMC Processed Regions
    void format_verbose_imc_state(oasys::StringBuffer* buf);

protected:
    /// methods to serialize individual IMC lists
    void serialize_orig_dest_list(oasys::SerializeAction* a);
    void serialize_not_handled_dest_list(oasys::SerializeAction* a);
    void serialize_handled_dest_list(oasys::SerializeAction* a);
    void serialize_processed_regions_list(oasys::SerializeAction* a);
    void serialize_processed_by_nodes_list(oasys::SerializeAction* a);

    void serialize_unmarshal_orig_dest_list(oasys::SerializeAction* a);
    void serialize_unmarshal_not_handled_dest_list(oasys::SerializeAction* a);
    void serialize_unmarshal_handled_dest_list(oasys::SerializeAction* a);
    void serialize_unmarshal_processed_regions_list(oasys::SerializeAction* a);
    void serialize_unmarshal_processed_by_nodes_list(oasys::SerializeAction* a);

private:
    /// Flag indicating that the router has processed the IMC bundle to prevent early deletion after a local delivery
    bool router_processed_imc_ = false;

    bool imc_is_dtnme_node_ = false;            ///< Whether the source node is a DTNME [IMC compatible]node
    bool imc_is_router_node_ = false;           ///< WHether the source node is a DTNME IMC Router node
    bool imc_sync_request_ = false;             ///< Whether a DTNME IMC sync is requested
    bool imc_sync_reply_ = false;               ///< Whether this is a DTNME IMC sync reply
    bool imc_is_proxy_petition_ = false;        ///< Wether bundle is an IMC Proxy petition
    bool imc_briefing_ = false;                 ///< Whether this bundle is an IMC Briefing (to trigger inclusion
                                                ///< of an IMC Block format 2 on an IPN Admin bundle)

    // IMC Extension Blocks info
    #define IMC_DEST_NODE_NOT_HANDLED false
    #define IMC_DEST_NODE_HANDLED     true
    size_t imc_dest_nodes_handled_ = 0;                             ///< Number of nodes in the dest list that are handled [delivered/transmitted] (true)
    size_t imc_dest_nodes_not_handled_ = 0;                         ///< Number of nodes in the dest list that are not handled (false)
    SPtr_IMC_DESTINATIONS_MAP sptr_imc_dest_map_;                   ///< list of [IPN] destination nodes to receive an IMC bundle
                                                                    ///< false = to be delivered or transmitted; true= delivered or transmitted
    SPtr_IMC_DESTINATIONS_MAP sptr_imc_orig_dest_map_;              ///< list of [IPN] destination nodes to receive an IMC bundle when bundle received

    #define IMC_NODE_IN_OUTER_REGIONS false
    #define IMC_NODE_IN_HOME_REGION   true
    size_t imc_home_region_unroutable_ = 0;                         ///< Number of nodes in the home region that are unrouteable
    size_t imc_outer_regions_unroutable_ = 0;                       ///< Number of nodes in the outer/unknown regions that are unrouteable
    SPtr_IMC_DESTINATIONS_MAP sptr_imc_unrouteable_dest_map_;       ///< list of unrouteable [IPN] destination nodes (added to all transmissions)
                                                                    ///< false = outer/unknown region; true = home region

    SPtr_IMC_DESTINATIONS_PER_LINK_MAP sptr_imc_link_dest_map_;     ///< list of [IPN] destination nodes grouped by link
    SPtr_IMC_PROCESSED_REGIONS_MAP sptr_imc_processed_regions_map_; ///< list of region numbers that have processed this IMC bundle

    SPtr_IMC_DESTINATIONS_MAP sptr_imc_alternate_dest_map_;         ///< list of [IPN] possible IMC Router nodes to receive an IMC bundle

    SPtr_IMC_PROCESSED_BY_NODE_MAP sptr_imc_processed_by_nodes_map_;  ///< list of [IPN] nodes that have processed this proxy jouin bundle

};


} // namespace dtn

#endif /* _BUNDLE_IMC_STATE_H_ */
