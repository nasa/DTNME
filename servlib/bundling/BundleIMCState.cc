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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "BundleIMCState.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
BundleIMCState::BundleIMCState()
{
}

//----------------------------------------------------------------------
BundleIMCState::~BundleIMCState()
{
}

//----------------------------------------------------------------------
void
BundleIMCState::serialize(oasys::SerializeAction* a)
{
    // IMC state flags
    a->process("imc_is_dtnme_node", &imc_is_dtnme_node_);
    a->process("imc_is_router_node", &imc_is_router_node_);
    a->process("imc_sync_reply", &imc_sync_reply_);
    a->process("imc_sync_request", &imc_sync_request_);
    a->process("imc_is_proxy_petition", &imc_is_proxy_petition_);


    // handle the lists
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        serialize_unmarshal_orig_dest_list(a);
        serialize_unmarshal_not_handled_dest_list(a);
        serialize_unmarshal_handled_dest_list(a);
        serialize_unmarshal_processed_regions_list(a);
        serialize_unmarshal_processed_by_nodes_list(a);
    } else {
        serialize_orig_dest_list(a);
        serialize_not_handled_dest_list(a);
        serialize_handled_dest_list(a);
        serialize_processed_regions_list(a);
        serialize_processed_by_nodes_list(a);
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::serialize_orig_dest_list(oasys::SerializeAction* a)
{
    size_t orig_dest_list_size = 0;
    if (sptr_imc_orig_dest_map_) {
        orig_dest_list_size = sptr_imc_orig_dest_map_->size();
    }

    a->process("imc_num_orig_dest_list", &orig_dest_list_size);

    if (orig_dest_list_size > 0) {
        auto iter = sptr_imc_orig_dest_map_->begin();

        size_t tmp_node;
        while (iter != sptr_imc_orig_dest_map_->end()) {
            tmp_node = iter->first;
            a->process("imc_node_orig", &tmp_node);
            ++iter;
        }
    }
}


//----------------------------------------------------------------------
void
BundleIMCState::serialize_unmarshal_orig_dest_list(oasys::SerializeAction* a)
{
    // read in the list of original dest nodes
    size_t tmp_num_nodes = 0;
    a->process("imc_num_orig_dest_list", &tmp_num_nodes);

    size_t tmp_node;
    for (size_t ix=0; ix<tmp_num_nodes; ++ix) {
        a->process("imc_node_orig", &tmp_node);
        add_imc_orig_dest_node(tmp_node);
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::serialize_not_handled_dest_list(oasys::SerializeAction* a)
{
    // write out the list of not handled nodes
    a->process("imc_num_not_handled", &imc_dest_nodes_not_handled_);

    if (imc_dest_nodes_not_handled_ > 0) {
        auto iter = sptr_imc_dest_map_->begin();

        size_t tmp_node;
        size_t ctr = 0;
        while (iter != sptr_imc_dest_map_->end()) {
            if (iter->second == IMC_DEST_NODE_NOT_HANDLED) {
                tmp_node = iter->first;
                a->process("imc_node_nh", &tmp_node);
                ++ctr;
            }
            ++iter;
        }
        ASSERT(ctr == imc_dest_nodes_not_handled_);
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::serialize_unmarshal_not_handled_dest_list(oasys::SerializeAction* a)
{
    // read in the list of not handled nodes
    size_t tmp_num_nodes = 0;
    a->process("imc_num_not_handled", &tmp_num_nodes);

    size_t tmp_node;
    for (size_t ix=0; ix<tmp_num_nodes; ++ix) {
        a->process("imc_node_nh", &tmp_node);
        add_imc_dest_node(tmp_node);
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::serialize_handled_dest_list(oasys::SerializeAction* a)
{
    // write out the list of handled nodes
    a->process("imc_num_handled", &imc_dest_nodes_handled_);

    if (imc_dest_nodes_handled_ > 0) {
        auto iter = sptr_imc_dest_map_->begin();

        size_t tmp_node;
        size_t ctr = 0;
        while (iter != sptr_imc_dest_map_->end()) {
            if (iter->second == IMC_DEST_NODE_HANDLED) {
                tmp_node = iter->first;
                a->process("imc_node_h", &tmp_node);
                ++ctr;
            }
            ++iter;
        }
        ASSERT(ctr == imc_dest_nodes_handled_);
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::serialize_unmarshal_handled_dest_list(oasys::SerializeAction* a)
{
    // read in the list of handled nodes
    size_t tmp_num_nodes = 0;
    a->process("imc_num_handled", &tmp_num_nodes);

    size_t tmp_node = 0;
    for (size_t ix=0; ix<tmp_num_nodes; ++ix) {
        a->process("imc_node_h", &tmp_node);
        imc_dest_node_handled(tmp_node);
    }
}        

//----------------------------------------------------------------------
void
BundleIMCState::serialize_processed_regions_list(oasys::SerializeAction* a)
{
    // write out the list of processed regions
    size_t num_regions = 0;
    if (sptr_imc_processed_regions_map_) {
        num_regions = sptr_imc_processed_regions_map_->size();
    }
    a->process("imc_num_regions", &num_regions);

    if (num_regions > 0) {
        auto iter = sptr_imc_processed_regions_map_->begin();

        size_t tmp_region;
        while (iter != sptr_imc_processed_regions_map_->end()) {
            tmp_region = iter->first;
            a->process("imc_region", &tmp_region);
            ++iter;
        }
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::serialize_unmarshal_processed_regions_list(oasys::SerializeAction* a)
{
    // read in the list of processed regions
    size_t num_regions = 0;
    size_t tmp_region;
    a->process("imc_num_regions", &num_regions);

    for (size_t ix=0; ix<num_regions; ++ix) {
        a->process("imc_region", &tmp_region);
        add_imc_region_processed(tmp_region);
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::serialize_processed_by_nodes_list(oasys::SerializeAction* a)
{
    // write out the list of processed regions
    size_t num_proc_nodes = 0;
    if (sptr_imc_processed_by_nodes_map_) {
        num_proc_nodes = sptr_imc_processed_by_nodes_map_->size();
    }
    a->process("imc_num_proc_nodes", &num_proc_nodes );

    if (num_proc_nodes > 0) {
        auto iter = sptr_imc_processed_by_nodes_map_->begin();

        size_t tmp_node;
        while (iter != sptr_imc_processed_by_nodes_map_->end()) {
            tmp_node = iter->first;
            a->process("imc_proc_node", &tmp_node);
            ++iter;
        }
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::serialize_unmarshal_processed_by_nodes_list(oasys::SerializeAction* a)
{
    // read in the list of processed regions
    size_t num_regions = 0;
    a->process("imc_num_proc_nodes", &num_regions);

    size_t tmp_node;
    for (size_t ix=0; ix<num_regions; ++ix) {
        a->process("imc_proc_node", &tmp_node);
        add_imc_processed_by_node(tmp_node);
    }
}


//----------------------------------------------------------------------
size_t
BundleIMCState::num_imc_dest_nodes() const
{
    size_t result = 0;
    if (sptr_imc_dest_map_) {
        result = sptr_imc_dest_map_->size();
    }
    return result;
}

//----------------------------------------------------------------------
void
BundleIMCState::add_imc_orig_dest_node(size_t dest_node)
{
    if (dest_node > 0) {
        // add this node to this list of dest nodes from the received bundle [block]
        if (!sptr_imc_orig_dest_map_) {
            sptr_imc_orig_dest_map_ = std::make_shared<IMC_DESTINATIONS_MAP>();
        }

        auto iter = sptr_imc_orig_dest_map_->find(dest_node);

        if (iter == sptr_imc_orig_dest_map_->end()) {
            sptr_imc_orig_dest_map_->insert(IMC_DESTINATIONS_PAIR(dest_node, IMC_DEST_NODE_NOT_HANDLED));
        }

        // also add this node to the working list
        add_imc_dest_node(dest_node);
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::add_imc_dest_node(size_t dest_node)
{
    if (dest_node > 0) {
        if (!sptr_imc_dest_map_) {
            sptr_imc_dest_map_ = std::make_shared<IMC_DESTINATIONS_MAP>();
        }

        auto iter = sptr_imc_dest_map_->find(dest_node);

        if (iter == sptr_imc_dest_map_->end()) {
            sptr_imc_dest_map_->insert(IMC_DESTINATIONS_PAIR(dest_node, IMC_DEST_NODE_NOT_HANDLED));
            ++imc_dest_nodes_not_handled_;
        }
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::imc_dest_node_handled(size_t dest_node)
{
    if (dest_node > 0) {
        if (!sptr_imc_dest_map_) {
            sptr_imc_dest_map_ = std::make_shared<IMC_DESTINATIONS_MAP>();
        }

        auto iter = sptr_imc_dest_map_->find(dest_node);

        if (iter == sptr_imc_dest_map_->end()) {
            // not found - probably just delivered to local node before list of dest nodes added
            sptr_imc_dest_map_->insert(IMC_DESTINATIONS_PAIR(dest_node, IMC_DEST_NODE_HANDLED));
            ++imc_dest_nodes_handled_;
        } else {
            if (iter->second == IMC_DEST_NODE_NOT_HANDLED) {
                // change value from not handled (false) to handled (true) and update oounts
                iter->second = IMC_DEST_NODE_HANDLED;
                --imc_dest_nodes_not_handled_;
                ++imc_dest_nodes_handled_;
            } else {
                // already marked as handled - nothing to do
            }
        }
    }
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
BundleIMCState::imc_dest_map() const
{
    return sptr_imc_dest_map_;
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
BundleIMCState::imc_orig_dest_map() const
{
    return sptr_imc_orig_dest_map_;
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
BundleIMCState::imc_alternate_dest_map() const
{
    return sptr_imc_alternate_dest_map_;
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
BundleIMCState::imc_dest_map_for_link(std::string& linkname) const
{
    SPtr_IMC_DESTINATIONS_MAP sptr_map;

    if (sptr_imc_link_dest_map_) {
        auto iter_link_map = sptr_imc_link_dest_map_->find(linkname);

        if (iter_link_map != sptr_imc_link_dest_map_->end()) {
            sptr_map = iter_link_map->second;
        }
    }

    return sptr_map;
}

//----------------------------------------------------------------------
std::string
BundleIMCState::imc_link_name_by_index(size_t index)
{
    std::string result;

    SPtr_IMC_DESTINATIONS_MAP sptr_map;

    if (sptr_imc_link_dest_map_) {
        auto iter_link_map = sptr_imc_link_dest_map_->begin();

        size_t ctr = 0;
        while (iter_link_map != sptr_imc_link_dest_map_->end()) {
            if (ctr == index) {
                result = iter_link_map->first;
                break;
            }

            ++ctr;
            ++iter_link_map;
        }
    }

    return result;
}

//----------------------------------------------------------------------
SPtr_IMC_PROCESSED_REGIONS_MAP
BundleIMCState::imc_processed_regions_map() const
{
    return sptr_imc_processed_regions_map_;
}

//----------------------------------------------------------------------
size_t
BundleIMCState::num_imc_processed_regions() const
{
    size_t result = 0;
    if (sptr_imc_processed_regions_map_) {
        result = sptr_imc_processed_regions_map_->size();
    }

    return result;
}

//----------------------------------------------------------------------
bool
BundleIMCState::is_imc_region_processed(size_t region)
{
    bool result = false;
    if (sptr_imc_processed_regions_map_) {
        auto iter = sptr_imc_processed_regions_map_->find(region);
        result = (iter != sptr_imc_processed_regions_map_->end());
    }

    return result;
}

//----------------------------------------------------------------------
void
BundleIMCState::add_imc_region_processed(size_t region)
{
    if (!sptr_imc_processed_regions_map_) {
        sptr_imc_processed_regions_map_ = std::make_shared<IMC_PROCESSED_REGIONS_MAP>();
    }

    (*sptr_imc_processed_regions_map_)[region] = true;
}

//----------------------------------------------------------------------
void
BundleIMCState::add_imc_dest_node_via_link(std::string linkname, size_t dest_node)
{
    SPtr_IMC_DESTINATIONS_MAP sptr_map;

    // create the map or maps if it doesn't exist
    if (!sptr_imc_link_dest_map_) {
        sptr_imc_link_dest_map_ = std::make_shared<IMC_DESTINATIONS_PER_LINK_MAP>();
    }

    // try to find an existing map for this link
    auto iter_link_map = sptr_imc_link_dest_map_->find(linkname);

    if (iter_link_map != sptr_imc_link_dest_map_->end()) {
        sptr_map = iter_link_map->second;
    } else {
        // not found so create one
        sptr_map = std::make_shared<IMC_DESTINATIONS_MAP>();
        (*sptr_imc_link_dest_map_)[linkname] = sptr_map;
    }

    (*sptr_map)[dest_node] = IMC_DEST_NODE_NOT_HANDLED;
}

//----------------------------------------------------------------------
void
BundleIMCState::copy_all_unhandled_nodes_to_via_link(std::string linkname)
{
    SPtr_IMC_DESTINATIONS_MAP sptr_map;

    // create the map or maps if it doesn't exist
    if (!sptr_imc_link_dest_map_) {
        sptr_imc_link_dest_map_ = std::make_shared<IMC_DESTINATIONS_PER_LINK_MAP>();
    }

    // try to find an existing map for this link
    auto iter_link_map = sptr_imc_link_dest_map_->find(linkname);

    if (iter_link_map != sptr_imc_link_dest_map_->end()) {
        sptr_map = iter_link_map->second;
    } else {
        // not found so create one
        sptr_map = std::make_shared<IMC_DESTINATIONS_MAP>();
        (*sptr_imc_link_dest_map_)[linkname] = sptr_map;
    }


    // copy the unhandled nodes to this via link's list
    auto iter = sptr_imc_dest_map_->begin();

    size_t tmp_node;
    while (iter != sptr_imc_dest_map_->end()) {
        if (iter->second == IMC_DEST_NODE_NOT_HANDLED) {
            tmp_node = iter->first;
            (*sptr_map)[tmp_node] = IMC_DEST_NODE_NOT_HANDLED;
        }
        ++iter;
    }
}



//----------------------------------------------------------------------
void
BundleIMCState::clear_imc_link_lists()
{
    SPtr_IMC_DESTINATIONS_MAP sptr_map;

    // create the map or maps if it doesn't exist
    if (sptr_imc_link_dest_map_) {
        sptr_imc_link_dest_map_->clear();
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::imc_link_transmit_success(std::string linkname)
{
    bool found_list = false;

    if (sptr_imc_link_dest_map_) {
        // try to find an existing map for this link
        auto iter_link_map = sptr_imc_link_dest_map_->find(linkname);

        if (iter_link_map != sptr_imc_link_dest_map_->end()) {
            found_list = true;

            // mark all of the nodes in the list as processed
            auto sptr_map = iter_link_map->second;
            auto iter_nodes = sptr_map->begin();
            while (iter_nodes != sptr_map->end()) {
                imc_dest_node_handled(iter_nodes->first);
                ++iter_nodes;
            }

            // Remove the entry for this link
            sptr_imc_link_dest_map_->erase(iter_link_map);
        }
    }

    if (!found_list && sptr_imc_dest_map_) {
        auto iter_nodes = sptr_imc_dest_map_->begin();
        while (iter_nodes != sptr_imc_dest_map_->end()) {
            if (iter_nodes->second != IMC_DEST_NODE_HANDLED) {
                imc_dest_node_handled(iter_nodes->first);
            }
            ++iter_nodes;
        }
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::imc_link_transmit_failure(std::string linkname)
{
    if (sptr_imc_link_dest_map_) {
        // try to find an existing map for this link
        auto iter_link_map = sptr_imc_link_dest_map_->find(linkname);

        if (iter_link_map != sptr_imc_link_dest_map_->end()) {
            // Remove the entry for this link
            sptr_imc_link_dest_map_->erase(iter_link_map);
        }
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::format_verbose_imc_orig_dest_map(oasys::StringBuffer* buf)
{
    if (!sptr_imc_orig_dest_map_ || (sptr_imc_orig_dest_map_->size() == 0)) {
        buf->append("IMC original Dest Nodes: none\n");
    } else {
        // output list of nodes yet to be handled (delivered/transmitted)
        buf->appendf("IMC orignial Dest Nodes (%zu):", sptr_imc_orig_dest_map_->size());
        
        auto iter = sptr_imc_orig_dest_map_->begin();

        size_t ctr = 0;
        while (iter != sptr_imc_orig_dest_map_->end()) {
            size_t node_num = iter->first;
            if ((ctr % 8) == 0) {
                buf->append("\n    ");
            }
            ++ctr;

            buf->appendf("%7" PRIu64 "  ", node_num);

            ++iter;
        }

        buf->append("\n");
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::format_verbose_imc_dest_map(oasys::StringBuffer* buf)
{
    if (!sptr_imc_dest_map_ || (sptr_imc_dest_map_->size() == 0)) {
        buf->append("IMC working Dest Nodes: none\n");
    } else {
        // output list of nodes yet to be handled (delivered/transmitted)
        if (imc_dest_nodes_not_handled_ == 0) {
            buf->append("IMC working Dest Nodes to be handled: none\n");
        } else {
            buf->appendf("IMC working Dest Nodes to be handled (%zu):", imc_dest_nodes_not_handled_);
        
            auto iter = sptr_imc_dest_map_->begin();
    
            size_t ctr = 0;
            while (iter != sptr_imc_dest_map_->end()) {
                if (iter->second == IMC_DEST_NODE_NOT_HANDLED) {
                    size_t node_num = iter->first;
                    if ((ctr % 8) == 0) {
                        buf->append("\n    ");
                    }
                    ++ctr;

                    buf->appendf("%7" PRIu64 "  ", node_num);
                }

                ++iter;
            }
            ASSERT(ctr == imc_dest_nodes_not_handled_);

            buf->append("\n");
        }

        // output list of nodes that have been handled (delivered/transmitted)
        if (imc_dest_nodes_handled_ == 0) {
            buf->append("IMC working Dest Nodes handled: none");
        } else {
            buf->appendf("IMC working Dest Nodes handled (%zu):", imc_dest_nodes_handled_);
        
            auto iter = sptr_imc_dest_map_->begin();
    
            size_t ctr = 0;
            while (iter != sptr_imc_dest_map_->end()) {
                if (iter->second == IMC_DEST_NODE_HANDLED) {
                    size_t node_num = iter->first;
                    if ((ctr % 8) == 0) {
                        buf->append("\n    ");
                    }
                    ++ctr;

                    buf->appendf("%7" PRIu64 "  ", node_num);
                }

                ++iter;
            }
            ASSERT(ctr == imc_dest_nodes_handled_);
        }

        buf->append("\n");
    }
}

//----------------------------------------------------------------------
void
BundleIMCState::format_verbose_imc_dest_nodes_per_link(oasys::StringBuffer* buf)
{
    if (!sptr_imc_link_dest_map_ || (sptr_imc_link_dest_map_->size() == 0)) {
        buf->append("IMC Dest Nodes per Link: none\n");
    } else {
        buf->appendf("IMC Dest Nodes per Link (%zu):\n", sptr_imc_link_dest_map_->size());

        IMC_DESTINATIONS_PER_LINK_MAP::iterator imc_link_iter = sptr_imc_link_dest_map_->begin();

        while (imc_link_iter != sptr_imc_link_dest_map_->end()) {
            SPtr_IMC_DESTINATIONS_MAP sptr_map = imc_link_iter->second;

            buf->appendf("-- Link %s (%zu):", imc_link_iter->first.c_str(), sptr_map->size());

            size_t ctr = 0;
            IMC_DESTINATIONS_MAP::iterator imc_dest_iter = sptr_map->begin();
            while (imc_dest_iter != sptr_map->end()) {
                size_t node_num = imc_dest_iter->first;
                if ((ctr % 8) == 0) {
                    buf->append("\n    ");
                }
                ++ctr;

                buf->appendf("%7" PRIu64 "  ", node_num);
                ++imc_dest_iter;
            }
            if ((ctr % 8) != 0) {
                buf->append("\n");
            }

            ++imc_link_iter;
        }
    }


    // output list of nodes that have been handled (delivered/transmitted)
    if (!sptr_imc_unrouteable_dest_map_) {
        buf->append("IMC unrouteable Dest Nodes: none\n");
    } else {
        buf->appendf("IMC unrouteable Dest Nodes (%zu):", sptr_imc_unrouteable_dest_map_->size());
    
        auto iter = sptr_imc_unrouteable_dest_map_->begin();

        size_t ctr = 0;
        while (iter != sptr_imc_unrouteable_dest_map_->end()) {
            size_t node_num = iter->first;
            if ((ctr % 8) == 0) {
                buf->append("\n    ");
            }
            ++ctr;

            buf->appendf("%7" PRIu64 "  ", node_num);

            ++iter;
        }
        if ((ctr % 8) != 0) {
            buf->append("\n");
        }
    }
    buf->append("\n");
}

//----------------------------------------------------------------------
void
BundleIMCState::format_verbose_imc_state(oasys::StringBuffer* buf)
{
#define bool_to_str(x)   ((x) ? "true" : "false")

    buf->appendf("IMC_is_dtnme_node: %s\n", bool_to_str(imc_is_dtnme_node_));
    buf->appendf("IMC_is_router_node: %s\n", bool_to_str(imc_is_router_node_));
    buf->appendf("IMC_sync_request: %s\n", bool_to_str(imc_sync_request_));
    buf->appendf("IMC_sync_reply: %s\n", bool_to_str(imc_sync_reply_));
    buf->appendf("IMC_is_proxy_petition: %s\n", bool_to_str(imc_is_proxy_petition_));

    if (!sptr_imc_processed_regions_map_ || (sptr_imc_processed_regions_map_->size() == 0)) {
        buf->append("IMC Processed Regions: none\n");
    } else {
        buf->appendf("IMC Processed Regions (%zu):", sptr_imc_processed_regions_map_->size());

        size_t ctr = 0;
        IMC_DESTINATIONS_MAP::iterator imc_dest_iter = sptr_imc_processed_regions_map_->begin();
        while (imc_dest_iter != sptr_imc_processed_regions_map_->end()) {
            size_t node_num = imc_dest_iter->first;
            if ((ctr % 8) == 0) {
                buf->append("\n    ");
            }
            ++ctr;

            buf->appendf("%7" PRIu64 "  ", node_num);
            ++imc_dest_iter;
        }

        buf->append("\n");
    }

    if (!sptr_imc_processed_by_nodes_map_ || (sptr_imc_processed_by_nodes_map_->size() == 0)) {
        buf->append("IMC Processed By Nodes: none\n");
    } else {
        buf->appendf("IMC Processed By Nodes (%zu):", sptr_imc_processed_by_nodes_map_->size());

        size_t ctr = 0;
        IMC_DESTINATIONS_MAP::iterator imc_dest_iter = sptr_imc_processed_by_nodes_map_->begin();
        while (imc_dest_iter != sptr_imc_processed_by_nodes_map_->end()) {
            size_t node_num = imc_dest_iter->first;
            if ((ctr % 8) == 0) {
                buf->append("\n    ");
            }
            ++ctr;

            buf->appendf("%7" PRIu64 "  ", node_num);
            ++imc_dest_iter;
        }

        buf->append("\n");
    }

    buf->append("\n");
}

//----------------------------------------------------------------------
void
BundleIMCState::add_imc_alternate_dest_node(size_t dest_node)
{
    if (dest_node > 0) {
        if (!sptr_imc_alternate_dest_map_) {
            sptr_imc_alternate_dest_map_ = std::make_shared<IMC_DESTINATIONS_MAP>();
        }

        auto iter = sptr_imc_alternate_dest_map_->find(dest_node);

        if (iter == sptr_imc_alternate_dest_map_->end()) {
            sptr_imc_alternate_dest_map_->insert(IMC_DESTINATIONS_PAIR(dest_node, IMC_DEST_NODE_NOT_HANDLED));
        }
    }
}


//----------------------------------------------------------------------
void
BundleIMCState::clear_imc_alternate_dest_nodes()
{
    // loop through list and 

    if (sptr_imc_alternate_dest_map_) {
        sptr_imc_alternate_dest_map_->clear();
    }
}


//----------------------------------------------------------------------
size_t
BundleIMCState::imc_alternate_dest_nodes_count()
{
    size_t result = 0;

    if (sptr_imc_alternate_dest_map_) {
        result = sptr_imc_alternate_dest_map_->size();
    }


    return result;
}



//----------------------------------------------------------------------
void
BundleIMCState::add_imc_processed_by_node(size_t node_num)
{
    if (node_num > 0) {
        if (!sptr_imc_processed_by_nodes_map_) {
            sptr_imc_processed_by_nodes_map_ = std::make_shared<IMC_PROCESSED_BY_NODE_MAP>();
        }

        auto iter = sptr_imc_processed_by_nodes_map_->find(node_num);

        if (iter == sptr_imc_processed_by_nodes_map_->end()) {
            sptr_imc_processed_by_nodes_map_->insert(IMC_PROCESSED_BY_NODE_PAIR(node_num, true));
        }
    }
}

//----------------------------------------------------------------------
bool 
BundleIMCState::imc_has_proxy_been_processed_by_node(size_t node_num)
{
    bool result = false;

    if (sptr_imc_processed_by_nodes_map_) {
        auto iter = sptr_imc_processed_by_nodes_map_->find(node_num);

        result = (iter != sptr_imc_processed_by_nodes_map_->end());
    }

    return result;
}


//----------------------------------------------------------------------
SPtr_IMC_PROCESSED_BY_NODE_MAP
BundleIMCState::imc_processed_by_nodes_map() const
{
    return sptr_imc_processed_by_nodes_map_;
}

//----------------------------------------------------------------------
void
BundleIMCState::copy_imc_processed_by_node_list(SPtr_IMC_PROCESSED_BY_NODE_MAP& sptr_other_list)
{
    if (sptr_other_list) {
        auto iter = sptr_other_list->begin();

        while (iter != sptr_other_list->end()) {
            add_imc_processed_by_node(iter->first);
            ++iter;
        }
    }
}


//----------------------------------------------------------------------
size_t
BundleIMCState::imc_size_of_sdnv_dest_nodes_array() const
{
    size_t total_len = SDNV::encoding_len(imc_dest_nodes_not_handled_);

    if (sptr_imc_dest_map_) {
        auto iter = sptr_imc_dest_map_->begin();

        while (iter != sptr_imc_dest_map_->end()) {
            if (iter->second == IMC_DEST_NODE_NOT_HANDLED) {
                total_len += SDNV::encoding_len(iter->first);
            }
            ++iter;
        }
    }
    return total_len;
}

//----------------------------------------------------------------------
ssize_t
BundleIMCState::imc_sdnv_encode_dest_nodes_array(u_char* buf_ptr, size_t buf_len) const
{
    // first encode the num dest nodes
    ssize_t result = 0;
    ssize_t enc_stat = SDNV::encode(imc_dest_nodes_not_handled_, buf_ptr, buf_len);

    if (enc_stat <= 0) {
        // return error
        result = enc_stat;
    } else {
        buf_ptr += enc_stat;
        buf_len -= enc_stat;
        result += enc_stat;

        if (sptr_imc_dest_map_) {
            auto iter = sptr_imc_dest_map_->begin();

            while (iter != sptr_imc_dest_map_->end()) {
                if (iter->second == IMC_DEST_NODE_NOT_HANDLED) {
                    enc_stat = SDNV::encode(iter->first, buf_ptr, buf_len);

                    if (enc_stat <= 0) {
                        // return error
                        result = enc_stat;
                        break;
                    } else {
                        buf_ptr += enc_stat;
                        buf_len -= enc_stat;
                        result += enc_stat;
                    }
                }
                ++iter;
            }
        }
    }
    return result;
}

//----------------------------------------------------------------------
size_t
BundleIMCState::imc_size_of_sdnv_processed_regions_array() const
{
    size_t total_len = SDNV::encoding_len(num_imc_processed_regions());

    if (sptr_imc_processed_regions_map_) {
        auto iter = sptr_imc_processed_regions_map_->begin();

        while (iter != sptr_imc_processed_regions_map_->end()) {
            total_len += SDNV::encoding_len(iter->first);

            ++iter;
        }
    }

    return total_len;
}

//----------------------------------------------------------------------
ssize_t
BundleIMCState::imc_sdnv_encode_processed_regions_array(u_char* buf_ptr, size_t buf_len) const
{
    // first encode the num dest nodes
    ssize_t result = 0;
    ssize_t enc_stat = SDNV::encode(num_imc_processed_regions(), buf_ptr, buf_len);

    if (enc_stat <= 0) {
        // return error
        result = enc_stat;
    } else {
        buf_ptr += enc_stat;
        buf_len -= enc_stat;
        result += enc_stat;

        if (sptr_imc_processed_regions_map_) {
            auto iter = sptr_imc_processed_regions_map_->begin();

            while (iter != sptr_imc_processed_regions_map_->end()) {
                enc_stat = SDNV::encode(iter->first, buf_ptr, buf_len);

                if (enc_stat <= 0) {
                    // return error
                    result = enc_stat;
                    break;
                } else {
                    buf_ptr += enc_stat;
                    buf_len -= enc_stat;
                    result += enc_stat;
                }
                ++iter;
            }
        }
    }
    return result;
}

//----------------------------------------------------------------------
SPtr_IMC_DESTINATIONS_MAP
BundleIMCState::imc_unrouteable_dest_map() const
{
    return sptr_imc_unrouteable_dest_map_;
}


//----------------------------------------------------------------------
void
BundleIMCState::clear_imc_unrouteable_dest_nodes()
{
    if (sptr_imc_unrouteable_dest_map_) {
        sptr_imc_unrouteable_dest_map_->clear();
    }

    imc_home_region_unroutable_ = 0;
    imc_outer_regions_unroutable_ = 0;
}


//----------------------------------------------------------------------
void
BundleIMCState::add_imc_unrouteable_dest_node(size_t dest_node, bool in_home_region)
{
    if (dest_node > 0) {
        // add this node to this list of dest nodes from the received bundle [block]
        if (!sptr_imc_unrouteable_dest_map_) {
            sptr_imc_unrouteable_dest_map_ = std::make_shared<IMC_DESTINATIONS_MAP>();
        }

        auto iter = sptr_imc_unrouteable_dest_map_->find(dest_node);

        if (iter == sptr_imc_unrouteable_dest_map_->end()) {
            sptr_imc_unrouteable_dest_map_->insert(IMC_DESTINATIONS_PAIR(dest_node, in_home_region));

            if (in_home_region) {
                ++imc_home_region_unroutable_;
            } else {
                ++imc_outer_regions_unroutable_;
            }
        }
    }
}

} // namespace dtn
