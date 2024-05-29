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

#include <inttypes.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/io/FileUtils.h>
#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/io/UDPClient.h>
#include <third_party/oasys/util/StringUtils.h>

#include "EhsDtnNode.h"
#include "EhsExternalRouterImpl.h"


namespace dtn {


//----------------------------------------------------------------------
EhsExternalRouterImpl::EhsExternalRouterImpl(LOG_CALLBACK_FUNC log_callback, int log_level, bool oasys_app)
//    : IOHandlerBase(new oasys::Notifier("/ehs/extrtr")),
    : Thread("/ehs/extrtr", oasys::Thread::DELETE_ON_EXIT),
      oasys::Logger("EhsExternalRouterImpl", "/ehs/extrtr"),
      ExternalRouterClientIF("EhsExternalRouterImpl"),
      eventq_(logpath_),
      log_callback_(log_callback),
      log_level_(log_level),
      oasys_app_(oasys_app),
      tcp_client_("/ehs/extrtr/tcpclient"),
      tcp_sender_()
{
    set_logpath("/ehs/extrtr");

    init();
}

//----------------------------------------------------------------------
EhsExternalRouterImpl::~EhsExternalRouterImpl()
{
    log_msg(oasys::LOG_INFO, "Destroying EhsExternalRouterImpl");

    tcp_sender_.set_should_stop();
    tcp_client_.close();

    // free list of DTN nodes
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        dtn_node_map_.erase(iter);
        node->do_shutdown();

        iter = dtn_node_map_.begin();
    }
    node = nullptr;

    // free the link config objects
    EhsLinkCfgIterator cliter = link_configs_.begin();
    while (cliter != link_configs_.end()) {
        EhsLinkCfg* cl = cliter->second;
        link_configs_.erase(cliter);
        delete cl;

        cliter = link_configs_.begin();
    }

    fwdlink_xmt_enabled_.clear();

    // free all pending events
    std::string *event;
    while (eventq_.try_pop(&event)) {
        delete event;
    }

}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::init() 
{
    tcp_sender_.set_parent(this);

    // start out in LOS state until informed we can start transmitting
    fwdlnk_aos_ = false;
    fwdlnk_enabled_ = false;
    fwdlnk_force_LOS_while_disabled_ = true;

    max_expiration_fwd_ = 8 * 60 * 60; // 8 hours worth of seconds
    max_expiration_rtn_ = 48 * 60 * 60; // 48 hours worth of seconds

    // intialize socket parameters
    remote_addr_ = htonl(INADDR_LOOPBACK);
    remote_port_ = 8001;

    // Default accepting custody of bundles to true
    accept_custody_.put_pair_double_wildcards(true);


    send_seq_ctr_ = 0;
    bytes_recv_ = 0;
    max_bytes_recv_ = 0;
}


//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_use_tcp_interface(std::string& val)
{
    (void) val;
    return true;
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_remote_address(std::string& val)
{
    return (0 == oasys::gethostbyname(val.c_str(), &remote_addr_));
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_remote_port(std::string& val)
{
    uint32_t port = strtoul(val.c_str(), nullptr, 10);
    bool result = (port > 0 && port < 65536);
    if (result) {
        remote_port_ = port;
    } else {
        log_msg(oasys::LOG_ERR, 
                "configure_mc_port - Invalid port number: %s", 
                val.c_str());
    }
    return result;
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_network_interface(std::string& val)
{
    (void) val;
    return true;
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_schema_file(std::string& val)
{
    // depricated - only retained for backward compatibility for a while
    (void) val;
    return true;
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::configure_forward_link(std::string& val)
{
    bool result = true;

    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    if (toks != 3) {
        result = false;
        log_msg(oasys::LOG_ERR, 
                "configure_forward_link: FORWARD_LINK has invalid number of parameters: %d",
                toks);
    } else {

        int tok_idx = 0;
        std::string link_id = tokens[tok_idx++];

        // load a temp EhsLinkCfg until we know all data is good
        EhsLinkCfg* cl = new EhsLinkCfg(link_id);

        // set the fwd link indication
        cl->is_fwdlnk_ = true;

        // parse throttle (bits per sec)
        char* endc = nullptr;
        cl->throttle_bps_ = strtoul(tokens[tok_idx++].c_str(), &endc, 10);

        // parse the reachable nodes list
        uint64_t node_id = 0;
        uint64_t end_node_id = 0;

        std::vector<std::string> node_tokens;

        int node_toks = oasys::tokenize(tokens[tok_idx++].c_str(), ",", &node_tokens);

        for (int ix=0; ix<node_toks; ix++) {
            node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);

            if ('\0' == *endc) {
                cl->source_nodes_[node_id] = true;
            } else if ('-' == *endc) {
                // a range has been specified
                char* end_str = endc + 1;
                end_node_id = strtoull(end_str, &endc, 10);

                if ('\0' == *endc  &&  end_node_id > node_id) {
                    if (end_node_id - node_id > 100) {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_forward_link: FORWARD_LINK has too many nodes in range: %s -- aborted", 
                                node_tokens[ix].c_str());
                        break;
                    } else {
                        for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                             cl->source_nodes_[jx] = true;
                        }
                    }
                } else {
                    result = false;
                    log_msg(oasys::LOG_ERR, 
                            "configure_forward_link: FORWARD_LINK has invalid range of Node IDs: %s  -- aborted", 
                            node_tokens[ix].c_str());
                    break;
                }
            } else {
                result = false;
                log_msg(oasys::LOG_ERR, 
                        "configure_forward_link: FORWARD_LINK has invalid Node ID: %s  -- aborted", 
                        node_tokens[ix].c_str());
                break;
            }
        }

        if (result) {
            do {
                oasys::ScopeLock l(&config_lock_, __func__);

                EhsLinkCfgIterator find_iter = link_configs_.find(link_id);
                if (find_iter != link_configs_.end()) {
                    // replace the config object
                    delete find_iter->second;
                    find_iter->second = cl;
                } else {
                    link_configs_[link_id] = cl;
                }
            } while (false); // limit scope of lock


            do {
                oasys::ScopeLock l(&node_map_lock_, __func__);

                // inform each DtnNode to reconfigure the link
                SPtr_EhsDtnNode node;
                EhsDtnNodeIterator iter = dtn_node_map_.begin();
                while (iter != dtn_node_map_.end()) {
                    node = iter->second;
                    node->reconfigure_link(link_id);
                    ++iter;
                }
            } while (false); // limit scope of lock

            log_msg(oasys::LOG_INFO, "configure_forward_link: FORWARD_LINK Link ID: %s - success",
                    link_id.c_str());
        } else {
            // not using this object because of an error
            delete cl;
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::configure_fwdlink_transmit_enable(std::string& val)
{
    bool result = true;

    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    if (toks != 2) {
        result = false;
        log_msg(oasys::LOG_ERR, 
                "configure_fwdlink_transmit_enable: FWDLINK_TRANSMIT_ENABLE has invalid number of parameters: %d",
                toks);
    } else {
        // check for special case setting the default
        if (tokens[0].compare("*") == 0 && tokens[1].compare("*") == 0) {
            fwdlink_xmt_enabled_.put_pair_double_wildcards(true);
            return true;
        }


        int tok_idx = 0;

        EhsNodeBoolMap tmp_src_list;
        EhsNodeBoolMap tmp_dst_list;

        // parse the source nodes list
        uint64_t node_id = 0;
        uint64_t end_node_id = 0;
        char* endc = nullptr;

        std::vector<std::string> node_tokens;

        int node_toks = oasys::tokenize(tokens[tok_idx++].c_str(), ",", &node_tokens);

        for (int ix=0; ix<node_toks; ix++) {
            node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);

            if ('\0' == *endc) {
                tmp_src_list[node_id] = true;
            } else if ('-' == *endc) {
                // a range has been specified
                char* end_str = endc + 1;
                end_node_id = strtoull(end_str, &endc, 10);

                if ('\0' == *endc  &&  end_node_id > node_id) {
                    if (end_node_id - node_id > 100) {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_fwdlink_transmit_enable: FWDLINK_TRANSMIT_ENABLE has too many nodes in source range: %s -- aborted", 
                                node_tokens[ix].c_str());
                        break;
                    } else {
                        for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                             tmp_src_list[jx] = true;
                        }
                    }
                } else {
                    result = false;
                    log_msg(oasys::LOG_ERR, 
                            "configure_fwdlink_transmit_enable: FWDLINK_TRANSMIT_ENABLE has invalid range of source Node IDs: %s  -- aborted", 
                            node_tokens[ix].c_str());
                    break;
                }
            } else {
                result = false;
                log_msg(oasys::LOG_ERR, 
                        "configure_fwdlink_transmit_enable: FWDLINK_TRANSMIT_ENABLE has invalid source Node ID: %s  -- aborted", 
                        node_tokens[ix].c_str());
                break;
            }
        }

        // parse the destination nodes list if no errors so far
        if (result) {
            node_toks = oasys::tokenize(tokens[tok_idx++].c_str(), ",", &node_tokens);
    
            for (int ix=0; ix<node_toks; ix++) {
                node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);

                if ('\0' == *endc) {
                    tmp_dst_list[node_id] = true;
                } else if ('-' == *endc) {
                    // a range has been specified
                    char* end_str = endc + 1;
                    end_node_id = strtoull(end_str, &endc, 10);

                    if ('\0' == *endc  &&  end_node_id > node_id) {
                        if (end_node_id - node_id > 100) {
                            result = false;
                            log_msg(oasys::LOG_ERR, 
                                    "configure_fwdlink_transmit_enable: FWDLINK_TRANSMIT_ENABLE (%s) has too many nodes in dest range: %s -- aborted", 
                                    node_tokens[ix].c_str());
                            break;
                        } else {
                            for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                                 tmp_dst_list[jx] = true;
                            }
                        }
                    } else {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_fwdlink_transmit_enable: FWDLINK_TRANSMIT_ENABLE (%s) has invalid range of dest Node IDs: %s  -- aborted", 
                                node_tokens[ix].c_str());
                        break;
                    }
                } else {
                    result = false;
                    log_msg(oasys::LOG_ERR, 
                            "configure_fwdlink_transmit_enable: FWDLINK_TRANSMIT_ENABLE has invalid dest Node ID: %s  -- aborted", 
                            node_tokens[ix].c_str());
                    break;
                }
            }
        }

        if (result) {
            do {
                oasys::ScopeLock l(&config_lock_, __func__);

                // update the list of enabled src/dest combos
                EhsNodeBoolIterator src_iter = tmp_src_list.begin();
                EhsNodeBoolIterator dst_iter;
                while (src_iter != tmp_src_list.end()) {
                    dst_iter = tmp_dst_list.begin();
                    while (dst_iter != tmp_dst_list.end()) {
                        fwdlink_xmt_enabled_.put_pair(src_iter->first, dst_iter->first, true);
                        ++dst_iter;
                    }
                    ++src_iter;
                }
            } while (false); // limit scope of lock
 
            do {
                oasys::ScopeLock l(&node_map_lock_, __func__);

                // inform each DtnNode to reconfigure the fwdlink transmit
                SPtr_EhsDtnNode node;
                EhsDtnNodeIterator iter = dtn_node_map_.begin();
                while (iter != dtn_node_map_.end()) {
                    node = iter->second;
                    node->fwdlink_transmit_enable(tmp_src_list, tmp_dst_list);
                    ++iter;
                }
            } while (false); // limit scope of lock

            log_msg(oasys::LOG_INFO, "configure_fwdlink_transmit_enable: FWDLINK_TRANSMIT_ENABLE - success");
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::configure_fwdlink_transmit_disable(std::string& val)
{
    bool result = true;

    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    if (toks != 2) {
        result = false;
        log_msg(oasys::LOG_ERR, 
                "configure_fwdlink_transmit_disable: FWDLINK_TRANSMIT_DISABLE has invalid number of parameters: %d",
                toks);
    } else {
        // check for special case setting the default
        if (tokens[0].compare("*") == 0 && tokens[1].compare("*") == 0) {
            fwdlink_xmt_enabled_.put_pair_double_wildcards(false);
            return true;
        }


        int tok_idx = 0;

        EhsNodeBoolMap tmp_src_list;
        EhsNodeBoolMap tmp_dst_list;

        // parse the source nodes list
        uint64_t node_id = 0;
        uint64_t end_node_id = 0;
        char* endc = nullptr;

        std::vector<std::string> node_tokens;

        int node_toks = oasys::tokenize(tokens[tok_idx++].c_str(), ",", &node_tokens);

        for (int ix=0; ix<node_toks; ix++) {
            node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);

            if ('\0' == *endc) {
                tmp_src_list[node_id] = true;
            } else if ('-' == *endc) {
                // a range has been specified
                char* end_str = endc + 1;
                end_node_id = strtoull(end_str, &endc, 10);

                if ('\0' == *endc  &&  end_node_id > node_id) {
                    if (end_node_id - node_id > 100) {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_fwdlink_transmit_disable: FWDLINK_TRANSMIT_DISABLE has too many nodes in source range: %s -- aborted", 
                                node_tokens[ix].c_str());
                        break;
                    } else {
                        for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                             tmp_src_list[jx] = true;
                        }
                    }
                } else {
                    result = false;
                    log_msg(oasys::LOG_ERR, 
                            "configure_fwdlink_transmit_disable: FWDLINK_TRANSMIT_DISABLE has invalid range of source Node IDs: %s  -- aborted", 
                            node_tokens[ix].c_str());
                    break;
                }
            } else {
                result = false;
                log_msg(oasys::LOG_ERR, 
                        "configure_fwdlink_transmit_disable: FWDLINK_TRANSMIT_DISABLE has invalid source Node ID: %s  -- aborted", 
                        node_tokens[ix].c_str());
                break;
            }
        }

        // parse the destination nodes list if no errors so far
        if (result) {
            node_toks = oasys::tokenize(tokens[tok_idx++].c_str(), ",", &node_tokens);
    
            for (int ix=0; ix<node_toks; ix++) {
                node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);

                if ('\0' == *endc) {
                    tmp_dst_list[node_id] = true;
                } else if ('-' == *endc) {
                    // a range has been specified
                    char* end_str = endc + 1;
                    end_node_id = strtoull(end_str, &endc, 10);

                    if ('\0' == *endc  &&  end_node_id > node_id) {
                        if (end_node_id - node_id > 100) {
                            result = false;
                            log_msg(oasys::LOG_ERR, 
                                    "configure_fwdlink_transmit_disable: FWDLINK_TRANSMIT_DISABLE (%s) has too many nodes in dest range: %s -- aborted", 
                                    node_tokens[ix].c_str());
                            break;
                        } else {
                            for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                                 tmp_dst_list[jx] = true;
                            }
                        }
                    } else {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_fwdlink_transmit_disable: FWDLINK_TRANSMIT_DISABLE (%s) has invalid range of dest Node IDs: %s  -- aborted", 
                                node_tokens[ix].c_str());
                        break;
                    }
                } else {
                    result = false;
                    log_msg(oasys::LOG_ERR, 
                            "configure_fwdlink_transmit_disable: FWDLINK_TRANSMIT_DISABLE has invalid dest Node ID: %s  -- aborted", 
                            node_tokens[ix].c_str());
                    break;
                }
            }
        }

        if (result) {
            do {
                oasys::ScopeLock l(&config_lock_, __func__);

                // update the list of enabled src/dest combos
                EhsNodeBoolIterator src_iter = tmp_src_list.begin();
                EhsNodeBoolIterator dst_iter;
                while (src_iter != tmp_src_list.end()) {
                    dst_iter = tmp_dst_list.begin();
                    while (dst_iter != tmp_dst_list.end()) {
                        fwdlink_xmt_enabled_.clear_pair(src_iter->first, dst_iter->first);
                        ++dst_iter;
                    }
                    ++src_iter;
                }
            } while (false); // limit scope of lock
 
            do {
                oasys::ScopeLock l(&node_map_lock_, __func__);

                // inform each DtnNode to reconfigure the fwdlink transmit
                SPtr_EhsDtnNode node;
                EhsDtnNodeIterator iter = dtn_node_map_.begin();
                while (iter != dtn_node_map_.end()) {
                    node = iter->second;
                    node->fwdlink_transmit_disable(tmp_src_list, tmp_dst_list);
                    ++iter;
                }
            } while (false); // limit scope of lock

            log_msg(oasys::LOG_INFO, "configure_fwdlink_transmit_disable: FWDLINK_TRANSMIT_DISABLE - success");
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::configure_fwdlnk_allow_ltp_acks_while_disabled(bool allowed)
{
    fwdlnk_force_LOS_while_disabled_ = !allowed;

    oasys::ScopeLock l(&node_map_lock_, __func__);

    // inform each DtnNode to reconfigure 
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->set_fwdlnk_force_LOS_while_disabled(fwdlnk_force_LOS_while_disabled_);
        ++iter;
    }

    log_msg(oasys::LOG_INFO, "configure_fwdlink_allow_ltp_acks_while_disabled: FWDLINK_TRANSMIT_DISABLE - success");
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::shutdown_server()
{
    bool result = true;

    oasys::ScopeLock l(&node_map_lock_, __func__);

    // inform each DtnNode to shutdown
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        result = result && node->shutdown_server();
        ++iter;
    }

    log_msg(oasys::LOG_INFO, "shutdown_server: %s", result ? "success":"failed");
    return result;
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::configure_link_enable(std::string& val)
{
    bool result = true;
    bool minor_error = false;

    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    if (toks != 4) {
        result = false;
        log_msg(oasys::LOG_ERR, 
                "configure_link_enable: LINK_ENABLE has invalid number of parameters: %d",
                toks);
    } else {

        EhsLinkCfg* cl;
        int tok_idx = 0;
        std::string link_id = tokens[tok_idx++];


        do {
            oasys::ScopeLock l(&config_lock_, __func__);

            EhsLinkCfgIterator find_iter = link_configs_.find(link_id);

            if (find_iter == link_configs_.end()) {
                // load a temp EhsLinkCfg until we know all data is good
                cl = new EhsLinkCfg(link_id);
                link_configs_[link_id] = cl;

                log_msg(oasys::LOG_INFO, "configure_link_enable: LINK_ENABLE added Link ID: %s - success",
                        link_id.c_str());
            } else {
                cl = find_iter->second;
            }


            // set the fwd link indication... NOT!
            cl->is_fwdlnk_ = false;

            // parse the "establish connection" flag
            cl->establish_connection_ = (0 == tokens[tok_idx++].compare("true"));

            // parse the source nodes list
            uint64_t node_id = 0;
            uint64_t end_node_id = 0;
            char* endc = nullptr;

            std::vector<std::string> node_tokens;

            int node_toks = oasys::tokenize(tokens[tok_idx++].c_str(), ",", &node_tokens);

            for (int ix=0; ix<node_toks; ix++) {
                node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);

                if ('\0' == *endc) {
                    cl->source_nodes_[node_id] = true;
                } else if ('-' == *endc) {
                    // a range has been specified
                    char* end_str = endc + 1;
                    end_node_id = strtoull(end_str, &endc, 10);

                    if ('\0' == *endc  &&  end_node_id > node_id) {
                        if (end_node_id - node_id > 10000) {
                            minor_error = true;
                            log_msg(oasys::LOG_ERR, 
                                    "configure_link_enable: LINK_ENABLE (%s) has a source range that is > 10,000 - only enabling endpoints: %" PRIu64 
                                    " and %" PRIu64,
                                    link_id.c_str(), node_id, end_node_id);

                             cl->source_nodes_[node_id] = true;
                             cl->source_nodes_[end_node_id] = true;

                        } else {
                            for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                                 cl->source_nodes_[jx] = true;
                            }
                        }
                    } else {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_link_enable: LINK_ENABLE (%s) has invalid range of source Node IDs: %s  -- aborted", 
                                link_id.c_str(), node_tokens[ix].c_str());
                        break;
                    }
                } else {
                    result = false;
                    log_msg(oasys::LOG_ERR, 
                            "configure_link_enable: LINK_ENABLE (%s) has invalid source Node ID: %s  -- aborted", 
                            link_id.c_str(), node_tokens[ix].c_str());
                    break;
                }
            }

            // parse the destination nodes list if no errors so far
            if (result) {
                node_toks = oasys::tokenize(tokens[tok_idx++].c_str(), ",", &node_tokens);
    
                for (int ix=0; ix<node_toks; ix++) {
                    node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);

                    if ('\0' == *endc) {
                        cl->dest_nodes_[node_id] = true;
                    } else if ('-' == *endc) {
                        // a range has been specified
                        char* end_str = endc + 1;
                        end_node_id = strtoull(end_str, &endc, 10);

                        if ('\0' == *endc  &&  end_node_id > node_id) {
                            if (end_node_id - node_id > 10000) {
                                minor_error = true;
                                log_msg(oasys::LOG_ERR, 
                                        "configure_link_enable: LINK_ENABLE (%s) has a destination range that is > 10,000 - only enabling endpoints: %" PRIu64 
                                        " and %" PRIu64,
                                        link_id.c_str(), node_id, end_node_id);

                                 cl->dest_nodes_[node_id] = true;
                                 cl->dest_nodes_[end_node_id] = true;
                            } else {
                                for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                                     cl->dest_nodes_[jx] = true;
                                }
                            }
                        } else {
                            result = false;
                            log_msg(oasys::LOG_ERR, 
                                    "configure_link_enable: LINK_ENABLE (%s) has invalid range of dest Node IDs: %s  -- aborted", 
                                    link_id.c_str(), node_tokens[ix].c_str());
                            break;
                        }
                    } else {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_link_enable: LINK_ENABLE (%s) has invalid dest Node ID: %s  -- aborted", 
                                link_id.c_str(), node_tokens[ix].c_str());
                        break;
                    }
                }
            }
        } while (false); // limit scope of lock




        if (result) {
            oasys::ScopeLock l(&node_map_lock_, __func__);

            // inform each DtnNode to reconfigure the link
            SPtr_EhsDtnNode node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->link_enable(cl);
                ++iter;
            }

            log_msg(oasys::LOG_DEBUG, "configure_link_enable: LINK_ENABLE Link ID: %s - success",
                    link_id.c_str());
        }
    }

    return result && !minor_error;
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::configure_link_disable(std::string& val)
{
    bool result = true;

    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    if (toks != 1) {
        result = false;
        log_msg(oasys::LOG_ERR, 
                "configure_link_disable: LINK_DISABLE has invalid number of parameters: %d",
                toks);
    } else {
        int tok_idx = 0;
        std::string link_id = tokens[tok_idx++];

        
        do {
            oasys::ScopeLock l(&config_lock_, __func__);

            EhsLinkCfgIterator find_iter = link_configs_.find(link_id);

            if (find_iter != link_configs_.end()) {
                EhsLinkCfg* cl = find_iter->second;
                link_configs_.erase(find_iter);
                delete cl;
            }
        } while (false); // limit scope of lock



        do {
            oasys::ScopeLock l(&node_map_lock_, __func__);
    
            // inform each DtnNode to close and delete the link
            SPtr_EhsDtnNode node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->link_disable(link_id);
                ++iter;
            }
        } while (false); // limit scope of lock

        log_msg(oasys::LOG_INFO, 
                "configure_link_disable: LINK_DISABLE Link ID (%s) disabled",
                link_id.c_str());
    }

    return result;
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::configure_max_expiration_fwd(std::string& val)
{
    bool result = false;

    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    if (toks != 1) {
        log_msg(oasys::LOG_ERR, 
                "configure_max_expiration_fwd: MAX_EXPIRATION_FWD has invalid number of parameters: %d",
                toks);
    } else {

        int tok_idx = 0;

        char* endc = nullptr;
        uint64_t exp = strtoull(tokens[tok_idx].c_str(), &endc, 10);
        if ('\0' != *endc) {
            log_msg(oasys::LOG_ERR, 
                    "configure_max_expiration_fwd: invalid value: %s",
                    tokens[tok_idx].c_str());
        } else if (exp > 0) {
            max_expiration_fwd_ = exp;
            result = true;
        }


        if (result) {
            oasys::ScopeLock l(&node_map_lock_, __func__);

            // inform each DtnNode to reconfigure the link
            SPtr_EhsDtnNode node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->reconfigure_max_expiration_fwd(max_expiration_fwd_);
                ++iter;
            }

            log_msg(oasys::LOG_INFO, 
                        "configure_max_expiration_fwd: MAX_EXPIRATION_FWD -- success");
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::configure_max_expiration_rtn(std::string& val)
{
    bool result = false;

    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    if (toks != 1) {
        log_msg(oasys::LOG_ERR, 
                "configure_max_expiration_rtn: MAX_EXPIRATION_RTN has invalid number of parameters: %d",
                toks);
    } else {

        int tok_idx = 0;

        char* endc = nullptr;
        uint64_t exp = strtoull(tokens[tok_idx].c_str(), &endc, 10);
        if ('\0' != *endc) {
            log_msg(oasys::LOG_ERR, 
                    "configure_max_expiration_rtn: invalid value: %s",
                    tokens[tok_idx].c_str());
        } else if (exp > 0) {
            max_expiration_rtn_ = exp;
            result = true;
        }


        if (result) {
            oasys::ScopeLock l(&node_map_lock_, __func__);

            // inform each DtnNode to reconfigure the link
            SPtr_EhsDtnNode node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->reconfigure_max_expiration_rtn(max_expiration_rtn_);
                ++iter;
            }

            log_msg(oasys::LOG_INFO, 
                        "configure_max_expiration_rtn: MAX_EXPIRATION_RTN -- success");
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_source_node_priority(std::string& val)
{
    bool result = true;

    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    if (toks != 2) {
        result = false;
        log_msg(oasys::LOG_ERR, 
                "configure_source_node_priority: SOURCE_PRIORITY has invalid number of parameters: %d",
                toks);
    } else {

        int tok_idx = 0;

        char* endc = nullptr;
        int priority = strtol(tokens[tok_idx].c_str(), &endc, 10);
        if ('\0' != *endc) {
            log_msg(oasys::LOG_ERR, 
                    "configure_source_node_priority: SOURCE_PRIORITY has invalid priority: %s",
                    tokens[tok_idx].c_str());
            return false;
        }
        if (priority > 999) priority = 999;
        if (priority < 0) priority = 0;
        
        ++tok_idx;
        
        // parse the nodes list
        uint64_t node_id = 0;
        uint64_t end_node_id = 0;

        std::vector<std::string> node_tokens;

        int node_toks = oasys::tokenize(tokens[tok_idx++].c_str(), ",", &node_tokens);

        for (int ix=0; ix<node_toks; ix++) {
            node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);

            if ('\0' == *endc) {
                src_priority_.set_node_priority(node_id, priority);
            } else if ('-' == *endc) {
                // a range has been specified
                char* end_str = endc + 1;
                end_node_id = strtoull(end_str, &endc, 10);

                if ('\0' == *endc  &&  end_node_id > node_id) {
                    if (end_node_id - node_id > 100) {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_source_node_priority: SOURCE_PRIORITY has too many nodes in range: %s -- aborted", 
                                node_tokens[ix].c_str());
                        break;
                    } else {
                        for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                             src_priority_.set_node_priority(jx, priority);
                        }
                    }
                } else {
                    result = false;
                    log_msg(oasys::LOG_ERR, 
                            "configure_source_node_priority: SOURCE_PRIORITY has invalid range of Node IDs: %s  -- aborted", 
                            node_tokens[ix].c_str());
                    break;
                }
            } else {
                result = false;
                log_msg(oasys::LOG_ERR, 
                        "configure_source_node_priority: SOURCE_PRIORITY has invalid Node ID: %s  -- aborted", 
                        node_tokens[ix].c_str());
                break;
            }
        }

        if (result) {
            oasys::ScopeLock l(&node_map_lock_, __func__);

            // inform each DtnNode to reconfigure the link
            SPtr_EhsDtnNode node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->reconfigure_source_priority();
                ++iter;
            }

            log_msg(oasys::LOG_INFO, 
                        "configure_source_node_priority: SOURCE_PRIORITY -- success");
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_source_node_priority(uint64_t node_id, int priority)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    src_priority_.set_node_priority(node_id, priority);

    // inform each DtnNode to reconfigure the link
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->reconfigure_source_priority();
        ++iter;
    }

    log_msg(oasys::LOG_INFO, 
            "configure_source_node_priority: success");

    return true;
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_dest_node_priority(std::string& val)
{
    bool result = true;

    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    if (toks != 2) {
        result = false;
        log_msg(oasys::LOG_ERR, 
                "configure_dest_node_priority: DEST_PRIORITY has invalid number of parameters: %d",
                toks);
    } else {

        int tok_idx = 0;

        char* endc = nullptr;
        int priority = strtol(tokens[tok_idx].c_str(), &endc, 10);
        if ('\0' != *endc) {
            log_msg(oasys::LOG_ERR, 
                    "configure_dest_node_priority: DEST_PRIORITY has invalid priority: %s",
                    tokens[tok_idx].c_str());
            return false;
        }
        if (priority > 999) priority = 999;
        if (priority < 0) priority = 0;
        
        ++tok_idx;
        
        // parse the nodes list
        uint64_t node_id = 0;
        uint64_t end_node_id = 0;

        std::vector<std::string> node_tokens;

        int node_toks = oasys::tokenize(tokens[tok_idx++].c_str(), ",", &node_tokens);

        for (int ix=0; ix<node_toks; ix++) {
            node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);

            if ('\0' == *endc) {
                dst_priority_.set_node_priority(node_id, priority);
            } else if ('-' == *endc) {
                // a range has been specified
                char* end_str = endc + 1;
                end_node_id = strtoull(end_str, &endc, 10);

                if ('\0' == *endc  &&  end_node_id > node_id) {
                    if (end_node_id - node_id > 100) {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_dest_node_priority: DEST_PRIORITY has too many nodes in range: %s -- aborted", 
                                node_tokens[ix].c_str());
                        break;
                    } else {
                        for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                             dst_priority_.set_node_priority(jx, priority);
                        }
                    }
                } else {
                    result = false;
                    log_msg(oasys::LOG_ERR, 
                            "configure_dest_node_priority: DEST_PRIORITY has invalid range of Node IDs: %s  -- aborted", 
                            node_tokens[ix].c_str());
                    break;
                }
            } else {
                result = false;
                log_msg(oasys::LOG_ERR, 
                        "configure_dest_node_priority: DEST_PRIORITY has invalid Node ID: %s  -- aborted", 
                        node_tokens[ix].c_str());
                break;
            }
        }

        if (result) {
            oasys::ScopeLock l(&node_map_lock_, __func__);

            // inform each DtnNode to reconfigure the link
            SPtr_EhsDtnNode node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->reconfigure_dest_priority();
                ++iter;
            }

            log_msg(oasys::LOG_INFO, 
                    "configure_dest_node_priority: DEST_PRIORITY -- success");
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_dest_node_priority(uint64_t node_id, int priority)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    dst_priority_.set_node_priority(node_id, priority);

    // inform each DtnNode to reconfigure the link
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->reconfigure_dest_priority();
        ++iter;
    }

    log_msg(oasys::LOG_INFO, 
            "configure_dest_node_priority: success");

    return true;
}


//----------------------------------------------------------------------
void
EhsExternalRouterImpl::send_msg(std::string* msg)
{
    oasys::ScopeLock l(&tcp_sender_lock_, __func__);

    tcp_sender_.post(msg);
}


//----------------------------------------------------------------------
int
EhsExternalRouterImpl::process_action(std::unique_ptr<std::string>& msgptr)
{
    CborParser parser;
    CborValue cvMessage;
    CborValue cvElement;
    CborError err;

    err = cbor_parser_init((const uint8_t*) msgptr->c_str(), msgptr->length(), 0, &parser, &cvMessage);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    err = cbor_value_enter_container(&cvMessage, &cvElement);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    uint64_t msg_type;
    uint64_t msg_version;
    std::string server_eid;
    int status = decode_server_msg_header(cvElement, msg_type, msg_version, server_eid);
    CHECK_CBORUTIL_STATUS_RETURN


    SPtr_EhsDtnNode dtn_node = nullptr;

    // all messages should include the EID of the DTNME node
    if (!server_eid.empty()) {
        std::string alert_str;

        if (msg_type == EXTRTR_MSG_ALERT) {
            if (msg_version == 0) {
                decode_alert_msg_v0(cvElement, alert_str);
            }
        }

        dtn_node = get_dtn_node(server_eid, server_eid, alert_str);

        if (nullptr == dtn_node) {
            return CBORUTIL_FAIL;
        }
    } else {
        log_msg(oasys::LOG_ALWAYS, "Received message without server EID");
        return CBORUTIL_FAIL;
    }

    // post the message to the correct DTN node
    dtn_node->post_event(new EhsCborReceivedEvent(msgptr));

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
SPtr_EhsDtnNode
EhsExternalRouterImpl::get_dtn_node(std::string eid, std::string eid_ipn, std::string alert)
{
    SPtr_EhsDtnNode dtn_node;

    oasys::ScopeLock l(&node_map_lock_, __func__);

    EhsDtnNodeIterator iter = dtn_node_map_.find(eid);
    if (iter == dtn_node_map_.end()) {
        // not currently in the list - create one if the alert is not shuttingDown
        if (0 != alert.compare("shuttingDown")) {
            dtn_node = std::make_shared<EhsDtnNode>(this, eid, eid_ipn);
            dtn_node_map_[eid] = dtn_node;

            dtn_node->set_log_level(log_level_);
            dtn_node->set_fwdlnk_force_LOS_while_disabled(fwdlnk_force_LOS_while_disabled_);
            dtn_node->set_fwdlnk_enabled_state(fwdlnk_enabled_);
            dtn_node->set_fwdlnk_aos_state(fwdlnk_aos_);
            dtn_node->reconfigure_max_expiration_fwd(max_expiration_fwd_);
            dtn_node->reconfigure_max_expiration_rtn(max_expiration_rtn_);
            dtn_node->start();

        } else {
            log_msg(oasys::LOG_NOTICE, "DTN node shutting down: %s - was not active", 
                    eid.c_str());
        }
    } else {
        dtn_node = iter->second;
        if (0 == alert.compare("shuttingDown")) {
            log_msg(oasys::LOG_NOTICE, "DTN node shutting down: %s", eid.c_str());
            dtn_node_map_.erase(iter);
            dtn_node->do_shutdown();
            dtn_node = nullptr;
        } else if (0 == alert.compare("justBooted")) {
            log_msg(oasys::LOG_NOTICE, "DTN node just booted: %s - re-initializing",
                    eid.c_str());
            dtn_node->do_shutdown();
            dtn_node_map_.erase(iter);
            dtn_node = nullptr;

            dtn_node = std::make_shared<EhsDtnNode>(this, eid, eid_ipn);
            dtn_node_map_[eid] = dtn_node;

            dtn_node->set_log_level(log_level_);
            dtn_node->set_fwdlnk_force_LOS_while_disabled(fwdlnk_force_LOS_while_disabled_);
            dtn_node->set_fwdlnk_enabled_state(fwdlnk_enabled_);
            dtn_node->set_fwdlnk_aos_state(fwdlnk_aos_);
            dtn_node->reconfigure_max_expiration_fwd(max_expiration_fwd_);
            dtn_node->reconfigure_max_expiration_rtn(max_expiration_rtn_);
            dtn_node->start();

            iter->second = dtn_node;
        }
    }

    return dtn_node;
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::remove_dtn_nodes_after_comm_error()
{
    SPtr_EhsDtnNode dtn_node;

    oasys::ScopeLock l(&node_map_lock_, __func__);

    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        dtn_node = iter->second;
        dtn_node->do_shutdown();
        dtn_node = nullptr;

        ++iter;
    }
    dtn_node_map_.clear();
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::get_link_configuration(EhsLink* el)
{
    bool result = false;

    oasys::ScopeLock l(&config_lock_, __func__);

    EhsLinkCfg* cfglnk;
    EhsLinkCfgIterator cfg_iter = link_configs_.begin();
    while (cfg_iter != link_configs_.end()) {
        cfglnk = cfg_iter->second;

        if ((cfglnk->link_id_.compare(el->link_id()) == 0) ||
            (cfglnk->link_id_.compare(el->remote_addr()) == 0)) {

            el->apply_ehs_cfg(cfglnk);

            result = true;
            break;
        }

        ++cfg_iter;
    }

    return result;
}

//----------------------------------------------------------------------
void 
EhsExternalRouterImpl::get_fwdlink_transmit_enabled_list(EhsSrcDstWildBoolMap& enabled_list)
{
    // copy our list to the passed in list
   enabled_list = fwdlink_xmt_enabled_;
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::get_accept_custody_list(EhsSrcDstWildBoolMap& accept_custody)
{
    accept_custody = accept_custody_;
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::get_source_node_priority(NodePriorityMap& pmap)
{
    pmap = src_priority_;
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::get_dest_node_priority(NodePriorityMap& pmap)
{
    pmap = dst_priority_;
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::configure_link_throttle(std::string& link_id, uint32_t throttle_bps)
{
    bool result = false;
    EhsLinkCfg* cl = nullptr;

    do {
        oasys::ScopeLock l(&config_lock_, __func__);

        EhsLinkCfgIterator find_iter = link_configs_.find(link_id);
        if (find_iter == link_configs_.end()) {
            log_msg(oasys::LOG_ERR, "configure_link_throttle - Link ID not found: %s", link_id.c_str());
        } else {
            cl = find_iter->second;
            cl->throttle_bps_ = throttle_bps;
            result = true;
        }
    } while (false); // limit scope of lock


    // inform each DtnNode to reconfigure the link
    if (cl != nullptr) {
        do {
            oasys::ScopeLock l(&node_map_lock_, __func__);

            SPtr_EhsDtnNode node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->reconfigure_link(link_id);
                ++iter;
            }
        } while (false); // limit scope of lock
    }

    return result;
}


//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_accept_custody(std::string& val)
{
    bool result = true;

    char* endc = nullptr;
    uint64_t src_node_id = 0;
    bool wildcard_source = false;
    bool accept = false;
    bool cleared = false;


    std::vector<std::string> tokens;

    int toks = oasys::tokenize(val.c_str(), "`", &tokens);

    // verify there are two tokens and the first is a valid node ID
    if (1 == toks) {
        // if one token then it must be "clear"
        if (0 == tokens[0].compare("clear")) {
            accept_custody_.clear();
            log_msg(oasys::LOG_DEBUG, 
                    "configure_accept_custody - cleared all configuration");
            cleared = true;
        } else {
            result = false;
            log_msg(oasys::LOG_ERR, 
                    "configure_accept_custody - invalid option: %s", tokens[0].c_str());
        }
    } else if (3 != toks) {
        result = false;
        log_msg(oasys::LOG_ERR, 
                "configure_accept_custody - Two colons required in config string -- abort processing: %s",
                val.c_str());
    } else {
        // parse the first two tokens

        // token[0] must be t[rue] or f[alse] -- really just looking at first char
        if (0 == tokens[0].compare("true")) {
            accept = true;
        } else if (0 == tokens[0].compare("false")) {
            accept = false;
        } else {
            result = false;
            log_msg(oasys::LOG_ERR, 
                    "configure_accept_custody - first token must be true or false: %s",
                    tokens[0].c_str());
        }

        // token[1] is the source and must be a wildcard or a node number
        if (0 == tokens[1].compare("*")) {
            wildcard_source = true;
        } else {
            src_node_id = strtoull(tokens[1].c_str(), &endc, 10);
            if ('\0' != *endc) {
                result = false;
                log_msg(oasys::LOG_ERR, 
                        "configure_accept_custody - Invalid Source Node ID: %s", tokens[0].c_str());
            }
        }
    }

    if (result && !cleared) {
        // parse the destination(s)
        uint64_t node_id = 0;
        uint64_t end_node_id = 0;

        std::vector<std::string> node_tokens;

        toks = oasys::tokenize(tokens[2].c_str(), ",", &node_tokens);

        for (int ix=0; ix<toks; ix++) {
            if (0 == node_tokens[ix].compare("*")) {
                // destination is the wildcard
                if (wildcard_source) {
                    accept_custody_.put_pair_double_wildcards(accept);
                } else {
                    accept_custody_.clear_source(src_node_id);
                    accept_custody_.put_pair_wildcard_dest(src_node_id, accept);
                }
            } else {
                node_id = strtoull(node_tokens[ix].c_str(), &endc, 10);
    
                if ('\0' == *endc) {
                    // destination is a single node ID
                    if (wildcard_source) {
                        accept_custody_.clear_dest(node_id);
                        accept_custody_.put_pair_wildcard_source(node_id, accept);
                    } else {
                        accept_custody_.put_pair(src_node_id, node_id, accept);
                    }

                } else if ('-' == *endc) {
                    // a destination range has been specified
                    char* end_str = endc + 1;
                    end_node_id = strtoull(end_str, &endc, 10);
        
                    if ('\0' == *endc  &&  end_node_id > node_id) {
                        if (end_node_id - node_id > 100) {
                            result = false;
                            log_msg(oasys::LOG_ERR, 
                                    "configure_accept_custody - Too many nodes in range: %s  -- skipping", 
                                    node_tokens[ix].c_str());
                        } else {
                            for (uint64_t jx=node_id; jx<=end_node_id; jx++) {
                                accept_custody_.put_pair(src_node_id, jx, accept);
                            }
                        }
                    } else {
                        result = false;
                        log_msg(oasys::LOG_ERR, 
                                "configure_accept_custody - Invalid Range of Node IDs: %s  -- continuing", 
                                node_tokens[ix].c_str());
                    }
                } else {
                    result = false;
                    log_msg(oasys::LOG_ERR, 
                            "configure_accept_custody - Invalid Node ID: %s  -- continuing", 
                            node_tokens[ix].c_str());
                }
                
            }
        }
    }

    // inform each DtnNode to reconfigure the accept_custody list
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
            node->reconfigure_accept_custody();
        ++iter;
    }

    return result;
}


//----------------------------------------------------------------------
int
EhsExternalRouterImpl::set_fwdlnk_enabled_state(bool state, uint32_t timeout_ms)
{
    (void) timeout_ms;

    oasys::ScopeLock l(&node_map_lock_, __func__);

    fwdlnk_enabled_ = state;

    // inform each DtnNode of the new fwd link state
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->set_fwdlnk_enabled_state(fwdlnk_enabled_);
        ++iter;
    }

    return EHSEXTRTR_SUCCESS;
}

//----------------------------------------------------------------------
int
EhsExternalRouterImpl::set_fwdlnk_aos_state(bool state, uint32_t timeout_ms)
{
    (void) timeout_ms;

    oasys::ScopeLock l(&node_map_lock_, __func__);

    fwdlnk_aos_ = state;

    // inform each DtnNode of the new fwd link state
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->set_fwdlnk_aos_state(fwdlnk_aos_);
        ++iter;
    }

    return EHSEXTRTR_SUCCESS;
}

//----------------------------------------------------------------------
int
EhsExternalRouterImpl::set_fwdlnk_throttle(uint32_t bps, uint32_t timeout_ms)
{
    (void) timeout_ms;

    do {
        oasys::ScopeLock l(&config_lock_, __func__);

        EhsLinkCfg* cfglnk;
        EhsLinkCfgIterator cfg_iter = link_configs_.begin();
        while (cfg_iter != link_configs_.end()) {
            cfglnk = cfg_iter->second;

            if (cfglnk->is_fwdlnk_) {
                cfglnk->throttle_bps_ = bps;
            }

            ++cfg_iter;
        }
    } while (false); // limit scope of lock

    do {
        oasys::ScopeLock l(&node_map_lock_, __func__);

        // inform each DtnNode of the new fwd link rate
        SPtr_EhsDtnNode node;
        EhsDtnNodeIterator iter = dtn_node_map_.begin();
        while (iter != dtn_node_map_.end()) {
            node = iter->second;
            node->set_fwdlnk_throttle(bps);
            ++iter;
        }
    } while (false); // limit scope of lock

    return EHSEXTRTR_SUCCESS;
}

//----------------------------------------------------------------------
int
EhsExternalRouterImpl::bundle_delete(uint64_t source_node_id, uint64_t dest_node_id)
{
    int cnt = 0;
    oasys::ScopeLock l(&node_map_lock_, __func__);
    
    // inform each DtnNode of the new fwd link rate
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        cnt += node->bundle_delete(source_node_id, dest_node_id);
        ++iter;
    }

    return cnt;
}

//----------------------------------------------------------------------
int
EhsExternalRouterImpl::bundle_delete_all()
{
    int cnt = 0;
    oasys::ScopeLock l(&node_map_lock_, __func__);
    
    // inform each DtnNode of the new fwd link rate
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        cnt += node->bundle_delete_all();
        ++iter;
    }

    return cnt;
}

//----------------------------------------------------------------------
const char* 
EhsExternalRouterImpl::update_statistics()
{
    static char buf[256];

    int nodes = num_dtn_nodes();
    if (0 == nodes) {
        snprintf(buf, sizeof(buf), "Listening...");
    } else {
        
        oasys::ScopeLock l(&node_map_lock_, __func__);

        uint64_t received = 0;
        uint64_t transmitted = 0;
        uint64_t transmit_failed = 0;
        uint64_t delivered = 0;
        uint64_t rejected = 0;
        uint64_t pending = 0;
        uint64_t custody = 0;
        bool fwdlnk_aos = false;
        bool fwdlnk_enabled = false;

        SPtr_EhsDtnNode node;
        EhsDtnNodeIterator iter = dtn_node_map_.begin();
        while (iter != dtn_node_map_.end()) {
            node = iter->second;
            node->update_statistics(received, transmitted, transmit_failed, delivered, rejected,
                                    pending, custody, fwdlnk_aos, fwdlnk_enabled);
            ++iter;
        }

        snprintf(buf, sizeof(buf), 
                 "Nodes: %d (%s/%s) Bundles Rcv: %" PRIu64 " Xmt: %" PRIu64 " XFail: %" PRIu64 " Dlv: %" PRIu64  
                 " Rjct: %" PRIu64 " Pend: %" PRIu64 " Cust: %" PRIu64 "     ", 
                 nodes, fwdlnk_enabled?"enabled":"disabled", fwdlnk_aos?"AOS":"LOS", 
                 received, transmitted, transmit_failed, delivered, rejected, pending, custody);
    }

    return (const char*) buf;
}

//----------------------------------------------------------------------
const char* 
EhsExternalRouterImpl::update_statistics2()
{
    static char buf[256];

    int nodes = num_dtn_nodes();
    if (0 == nodes) {
        snprintf(buf, sizeof(buf), "Listening...");
    } else {
        
        oasys::ScopeLock l(&node_map_lock_, __func__);

        uint64_t received = 0;
        uint64_t transmitted = 0;
        uint64_t transmit_failed = 0;
        uint64_t delivered = 0;
        uint64_t expired = 0;
        uint64_t rejected = 0;
        uint64_t pending = 0;
        uint64_t custody = 0;
        bool fwdlnk_aos = false;
        bool fwdlnk_enabled = false;

        SPtr_EhsDtnNode node;
        EhsDtnNodeIterator iter = dtn_node_map_.begin();
        while (iter != dtn_node_map_.end()) {
            node = iter->second;
            node->update_statistics2(received, transmitted, transmit_failed, delivered, rejected,
                                    pending, custody, expired, fwdlnk_aos, fwdlnk_enabled);
            ++iter;
        }

        snprintf(buf, sizeof(buf), 
                 "Nodes: %d (%s/%s) Bundles Rcv: %" PRIu64 " Xmt: %" PRIu64 " XFail: %" PRIu64 " Dlv: %" PRIu64  
                 " Expd: %" PRIu64 " Rjct: %" PRIu64 " Pend: %" PRIu64 " Cust: %" PRIu64 "     ", 
                 nodes, fwdlnk_enabled?"enabled":"disabled", fwdlnk_aos?"AOS":"LOS", 
                 received, transmitted, transmit_failed, delivered, expired, rejected, pending, custody);
    }

    return (const char*) buf;
}

//----------------------------------------------------------------------
const char* 
EhsExternalRouterImpl::update_statistics3()
{
    static char buf[256];

    int nodes = num_dtn_nodes();
    if (0 == nodes) {
        snprintf(buf, sizeof(buf), "Listening...");
    } else {
        
        oasys::ScopeLock l(&node_map_lock_, __func__);

        uint64_t received = 0;
        uint64_t transmitted = 0;
        uint64_t transmit_failed = 0;
        uint64_t delivered = 0;
        uint64_t expired = 0;
        uint64_t rejected = 0;
        uint64_t pending = 0;
        uint64_t custody = 0;
        bool fwdlnk_aos = false;
        bool fwdlnk_enabled = false;
        uint64_t links_open = 0;
        uint64_t num_links = 0;

        SPtr_EhsDtnNode node;
        EhsDtnNodeIterator iter = dtn_node_map_.begin();
        while (iter != dtn_node_map_.end()) {
            node = iter->second;
            node->update_statistics3(received, transmitted, transmit_failed, delivered, rejected,
                                    pending, custody, expired, fwdlnk_aos, fwdlnk_enabled, links_open,
                                    num_links);
            ++iter;
        }

        snprintf(buf, sizeof(buf), 
                 "Nodes: %d (%s/%s) Links: %" PRIu64 "/%" PRIu64 
                 " Bundles Rcv: %" PRIu64 " Xmt: %" PRIu64 " XFail: %" PRIu64 " Dlv: %" PRIu64  
                 " Expd: %" PRIu64 " Rjct: %" PRIu64 " Cust: %" PRIu64 " Pend: %" PRIu64 "     " ,
                 nodes, fwdlnk_enabled?"enabled":"disabled", fwdlnk_aos?"AOS":"LOS", 
                 links_open, num_links,
                 received, transmitted, transmit_failed, delivered, expired, rejected, custody, pending );
    }

    return (const char*) buf;
}

//----------------------------------------------------------------------
int
EhsExternalRouterImpl::num_dtn_nodes()
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    return dtn_node_map_.size();
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::set_link_statistics(bool enabled)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->set_link_statistics(enabled);
        ++iter;
    }
}


//----------------------------------------------------------------------
void
EhsExternalRouterImpl::bundle_list(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    int cnt = 0;
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;

        buf->appendf("Bundle list for Node: %s\n", iter->first.c_str());
        node->bundle_list(buf);
        buf->append("\n\n");

        ++iter;
        ++cnt;
    }

    if (0 == cnt) {
        buf->appendf("Bundle list: No DTN Nodes found yet\n\n");
    }
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::bundle_stats_by_src_dst(int* count, EhsBundleStats** stats)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->bundle_stats_by_src_dst(count, stats);
        ++iter;
    }
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::bundle_stats_by_src_dst_free(int count, EhsBundleStats** stats)
{
    (void) count;

    // currently *stats is a single block of memory
    if (stats != nullptr && *stats != nullptr) {
        free(*stats);
        *stats = nullptr;
    }
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->fwdlink_interval_stats(count, stats);
        ++iter;
    }
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::fwdlink_interval_stats_free(int count, EhsFwdLinkIntervalStats** stats)
{
    (void) count;

    // currently *stats is a single block of memory
    if (stats != nullptr && *stats != nullptr) {
        free(*stats);
        *stats = nullptr;
    }
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::request_bard_usage_stats()
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    if (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->request_bard_usage_stats();
    }
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::bard_usage_stats(EhsBARDUsageStatsVector& usage_stats, EhsRestageCLStatsVector& cl_stats)
{
    bool result = false;

    oasys::ScopeLock l(&node_map_lock_, __func__);

    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    if (iter != dtn_node_map_.end()) {
        node = iter->second;
        result = node->bard_usage_stats(usage_stats, cl_stats);
    }

    return result;
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::bard_add_quota(EhsBARDUsageStats& quota)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    if (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->bard_add_quota(quota);
    }
}


//----------------------------------------------------------------------
void
EhsExternalRouterImpl::bard_del_quota(EhsBARDUsageStats& quota)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    if (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->bard_del_quota(quota);
    }
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::send_link_add_msg(std::string& link_id, std::string& next_hop, std::string& link_mode,
                                         std::string& cl_name,  LinkParametersVector& params)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    if (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->send_link_add_msg(link_id, next_hop, link_mode, cl_name, params);
    }
}


//----------------------------------------------------------------------
void
EhsExternalRouterImpl::send_link_del_msg(std::string& link_id)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    if (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->send_link_del_msg(link_id);
    }
}






//----------------------------------------------------------------------
void
EhsExternalRouterImpl::unrouted_bundle_stats_by_src_dst(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    int cnt = 0;
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;

        buf->appendf("Unrouted bundle stats by src-dst for Node: %s\n", iter->first.c_str());
        node->unrouted_bundle_stats_by_src_dst(buf);
        buf->append("\n\n");

        ++iter;
        ++cnt;
    }

    if (0 == cnt) {
        buf->appendf("Unrouted bundle stats: No DTN Nodes found yet\n\n");
    }
}


//----------------------------------------------------------------------
void
EhsExternalRouterImpl::bundle_stats(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    int cnt = 0;
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;

        buf->appendf("\nBundle Statistics for %s:\n", iter->first.c_str());
        node->bundle_stats(buf);
        buf->append("\n");

        ++iter;
        ++cnt;
    }

    if (0 == cnt) {
        buf->appendf("Bundle stats: No DTN Nodes found yet\n\n");
    }
}


//----------------------------------------------------------------------
void
EhsExternalRouterImpl::link_dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    int cnt = 0;
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;

        buf->appendf("Link dump for Node: %s\n", iter->first.c_str());
        node->link_dump(buf);
        buf->append("\n\n");

        ++iter;
        ++cnt;
    }

    if (0 == cnt) {
        buf->appendf("Link dump: No DTN Nodes found yet\n\n");
    }
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::fwdlink_transmit_dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&node_map_lock_, __func__);

    int cnt = 0;
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;

        buf->appendf("FwdLink Transmit dump for Node: %s\n", iter->first.c_str());
        node->fwdlink_transmit_dump(buf);
        buf->append("\n\n");

        ++iter;
        ++cnt;
    }

    if (0 == cnt) {
        buf->appendf("FwdLink Transmit dump: No DTN Nodes found yet\n\n");
    }
}

//----------------------------------------------------------------------
uint64_t
EhsExternalRouterImpl::max_bytes_sent()
{
    oasys::ScopeLock l(&tcp_sender_lock_, __func__);

    return tcp_sender_.max_bytes_sent();
}

//----------------------------------------------------------------------
void 
EhsExternalRouterImpl::set_log_level(int level)
{
    log_level_ = level;

    // update all of the DtnNodes
    SPtr_EhsDtnNode node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->set_log_level(log_level_);
        ++iter;
    }
}

//----------------------------------------------------------------------
void 
EhsExternalRouterImpl::log_msg(const std::string& path, oasys::log_level_t level, const char*format, ...)
{
    if ((int)level >= log_level_) {
        if (nullptr != log_callback_) {
            va_list args;
            va_start(args, format);
            log_msg_va(path, level, format, args);
            va_end(args);
        }
    }
}

//----------------------------------------------------------------------
void 
EhsExternalRouterImpl::log_msg_va(const std::string& path, oasys::log_level_t level, const char*format, va_list args)
{
    if ((int)level >= log_level_) {
        if (nullptr != log_callback_) {
            oasys::ScopeLock l(&log_lock_, __func__);

            static char buf[2048];
            vsnprintf(buf, sizeof(buf), format, args);

            std::string msg(buf);

            log_callback_(path, (int)level, msg);
        }
    }
}

//----------------------------------------------------------------------
void 
EhsExternalRouterImpl::log_msg(const std::string& path, oasys::log_level_t level, std::string& msg)
{
    if ((int)level >= log_level_) {
        if (nullptr != log_callback_) {
            oasys::ScopeLock l(&log_lock_, __func__);

            log_callback_(path, (int)level, msg);
        }
    }
}

//----------------------------------------------------------------------
void 
EhsExternalRouterImpl::log_msg(oasys::log_level_t level, const char*format, ...)
{
    // internal use only - passes the logpath_ of this object
    if ((int)level >= log_level_) {
        if (nullptr != log_callback_) {
            va_list args;
            va_start(args, format);
            log_msg_va(logpath_, level, format, args);
            va_end(args);
        }
    }
}



//----------------------------------------------------------------------
void
EhsExternalRouterImpl::run() 
{
    char threadname[16] = "EhsExtRtrImpTcp";
    pthread_setname_np(pthread_self(), threadname);

    oasys::ScratchBuffer<u_char*, 2048> buffer;

    union US { 
       char       chars[4];
       int32_t    int_size;
    };   

    US union_size;


    // initialize the TCP client
    tcp_client_.set_logfd(false);
    tcp_client_.init_socket();

    tcp_sender_.start();

    while (!should_stop())
    {
        // try to connect if needed
        if (tcp_client_.state() != oasys::IPSocket::ESTABLISHED) {
            if (tcp_client_.connect(remote_addr_, remote_port_) != 0) {
                log_err("Unable to connect to External Router TCP interface (%s:%u)... will try again in 10 seconds",
                        intoa(remote_addr_) , remote_port_);
                int ctr = 0;
                while (!should_stop() && ++ctr <= 100)
                {
                    usleep(100000);
                }
            }
            else
            {
                // give connection time to complete
                usleep(500000);
            }
            continue;
        }

        log_always("Connected to DTNME server - state = %d", (int)tcp_client_.state());

        // Send the ExtneralRouter client-side magic number first thing
        union_size.int_size = 0x58434C54;  // XCLT
        union_size.int_size = ntohl(union_size.int_size);
        int cc = tcp_client_.writeall(union_size.chars, 4);
        if (cc != 4) {
            log_err("error sending ExternalRouter client-side magic number: %s", strerror(errno));
        }

        bool got_magic_number = false;

        while (!should_stop() && (tcp_client_.state() == oasys::IPSocket::ESTABLISHED)) {
            // read the length field (4 bytes)

            //int cc = tcp_client_.readall(union_size.chars, 4);
            int ret;

            int data_to_read = 4;       
            //
            // get the size off the wire and then read the data
            //
            int data_ptr     = 0;
            union_size.int_size = -1;
            while (!should_stop() && (data_to_read > 0))
            {
                ret = tcp_client_.timeout_read((char *) &(union_size.chars[data_ptr]), data_to_read, 100);
                if ((ret == oasys::IOEOF) || (ret == oasys::IOERROR)) {   
//                    if (errno == EINTR) {
//                        continue;
//                    }
                    tcp_client_.close();
                    //set_should_stop();
                    break;
                }
                if (ret > 0) {
                    data_ptr     += ret;
                    data_to_read -= ret;
                }
            }


            if (data_to_read > 0) continue; 
            if (should_stop()) continue;

            union_size.int_size = ntohl(union_size.int_size);

            if (!got_magic_number) {
                // first 4 bytes read must be the client side magic number or we abort
                if (union_size.int_size != 0x58525452) {   // XRTR
                    log_err("Aborting - did not receive ExternalRouter server-side magic number: %8.8x", union_size.int_size);
                    tcp_client_.close();
                    break;
                }
                got_magic_number = true;

                //dzdebug
                log_always("EhsExternalRouterImpl got magic number from server");

                continue; // now try to read a size value
            }

            // allow up to 10MB of data from the server (bundle report segments)
            if ((union_size.int_size < 0) || (union_size.int_size > 10000000)) {
                log_crit("Error reading TCP socket - expected message length = %d", union_size.int_size);
                tcp_client_.close();
                continue;
            }

            buffer.clear();
            buffer.reserve(union_size.int_size);


            data_ptr = 0;
            data_to_read = union_size.int_size;
            while (!should_stop() && (data_to_read > 0)) {
                ret = tcp_client_.timeout_read((char*) buffer.buf()+data_ptr, data_to_read, 100);
                if ((ret == oasys::IOEOF) || (ret == oasys::IOERROR)) {   
                    tcp_client_.close();
                    break;
                }
                if (ret > 0) {
                    buffer.incr_len(ret);
                    data_ptr     += ret;
                    data_to_read -= ret;
                }
            } 


            if (data_to_read > 0) continue; 
            if (should_stop()) continue;

            // process the message
            //dzdebug log_debug("Received message of length: %d", union_size.int_size);

            std::unique_ptr<std::string> msgptr(new std::string((const char*) buffer.buf(), union_size.int_size));

            process_action(msgptr);
        }

        remove_dtn_nodes_after_comm_error();


    }

    tcp_sender_.set_should_stop();
}

EhsExternalRouterImpl::TcpSender::TcpSender()
    : Thread("/ehs/extrtr/tcpsndr"),
      Logger("EhsExternalRouterImpl::TcpSender", "/ehs/extrtr/tcpsndr"),
      eventq_(logpath_)
{
}

EhsExternalRouterImpl::TcpSender::~TcpSender()
{
    // free all pending events
    std::string *event;
    while (eventq_.try_pop(&event))
        delete event;
}

void
EhsExternalRouterImpl::TcpSender::post(std::string* event)
{
    if (should_stop()) {
        delete event;
    } else {
        eventq_.push_back(event);
    }
}

void
EhsExternalRouterImpl::TcpSender::run() 
{
     
    log_always("EhsExternalRouterImpl::TcpSender::run - started");

    // block on input from the socket and
    // on input from the bundle event list
    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq_.read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    while (!should_stop()) {

        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 10);

        if (should_stop()) {
            break;
        }

        if (ret == oasys::IOINTR) {
            log_debug("TcpSender interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_debug("TcpSender error");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            std::string *event;
            if (eventq_.try_pop(&event)) {
                ASSERT(event != nullptr)

                parent_->send_tcp_msg(event);

                delete event;
            }
        }
    }
}

void
EhsExternalRouterImpl::send_tcp_msg(std::string* msg)
{
    union US { 
       char       chars[4];
       int32_t    int_size;
    };   

    US union_size;

    int cc;

    if (tcp_client_.state() == oasys::IPSocket::ESTABLISHED) {
        union_size.int_size = htonl(msg->size());

        cc = tcp_client_.writeall(union_size.chars, 4);
        if (cc != 4) {
            log_err("error writing msg: %s", strerror(errno));
        } else {
            cc = tcp_client_.writeall( const_cast< char * >(msg->c_str()), msg->size());
            if (cc != (int) msg->size())
            {
                log_err("error writing msg: %s", strerror(errno));
            }
        }
    }
}

uint64_t
EhsExternalRouterImpl::TcpSender::max_bytes_sent()
{
    return 0;
}

} // namespace dtn

