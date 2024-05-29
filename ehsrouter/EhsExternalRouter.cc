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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <string.h>

#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/util/StringBuffer.h>

#include "EhsExternalRouter.h"
#include "EhsExternalRouterImpl.h"

namespace dtn {


//----------------------------------------------------------------------
EhsExternalRouter::EhsExternalRouter(LOG_CALLBACK_FUNC log_callback, int log_level, bool oasys_app)
{
    if (!oasys_app)
    {
        if (!oasys::Log::initialized()) {
            oasys::Log::init("-", oasys::LOG_INFO, "", "~/.ehsrouterdebug");  // "-" is stdout
        }
    }

    ehs_ext_router_ = new EhsExternalRouterImpl(log_callback, log_level, oasys_app);
}

//----------------------------------------------------------------------
EhsExternalRouter::~EhsExternalRouter()
{
    // ehs_ext_router_ will delete itself
}

//----------------------------------------------------------------------
void
EhsExternalRouter::start()
{
    ehs_ext_router_->start();
}


//----------------------------------------------------------------------
bool
EhsExternalRouter::started()
{
    return ehs_ext_router_->started();
}


//----------------------------------------------------------------------
void
EhsExternalRouter::stop()
{
    ehs_ext_router_->set_should_stop();
    sleep(1);
    delete this;
}


//----------------------------------------------------------------------
bool 
EhsExternalRouter::shutdown_server()
{
    return ehs_ext_router_->shutdown_server();
}

//----------------------------------------------------------------------
bool 
EhsExternalRouter::configure_use_tcp_interface(std::string& val)
{
    return ehs_ext_router_->configure_use_tcp_interface(val);
}

//----------------------------------------------------------------------
bool 
EhsExternalRouter::configure_remote_address(std::string& val)
{
    return ehs_ext_router_->configure_remote_address(val);
}

//----------------------------------------------------------------------
bool 
EhsExternalRouter::configure_remote_port(std::string& val)
{
    return ehs_ext_router_->configure_remote_port(val);
}

//----------------------------------------------------------------------
bool 
EhsExternalRouter::configure_network_interface(std::string& val)
{
    return ehs_ext_router_->configure_network_interface(val);
}

//----------------------------------------------------------------------
bool 
EhsExternalRouter::configure_schema_file(std::string& val)
{
    return ehs_ext_router_->configure_schema_file(val);
}


//----------------------------------------------------------------------
bool
EhsExternalRouter::configure_forward_link(std::string& val)
{
    return ehs_ext_router_->configure_forward_link(val);
}

//----------------------------------------------------------------------
bool
EhsExternalRouter::configure_fwdlink_transmit_enable(std::string& val)
{
    return ehs_ext_router_->configure_fwdlink_transmit_enable(val);
}

//----------------------------------------------------------------------
bool
EhsExternalRouter::configure_fwdlink_transmit_disable(std::string& val)
{
    return ehs_ext_router_->configure_fwdlink_transmit_disable(val);
}

//----------------------------------------------------------------------
void
EhsExternalRouter::configure_fwdlnk_allow_ltp_acks_while_disabled(bool allowed)
{
    ehs_ext_router_->configure_fwdlnk_allow_ltp_acks_while_disabled(allowed);
}

//----------------------------------------------------------------------
bool
EhsExternalRouter::configure_link_enable(std::string& val)
{
    return ehs_ext_router_->configure_link_enable(val);
}

//----------------------------------------------------------------------
bool
EhsExternalRouter::configure_link_disable(std::string& val)
{
    return ehs_ext_router_->configure_link_disable(val);
}

//----------------------------------------------------------------------
bool
EhsExternalRouter::configure_max_expiration_fwd(std::string& val)
{
    return ehs_ext_router_->configure_max_expiration_fwd(val);
}

//----------------------------------------------------------------------
bool
EhsExternalRouter::configure_max_expiration_rtn(std::string& val)
{
    return ehs_ext_router_->configure_max_expiration_rtn(val);
}

//----------------------------------------------------------------------
bool
EhsExternalRouter::configure_accept_custody(std::string& val)
{
    return ehs_ext_router_->configure_accept_custody(val);
}


//----------------------------------------------------------------------
const char* 
EhsExternalRouter::update_statistics()
{
    return ehs_ext_router_->update_statistics();
}


//----------------------------------------------------------------------
const char* 
EhsExternalRouter::update_statistics2()
{
    return ehs_ext_router_->update_statistics2();
}


//----------------------------------------------------------------------
const char* 
EhsExternalRouter::update_statistics3()
{
    return ehs_ext_router_->update_statistics3();
}


//----------------------------------------------------------------------
int
EhsExternalRouter::num_dtn_nodes()
{
    return ehs_ext_router_->num_dtn_nodes();
}

//----------------------------------------------------------------------
void
EhsExternalRouter::set_link_statistics(bool enabled)
{
    ehs_ext_router_->set_link_statistics(enabled);
}

//----------------------------------------------------------------------
void
EhsExternalRouter::bundle_list(std::string& buf)
{
    oasys::StringBuffer obuf;
    ehs_ext_router_->bundle_list(&obuf);
    buf = obuf.c_str();
}

//----------------------------------------------------------------------
void
EhsExternalRouter::bundle_stats_by_src_dst(int* count, EhsBundleStats** stats)
{
    if (NULL == count) return;
    if (stats == NULL || *stats != NULL) {
        *count = 0;
        return;
    }

    ehs_ext_router_->bundle_stats_by_src_dst(count, stats);
}

//----------------------------------------------------------------------
void
EhsExternalRouter::bundle_stats_by_src_dst_free(int count, EhsBundleStats** stats)
{
    ehs_ext_router_->bundle_stats_by_src_dst_free(count, stats);
}

//----------------------------------------------------------------------
void
EhsExternalRouter::fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats)
{
    if (NULL == count) return;
    if (stats == NULL || *stats != NULL) {
        *count = 0;
        return;
    }

    ehs_ext_router_->fwdlink_interval_stats(count, stats);
}

//----------------------------------------------------------------------
void
EhsExternalRouter::fwdlink_interval_stats_free(int count, EhsFwdLinkIntervalStats** stats)
{
    ehs_ext_router_->fwdlink_interval_stats_free(count, stats);
}


//----------------------------------------------------------------------
void
EhsExternalRouter::unrouted_bundle_stats_by_src_dst(std::string& buf)
{
    oasys::StringBuffer obuf;
    ehs_ext_router_->unrouted_bundle_stats_by_src_dst(&obuf);
    buf = obuf.c_str();
}

//----------------------------------------------------------------------
void
EhsExternalRouter::bundle_stats(std::string& buf)
{
    oasys::StringBuffer obuf;
    ehs_ext_router_->bundle_stats(&obuf);
    buf = obuf.c_str();
}

//----------------------------------------------------------------------
void
EhsExternalRouter::request_bard_usage_stats()
{
    ehs_ext_router_->request_bard_usage_stats();
}

//----------------------------------------------------------------------
bool
EhsExternalRouter::bard_usage_stats(EhsBARDUsageStatsVector& usage_stats, EhsRestageCLStatsVector& cl_stats)
{
    return ehs_ext_router_->bard_usage_stats(usage_stats, cl_stats);
}

//----------------------------------------------------------------------
void
EhsExternalRouter::bard_add_quota(EhsBARDUsageStats& quota)
{
    ehs_ext_router_->bard_add_quota(quota);
}


//----------------------------------------------------------------------
void
EhsExternalRouter::bard_del_quota(EhsBARDUsageStats& quota)
{
    ehs_ext_router_->bard_del_quota(quota);
}


//----------------------------------------------------------------------
void
EhsExternalRouter::send_link_add_msg(std::string& link_id, std::string& next_hop, std::string& link_mode,
                                   std::string& cl_name,  LinkParametersVector& params)
{
    ehs_ext_router_->send_link_add_msg(link_id, next_hop, link_mode, cl_name, params);
}

//----------------------------------------------------------------------
void
EhsExternalRouter::send_link_del_msg(std::string& link_id)
{
    ehs_ext_router_->send_link_del_msg(link_id);
}




//----------------------------------------------------------------------
uint64_t
EhsExternalRouter::max_bytes_sent()
{
    return ehs_ext_router_->max_bytes_sent();
}

//----------------------------------------------------------------------
uint64_t
EhsExternalRouter::max_bytes_recv()
{
    return ehs_ext_router_->max_bytes_recv();
}


//----------------------------------------------------------------------
void
EhsExternalRouter::link_dump(std::string& buf)
{
    oasys::StringBuffer obuf;
    ehs_ext_router_->link_dump(&obuf);
    buf = obuf.c_str();
}

//----------------------------------------------------------------------
void
EhsExternalRouter::fwdlink_transmit_dump(std::string& buf)
{
    oasys::StringBuffer obuf;
    ehs_ext_router_->fwdlink_transmit_dump(&obuf);
    buf = obuf.c_str();
}

//----------------------------------------------------------------------
int
EhsExternalRouter::set_fwdlnk_enabled(uint32_t timeout_ms)
{
    return ehs_ext_router_->set_fwdlnk_enabled_state(true, timeout_ms);
}

//----------------------------------------------------------------------
int
EhsExternalRouter::set_fwdlnk_disabled(uint32_t timeout_ms)
{
    return ehs_ext_router_->set_fwdlnk_enabled_state(false, timeout_ms);
}


//----------------------------------------------------------------------
int
EhsExternalRouter::set_fwdlnk_aos(uint32_t timeout_ms)
{
    return ehs_ext_router_->set_fwdlnk_aos_state(true, timeout_ms);
}

//----------------------------------------------------------------------
int
EhsExternalRouter::set_fwdlnk_los(uint32_t timeout_ms)
{
    return ehs_ext_router_->set_fwdlnk_aos_state(false, timeout_ms);
}

//----------------------------------------------------------------------
int
EhsExternalRouter::set_fwdlnk_throttle(uint32_t bps, uint32_t timeout_ms)
{
    return ehs_ext_router_->set_fwdlnk_throttle(bps, timeout_ms);
}

//----------------------------------------------------------------------
int
EhsExternalRouter::bundle_delete(uint64_t source_node_id, uint64_t dest_node_id)
{
    return ehs_ext_router_->bundle_delete(source_node_id, dest_node_id);
}

//----------------------------------------------------------------------
int
EhsExternalRouter::bundle_delete_all()
{
    return ehs_ext_router_->bundle_delete_all();
}

//----------------------------------------------------------------------
void 
EhsExternalRouter::set_log_level(int level)
{
    if (level > 7) level = 7;  // 7 = oasys::LOG_ALWAYS
    ehs_ext_router_->set_log_level(level);
}

//----------------------------------------------------------------------
const char*
EhsExternalRouter::quota_type_to_str(uint64_t quota_type)
{
    switch (quota_type) {
        case EHSEXTRTR_BARD_QUOTA_TYPE_SRC: return "src";
        case EHSEXTRTR_BARD_QUOTA_TYPE_DST: return "dst";
        default:
            return "unk";
    }
}

//----------------------------------------------------------------------
const char*
EhsExternalRouter::scheme_type_to_str(uint64_t scheme)
{
    switch (scheme) {
        case EHSEXTRTR_BARD_QUOTA_SCHEME_IPN: return "ipn";
        case EHSEXTRTR_BARD_QUOTA_SCHEME_DTN: return "dtn";
        case EHSEXTRTR_BARD_QUOTA_SCHEME_IMC: return "imc";
        default:
            return "unk";
    }
}

//----------------------------------------------------------------------
const char*
EhsExternalRouter::cl_state_to_str(uint64_t cl_state)
{
    switch (cl_state) {
        case EHSEXTRTR_RESTAGE_CL_STATE_ONLINE: return "ONLINE";
        case EHSEXTRTR_RESTAGE_CL_STATE_LOW: return "LOW_QUOTA";
        case EHSEXTRTR_RESTAGE_CL_STATE_HIGH: return "HIGH_QUOTA";
        case EHSEXTRTR_RESTAGE_CL_STATE_FULL_QUOTA: return "FULL_QUOTA";
        case EHSEXTRTR_RESTAGE_CL_STATE_FULL_DISK: return "FULL_DISK";
        case EHSEXTRTR_RESTAGE_CL_STATE_ERROR: return "ERROR";
        case EHSEXTRTR_RESTAGE_CL_STATE_SHUTDOWN: return "SHUTDOWN";
        default:
            return "unknown";
    }
}


} // namespace dtn

