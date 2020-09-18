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

#ifdef EHSROUTER_ENABLED

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <inttypes.h>
#include <sys/ioctl.h>
#include <net/if.h>


#include <oasys/debug/Log.h>
#include <oasys/io/FileUtils.h>
#include <oasys/io/NetUtils.h>
#include <oasys/io/UDPClient.h>
#include <oasys/util/StringUtils.h>

#include "EhsDtnNode.h"
#include "EhsExternalRouterImpl.h"

#define SEND(event, data, dtn_node) \
    rtrmessage::bpa message; \
    message.event(data); \
    send_msg(message, dtn_node->eid());

// in case a method needs to send multiple messages
#define SEND2(event, data, dtn_node) \
    rtrmessage::bpa message2; \
    message2.event(data); \
    send_msg(message2, dtn_node->eid());

// in case a method needs to send multiple messages
#define SEND3(event, data, dtn_node) \
    rtrmessage::bpa message3; \
    message3.event(data); \
    send_msg(message3, dtn_node->eid());

#define CATCH(exception) \
    catch (exception &e) { log_msg(oasys::LOG_WARN, "%s", e.what()); }

namespace dtn {


//----------------------------------------------------------------------
EhsExternalRouterImpl::EhsExternalRouterImpl(LOG_CALLBACK_FUNC log_callback, int log_level, bool oasys_app)
    : IOHandlerBase(new oasys::Notifier("/ehs/extrtr")),
      Thread("/ehs/extrtr", oasys::Thread::DELETE_ON_EXIT),
      log_callback_(log_callback),
      log_level_(log_level),
      oasys_app_(oasys_app)
{
    set_logpath("/ehs/extrtr");

    init();
}

//----------------------------------------------------------------------
EhsExternalRouterImpl::~EhsExternalRouterImpl()
{
    log_msg(oasys::LOG_INFO, "Destroying EhsExternalRouterImpl");

    if (NULL != sender_) {
        sender_->set_should_stop();
    }

    do {
        // limit scope of lock
        oasys::ScopeLock l(&lock_, __FUNCTION__);

        if (NULL != tcp_sender_) {
            tcp_sender_->set_should_stop();
        }
        usleep(300000);
        if (NULL != tcp_client_) {
            tcp_client_->close();
            delete tcp_client_;
        }
    } while (false);

    // free list of DTN nodes
    EhsDtnNode* node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        dtn_node_map_.erase(iter);
        node->set_should_stop();

        iter = dtn_node_map_.begin();
    }

    EhsLinkCfgIterator cliter = link_configs_.begin();
    while (cliter != link_configs_.end()) {
        EhsLinkCfg* cl = cliter->second;
        link_configs_.erase(cliter);
        delete cl;

        cliter = link_configs_.begin();
    }

    fwdlink_xmt_enabled_.clear();

    sleep(1);

    // free all pending events
    std::string *event;
    while (eventq->try_pop(&event))
        delete event;

    delete eventq;

    sleep(1);

    delete parser_;

    // free the link config objects
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::init() 
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    // start out in LOS state until informed we can start transmitting
    fwdlnk_aos_ = false;
    fwdlnk_enabled_ = false;
    fwdlnk_force_LOS_while_disabled_ = true;

    max_expiration_fwd_ = 8 * 60 * 60; // 8 hours worth of seconds
    max_expiration_rtn_ = 48 * 60 * 60; // 48 hours worth of seconds

    // intialize socket parameters
    sender_                 = NULL;
    use_tcp_interface_      = false;
    tcp_client_             = NULL;

    // listen for the ALLRTRS group (224.0.0.2) on the loopback interface only
    local_addr_ = htonl(INADDR_LOOPBACK);
    remote_addr_ = htonl(INADDR_ALLRTRS_GROUP);

    // Initialize EhsExternalRouterImpl parameters
    local_port_             = 8001;
    remote_port_            = 8001;
    schema_                 = "/etc/router.xsd";
    server_validation_      = false;
    client_validation_      = false;
    parser_                 = NULL;

    // Default accepting custody of bundles to true
    accept_custody_.put_pair_double_wildcards(true);

    //set_logfd(false);

    eventq = new oasys::MsgQueue< std::string * >(logpath_);

    send_seq_ctr_ = 0;
    bytes_recv_ = 0;
    max_bytes_recv_ = 0;
}


//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_use_tcp_interface(std::string& val)
{
    bool result = true;
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    if ((0 == val.compare("true")) || (0 == val.compare("TRUE"))) {
        use_tcp_interface_ = true;
    } else if ((0 == val.compare("false")) || (0 == val.compare("FALSE"))) {
        use_tcp_interface_ = false;
    } else  {
        use_tcp_interface_ = false;
        result = false;
    } 

    return result;
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_mc_address(std::string& val)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    return (0 == oasys::gethostbyname(val.c_str(), &remote_addr_));
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_mc_port(std::string& val)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    uint32_t port = strtoul(val.c_str(), NULL, 10);
    bool result = (port > 0 && port < 65536);
    if (result) {
        remote_port_ = port;
        local_port_ = port;
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
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    bool result = true;
    if (0 == val.compare("any")) {
        local_addr_ = htonl(INADDR_ANY);
    } else if (0 == val.compare("lo")) {
        local_addr_ = htonl(INADDR_LOOPBACK);
    } else if (0 == val.compare("localhost")) {
        local_addr_ = htonl(INADDR_LOOPBACK);
    } else if (val.find("eth") != std::string::npos) {
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, val.c_str(), IFNAMSIZ-1);
        if (ioctl(fd, SIOCGIFADDR, &ifr)) {
            result = false;
            log_msg(oasys::LOG_ERR, 
                    "configure_network_interface - Error in ioctl for interface name: %s", 
                    ifr.ifr_name);
        } else {
            //dzdebug local_addr_ = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
            struct sockaddr_in *sin = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
            local_addr_ = sin->sin_addr.s_addr;
        }
    } else {
        // Assume this is the IP address of one of our interfaces?
        if (0 != oasys::gethostbyname(val.c_str(), &local_addr_)) {
            result = false;
            log_msg(oasys::LOG_ERR, 
                    "configure_network_interface - Error in gethostbyname for %s", 
                    val.c_str());
        }
    }

    log_msg(oasys::LOG_INFO, "configure_network_interface: %s", intoa(local_addr_));

    return result;
}

//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_schema_file(std::string& val)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    bool result = oasys::FileUtils::readable(val.c_str());
    if (result) {
        schema_ = val;
    } else {
        log_msg(oasys::LOG_ERR, 
                "configure_schema_file - Invalid filename or access rights: %s", 
                val.c_str());
    }
    return result;
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
        char* endc = NULL;
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
            oasys::ScopeLock l(&lock_, __FUNCTION__);

            EhsLinkCfgIterator find_iter = link_configs_.find(link_id);
            if (find_iter != link_configs_.end()) {
                // replace the config object
                delete find_iter->second;
                find_iter->second = cl;
            } else {
                link_configs_[link_id] = cl;
            }

            // inform each DtnNode to reconfigure the link
            EhsDtnNode* node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->reconfigure_link(link_id);
                ++iter;
            }

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
        char* endc = NULL;

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
            oasys::ScopeLock l(&lock_, __FUNCTION__);

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
 
            // inform each DtnNode to reconfigure the fwdlink transmit
            EhsDtnNode* node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->fwdlink_transmit_enable(tmp_src_list, tmp_dst_list);
                ++iter;
            }

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
        char* endc = NULL;

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
            oasys::ScopeLock l(&lock_, __FUNCTION__);

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
 
            // inform each DtnNode to reconfigure the fwdlink transmit
            EhsDtnNode* node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->fwdlink_transmit_disable(tmp_src_list, tmp_dst_list);
                ++iter;
            }

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

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    // inform each DtnNode to reconfigure 
    EhsDtnNode* node;
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

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    // inform each DtnNode to shutdown
    EhsDtnNode* node;
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

        
        lock_.lock(__FUNCTION__);
        EhsLinkCfgIterator find_iter = link_configs_.find(link_id);
        lock_.unlock();

        if (find_iter == link_configs_.end()) {
            // load a temp EhsLinkCfg until we know all data is good
            cl = new EhsLinkCfg(link_id);
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
        char* endc = NULL;

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
                                "configure_link_enable: LINK_ENABLE (%s) has too many nodes in source range: %s -- aborted", 
                                link_id.c_str(), node_tokens[ix].c_str());
                        break;
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
                        if (end_node_id - node_id > 100) {
                            result = false;
                            log_msg(oasys::LOG_ERR, 
                                    "configure_link_enable: LINK_ENABLE (%s) has too many nodes in dest range: %s -- aborted", 
                                    link_id.c_str(), node_tokens[ix].c_str());
                            break;
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

        if (result) {
            oasys::ScopeLock l(&lock_, __FUNCTION__);

            if (find_iter == link_configs_.end()) {
                link_configs_[link_id] = cl;

                log_msg(oasys::LOG_INFO, "configure_link_enable: LINK_ENABLE added Link ID: %s - success",
                        link_id.c_str());
            }


            // inform each DtnNode to reconfigure the link
            EhsDtnNode* node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->link_enable(cl);
                ++iter;
            }

            log_msg(oasys::LOG_DEBUG, "configure_link_enable: LINK_ENABLE Link ID: %s - success",
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

        
        lock_.lock(__FUNCTION__);
        EhsLinkCfgIterator find_iter = link_configs_.find(link_id);

        if (find_iter != link_configs_.end()) {
            EhsLinkCfg* cl = find_iter->second;
            link_configs_.erase(find_iter);
            delete cl;

            oasys::ScopeLock l(&lock_, __FUNCTION__);
    
            // inform each DtnNode to close and delete the link
            EhsDtnNode* node;
            EhsDtnNodeIterator iter = dtn_node_map_.begin();
            while (iter != dtn_node_map_.end()) {
                node = iter->second;
                node->link_disable(link_id);
                ++iter;
            }

            log_msg(oasys::LOG_INFO, 
                    "configure_link_disable: LINK_DISABLE Link ID (%s) disabled",
                    link_id.c_str());
        } else {
            log_msg(oasys::LOG_DEBUG, 
                    "configure_link_disable: LINK_DISABLE Link ID not found: %s",
                    link_id.c_str());
        }
        lock_.unlock();
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

        char* endc = NULL;
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
            oasys::ScopeLock l(&lock_, __FUNCTION__);

            // inform each DtnNode to reconfigure the link
            EhsDtnNode* node;
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

        char* endc = NULL;
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
            oasys::ScopeLock l(&lock_, __FUNCTION__);

            // inform each DtnNode to reconfigure the link
            EhsDtnNode* node;
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

        char* endc = NULL;
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
            oasys::ScopeLock l(&lock_, __FUNCTION__);

            // inform each DtnNode to reconfigure the link
            EhsDtnNode* node;
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
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    src_priority_.set_node_priority(node_id, priority);

    // inform each DtnNode to reconfigure the link
    EhsDtnNode* node;
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

        char* endc = NULL;
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
            oasys::ScopeLock l(&lock_, __FUNCTION__);

            // inform each DtnNode to reconfigure the link
            EhsDtnNode* node;
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
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    dst_priority_.set_node_priority(node_id, priority);

    // inform each DtnNode to reconfigure the link
    EhsDtnNode* node;
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
bool
EhsExternalRouterImpl::open_socket() 
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    params_.multicast_ = true;
    params_.recv_bufsize_ = 10240000;
//    params_.send_bufsize_ = 1024000;
    if (fd() == -1) {
        init_socket();
    }

    // if still no socket then there was an error
    if (fd() == -1) {
        log_msg(oasys::LOG_ERR, "Error intializing socket");
        return false;
    }


    // Check the buffer sizes
    uint32_t rcv_size = 0;
    uint32_t snd_size = 0;
    socklen_t len = sizeof(rcv_size);
    if (::getsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &rcv_size, &len) != 0) {
        log_msg(oasys::LOG_ERR, "Error reading socket receive buffer size");
    }
    if (::getsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &snd_size, &len) != 0) {
        log_msg(oasys::LOG_ERR, "Error reading socket send buffer size");
    }
    log_msg(oasys::LOG_NOTICE, "EhsExternalRouterImpl - Receiver Socket Buffers: rcv: %u snd: %u",
            rcv_size, snd_size);

    if (bind(remote_addr_, local_port_)) {
        log_msg(oasys::LOG_ERR, "Error binding socket to port");
        return false;
    }

    return true;
}



//----------------------------------------------------------------------
void
EhsExternalRouterImpl::run() 
{
    if (use_tcp_interface_)
    {
        run_tcp();
    }
    else
    {
        run_multicast();
    }
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::run_multicast() 
{
    log_msg(oasys::LOG_INFO, "EhsExternalRouterImpl running multicast on network_interface: %s", intoa(local_addr_));

    // Create the XML parser
    parser_ = new oasys::XercesXMLUnmarshal(client_validation_,
                                            schema_.c_str());


    // variables used to read in UDP packets
    char recv_buf[MAX_UDP_PACKET+8];
    in_addr_t raddr;
    u_int16_t rport;
    int bytes;


    sender_ = new Sender(this, params_, local_addr_, remote_addr_, remote_port_);
    sender_->start();


    if (open_socket()) {
        // block on input from the socket and
        // on input from the bundle event list
        struct pollfd pollfds[1];

        struct pollfd* sock_poll = &pollfds[0];
        sock_poll->fd = fd();
        sock_poll->events = POLLIN;
        sock_poll->revents = 0;

        bool first_receive = true;
        while (!should_stop()) {
            int ret = oasys::IO::poll_multiple(pollfds, 1, 1000,
                get_notifier());

            if (ret == oasys::IOTIMEOUT) {
                continue;
            }

            if (ret == oasys::IOINTR) {
                log_msg(oasys::LOG_DEBUG, "poll interrupted - abort");
                set_should_stop();
                continue;
            }

            if (ret == oasys::IOERROR) {
                log_msg(oasys::LOG_DEBUG, "poll error - abort");
                set_should_stop();
                continue;
            }

            if (sock_poll->revents & POLLIN) {
                if (first_receive) {
                    first_receive = false;
                    recv_timer_.get_time();
                }

                bytes = recvfrom(recv_buf, MAX_UDP_PACKET, 0, &raddr, &rport);
                recv_buf[bytes] = '\0';

                bytes_recv_ += bytes;

                //dz debug
                //log_msg(oasys::LOG_DEBUG, "received %d bytes from %s:%u:", 
                //        bytes, intoa(raddr), rport);
                //log_msg(oasys::LOG_CRIT, "ExternalRouter Msg: %s", recv_buf);

                process_action(recv_buf);
            }

            if (!first_receive && recv_timer_.elapsed_ms() >= 1000) {
                recv_timer_.get_time();
                if (bytes_recv_ > max_bytes_recv_) max_bytes_recv_ = bytes_recv_;
                bytes_recv_ = 0;
            }
        }
    }
}


//----------------------------------------------------------------------
void
EhsExternalRouterImpl::send_msg(rtrmessage::bpa &message, const std::string* server_eid)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    xercesc::MemBufFormatTarget buf;
    xml_schema::namespace_infomap map;

    // Identify the DTN node for which this message is intended
    message.server_eid(server_eid->c_str());

    if (use_tcp_interface_) {
        // only one destination supported for TCP so set seq ctr at this funnel point
        message.sequence_ctr(send_seq_ctr_++);
    } else {
        // UDP can theoretically support multiple nodes so sequence is maintained
        // in the EHSDtnNode and since it is multi-threaded it is possible for
        // those to go through out of order. Review log messages carefully on the DTNME server
        // to determine if messages are just received out of order or getting dropped.
    }

    if (server_validation_)
        map[""].schema = schema_.c_str();

    try {
        bpa_(buf, message, map, "UTF-8",
             xml_schema::flags::dont_initialize);
         if (!should_stop()) {
             if (use_tcp_interface_) {
                 tcp_sender_->post(new std::string((char *)buf.getRawBuffer()));
             } else {
                 sender_->eventq->push_back(new std::string((char *)buf.getRawBuffer()));
             }
         }
    }
    catch (xml_schema::serialization &e) {
        const xml_schema::errors &elist = e.errors();
        xml_schema::errors::const_iterator i = elist.begin();
        xml_schema::errors::const_iterator end = elist.end();

        for (; i < end; ++i) {
            std::cout << (*i).message() << std::endl;
        }
    }
    CATCH(xml_schema::unexpected_element)
    CATCH(xml_schema::no_namespace_mapping)
    CATCH(xml_schema::no_prefix_mapping)
    CATCH(xml_schema::xsi_already_in_use)
}




//----------------------------------------------------------------------
void
EhsExternalRouterImpl::process_action(const char *payload)
{
    // clear any error condition before next parse
    parser_->reset_error();
    
    // parse the xml payload received
    const xercesc::DOMDocument *doc = parser_->doc(payload);

    // was the message valid?
    if (parser_->error()) {
        log_msg(oasys::LOG_WARN, "process_action - received invalid message");
        return;
    }

    std::unique_ptr<rtrmessage::bpa> instance;

    try {
        instance = rtrmessage::bpa_(*doc);
    }
    CATCH(xml_schema::expected_element)
    CATCH(xml_schema::unexpected_element)
    CATCH(xml_schema::expected_attribute)
    CATCH(xml_schema::unexpected_enumerator)
    CATCH(xml_schema::no_type_info)
    CATCH(xml_schema::not_derived)

    rtrmessage::bpa* in_bpa = instance.get();

    // Check that we have an instance object to work with
    if (NULL == in_bpa) {
        log_msg(oasys::LOG_WARN, "process_action - no object extracted from message");
        return;
    }

    EhsDtnNode* dtn_node = NULL;

    // all messages should include the EID of the DTNME node
    if (in_bpa->eid().present()) {

        // configure for this EID if not already done 
        std::string eid_str = in_bpa->eid().get();
        std::string eid_ipn_str = in_bpa->eid_ipn().get();
        std::string alert_str;
        if (in_bpa->alert().present()) {
            alert_str = in_bpa->alert().get().c_str();
            // the only other alert is "shuttingDown" as a standalone message
        }

        dtn_node = get_dtn_node(eid_str, eid_ipn_str, alert_str);

        if (NULL == dtn_node) {
            return;
        }
    } else {
        //log_msg(oasys::LOG_DEBUG, "Received message without EID - probably a loopback: %s", payload);
        return;
    }

    // post the message to the correct DTN node
    dtn_node->post_event(new EhsBpaReceivedEvent(instance));
}

//----------------------------------------------------------------------
EhsDtnNode*
EhsExternalRouterImpl::get_dtn_node(std::string eid, std::string eid_ipn, std::string alert)
{
    EhsDtnNode* dtn_node = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsDtnNodeIterator iter = dtn_node_map_.find(eid);
    if (iter == dtn_node_map_.end()) {
        // not currently in the list - create one if the alert is not shuttingDown
        if (0 != alert.compare("shuttingDown")) {
            dtn_node = new EhsDtnNode(this, eid, eid_ipn);
            dtn_node_map_.insert(EhsDtnNodePair(eid, dtn_node));
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
            dtn_node->set_should_stop();
            dtn_node = NULL;
        } else if (0 == alert.compare("justBooted")) {
            log_msg(oasys::LOG_NOTICE, "DTN node just booted: %s - re-initializing",
                    eid.c_str());
            dtn_node->set_should_stop();

            dtn_node = new EhsDtnNode(this, eid, eid_ipn);
            dtn_node_map_.insert(EhsDtnNodePair(eid, dtn_node));
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
bool 
EhsExternalRouterImpl::get_link_configuration(EhsLink* el)
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

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
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    pmap = src_priority_;
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::get_dest_node_priority(NodePriorityMap& pmap)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    pmap = dst_priority_;
}

//----------------------------------------------------------------------
bool
EhsExternalRouterImpl::configure_link_throttle(std::string& link_id, uint32_t throttle_bps)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    bool result = true;

    EhsLinkCfg* cl;
    EhsLinkCfgIterator find_iter = link_configs_.find(link_id);
    if (find_iter == link_configs_.end()) {
        result = false;
        log_msg(oasys::LOG_ERR, "configure_link_throttle - Link ID not found: %s", link_id.c_str());
    } else {
        cl = find_iter->second;
        cl->throttle_bps_ = throttle_bps;
    }

    // inform each DtnNode to reconfigure the link
    if (result) {
        EhsDtnNode* node;
        EhsDtnNodeIterator iter = dtn_node_map_.begin();
        while (iter != dtn_node_map_.end()) {
            node = iter->second;
            node->reconfigure_link(link_id);
            ++iter;
        }
    }

    return result;
}


//----------------------------------------------------------------------
bool 
EhsExternalRouterImpl::configure_accept_custody(std::string& val)
{
    bool result = true;

    char* endc = NULL;
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
    EhsDtnNode* node;
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

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    fwdlnk_enabled_ = state;

    // inform each DtnNode of the new fwd link state
    EhsDtnNode* node;
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

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    fwdlnk_aos_ = state;

    // inform each DtnNode of the new fwd link state
    EhsDtnNode* node;
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

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsLinkCfg* cfglnk;
    EhsLinkCfgIterator cfg_iter = link_configs_.begin();
    while (cfg_iter != link_configs_.end()) {
        cfglnk = cfg_iter->second;

        if (cfglnk->is_fwdlnk_) {
            cfglnk->throttle_bps_ = bps;
        }

        ++cfg_iter;
    }

    // inform each DtnNode of the new fwd link rate
    EhsDtnNode* node;
    EhsDtnNodeIterator iter = dtn_node_map_.begin();
    while (iter != dtn_node_map_.end()) {
        node = iter->second;
        node->set_fwdlnk_throttle(bps);
        ++iter;
    }

    return EHSEXTRTR_SUCCESS;
}

//----------------------------------------------------------------------
int
EhsExternalRouterImpl::bundle_delete(uint64_t source_node_id, uint64_t dest_node_id)
{
    int cnt = 0;
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    
    // inform each DtnNode of the new fwd link rate
    EhsDtnNode* node;
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
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    
    // inform each DtnNode of the new fwd link rate
    EhsDtnNode* node;
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
        
        oasys::ScopeLock l(&lock_, __FUNCTION__);

        uint64_t received = 0;
        uint64_t transmitted = 0;
        uint64_t transmit_failed = 0;
        uint64_t delivered = 0;
        uint64_t rejected = 0;
        uint64_t pending = 0;
        uint64_t custody = 0;
        bool fwdlnk_aos = false;
        bool fwdlnk_enabled = false;

        EhsDtnNode* node;
        EhsDtnNodeIterator iter = dtn_node_map_.begin();
        while (iter != dtn_node_map_.end()) {
            node = iter->second;
            node->update_statistics(&received, &transmitted, &transmit_failed, &delivered, &rejected,
                                    &pending, &custody, &fwdlnk_aos, &fwdlnk_enabled);
            ++iter;
        }

        snprintf(buf, sizeof(buf), 
                 "Nodes: %d (%s/%s) Bundles Rcv: %" PRIu64 " Xmt: %" PRIu64 " XFail: %" PRIu64 " Dlv: %" PRIu64  
                 " Rjct: %" PRIu64 " Pend: %" PRIu64 " Cust: %" PRIu64, 
                 nodes, fwdlnk_enabled?"enabled":"disabled", fwdlnk_aos?"AOS":"LOS", 
                 received, transmitted, transmit_failed, delivered, rejected, pending, custody);
    }

    return (const char*) buf;
}

//----------------------------------------------------------------------
int
EhsExternalRouterImpl::num_dtn_nodes()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    return dtn_node_map_.size();
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::set_link_statistics(bool enabled)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsDtnNode* node;
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
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    int cnt = 0;
    EhsDtnNode* node;
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
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsDtnNode* node;
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
    if (stats != NULL && *stats != NULL) {
        free(*stats);
        *stats = NULL;
    }
}

//----------------------------------------------------------------------
void
EhsExternalRouterImpl::fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsDtnNode* node;
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
    if (stats != NULL && *stats != NULL) {
        free(*stats);
        *stats = NULL;
    }
}


//----------------------------------------------------------------------
void
EhsExternalRouterImpl::unrouted_bundle_stats_by_src_dst(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    int cnt = 0;
    EhsDtnNode* node;
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
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    int cnt = 0;
    EhsDtnNode* node;
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
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    int cnt = 0;
    EhsDtnNode* node;
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
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    int cnt = 0;
    EhsDtnNode* node;
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
    uint64_t result = 0;
    if (NULL != sender_) result = sender_->max_bytes_sent();

    oasys::ScopeLock l(&lock_, __FUNCTION__);
    if (NULL != tcp_sender_) result = tcp_sender_->max_bytes_sent();

    return result;
}

//----------------------------------------------------------------------
void 
EhsExternalRouterImpl::set_log_level(int level)
{
    log_level_ = level;

    // update all of the DtnNodes
    EhsDtnNode* node;
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
        if (NULL != log_callback_) {
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
        if (NULL != log_callback_) {
            oasys::ScopeLock l(&log_lock_, __FUNCTION__);

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
        if (NULL != log_callback_) {
            oasys::ScopeLock l(&log_lock_, __FUNCTION__);

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
        if (NULL != log_callback_) {
            va_list args;
            va_start(args, format);
            log_msg_va(logpath_, level, format, args);
            va_end(args);
        }
    }
}


//----------------------------------------------------------------------
EhsExternalRouterImpl::Sender::Sender(EhsExternalRouterImpl* parent, 
                                      oasys::IPSocket::ip_socket_params& params,
                                      in_addr_t local_addr,
                                      in_addr_t remote_addr, uint16_t remote_port)
    : IOHandlerBase(new oasys::Notifier("/ehs/extrtr/sender")),
      Thread("/ehs/extrtr/sender", oasys::Thread::DELETE_ON_EXIT),
      lock_(new oasys::SpinLock()),
      parent_(parent),
      bucket_(logpath(), 50000000, 65535*8)
{
    local_addr_ = local_addr;
    remote_addr_ = remote_addr;
    remote_port_ = remote_port;

    set_logpath("/ehs/extrtr/sender");

    bytes_sent_ = 0;
    max_bytes_sent_ = 0;

    // Local port should be zero - let the OS assign a port
    local_port_ = 0;

    // copy the param values from the parent and then override a few
    params_ = params;

//    params_.recv_bufsize_ = 102400000;
    params_.send_bufsize_ = 10240000;
    params_.multicast_ = false;

    if (fd() == -1) {
        init_socket();
    }

    // Now initialize for multicast sending only
    // set TTL on outbound packets
    u_char ttl = (u_char) params_.mcast_ttl_ & 0xff;
    if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_TTL, (void*) &ttl, 1)
            < 0) {
        parent_->log_msg(logpath_, oasys::LOG_WARN, 
                         "Sender - error setting multicast ttl: %s",
                         strerror(errno));
    }

    // restrict outbound multicast to named interface
    // (INADDR_ANY means outbound on all interaces)
    struct in_addr which;
    memset(&which,0,sizeof(struct in_addr));
    which.s_addr = local_addr_;
    if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_IF, &which,
                sizeof(which)) < 0)
    {
        parent_->log_msg(logpath_, oasys::LOG_WARN, 
                         "Sender - error setting outbound multicast interface: %s",
                         intoa(local_addr_));
    }

    // Check the buffer sizes
    uint32_t rcv_size = 0;
    uint32_t snd_size = 0;
    socklen_t len = sizeof(rcv_size);
    if (::getsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &rcv_size, &len) != 0) {
        parent_->log_msg(oasys::LOG_ERR, "Error reading socket receive buffer size");
    }
    if (::getsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &snd_size, &len) != 0) {
        parent_->log_msg(oasys::LOG_ERR, "Error reading socket send buffer size");
    }
    parent_->log_msg(oasys::LOG_NOTICE, "EhsExternalRouterImpl - Sender Socket Buffers: rcv: %u snd: %u",
                     rcv_size, snd_size);

    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    eventq = new oasys::MsgQueue< std::string * >(logpath_, lock_);
}

EhsExternalRouterImpl::Sender::~Sender()
{
    // free all pending events
    std::string *event;
    while (eventq->try_pop(&event))
        delete event;

    delete eventq;
}

//----------------------------------------------------------------------
// Sender main loop
void
EhsExternalRouterImpl::Sender::run() 
{
    // block on input from the socket and
    // on input from the bundle event list
    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq->read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    bool first_transmit = true;
    while (1) {
        if (should_stop()) return;

        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 1000,
                                           get_notifier());

        if (ret == oasys::IOINTR) {
            parent_->log_msg(logpath_, oasys::LOG_DEBUG, 
                             "EhsExternalRotuerImpl::Sender interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            parent_->log_msg(logpath_, oasys::LOG_DEBUG, 
                             "EhsExternalRotuerImpl::Sender IO error");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            std::string *event;
            if (eventq->try_pop(&event)) {
                ASSERT(event != NULL)

                if (first_transmit) {
                    first_transmit = false;
                    transmit_timer_.get_time();
                }

                //dz debug - rate limiting
//                while (!bucket_.try_to_drain(event->size()*8)) {
//                    usleep(100);
//                }

                int cc = sendto(const_cast< char * >(event->c_str()),
                    event->size(), 0,
                    remote_addr_,
                    remote_port_);

                bytes_sent_ += cc;

                delete event;
            }
        }

        if (!first_transmit && transmit_timer_.elapsed_ms() >= 1000) {
            //if (bytes_sent_ > 0) {
            //    bytes_sent_ *= 8;
            //    parent_->log_msg(logpath_, oasys::LOG_CRIT, 
            //                     "EhsExternalRouterImpl sent %" PRIu64 " bits in %u ms", 
            //                     bytes_sent_, transmit_timer_.elapsed_ms());
            //    transmit_timer_.get_time();
            //    bytes_sent_ = 0;
            //}

            transmit_timer_.get_time();
            if (bytes_sent_ > max_bytes_sent_) max_bytes_sent_ = bytes_sent_;
            bytes_sent_ = 0;
        }
    }
}



//----------------------------------------------------------------------
void
EhsExternalRouterImpl::run_tcp() 
{

    oasys::ScratchBuffer<u_char*, 2048> buffer;

    union US { 
       char       chars[4];
       int32_t    int_size;
    };   

    US union_size;


    log_msg(oasys::LOG_INFO, "EhsExternalRouterImpl running TCP on network_interface: %s", intoa(local_addr_));

    // Create the XML parser
    parser_ = new oasys::XercesXMLUnmarshal(client_validation_,
                                            schema_.c_str());


    // initialize the TCP client
    tcp_client_ = new oasys::TCPClient("ehsrouter/tcpclient");
    tcp_client_->set_logfd(false);
    tcp_client_->init_socket();

    tcp_sender_ = new TcpSender(this);
    tcp_sender_->start();

    while (!should_stop())
    {
        // try to connect if needed
//        if (tcp_client_->state() == oasys::IPSocket::CONNECTING) {
//                log_crit("socket state is CONNECTING - sleep a bit and check again");
//            usleep(100000); // wait 1/10th second and check again
//            continue;
//        }
        if (tcp_client_->state() != oasys::IPSocket::ESTABLISHED) {
            log_crit("connect to DTNME server... state = %d", (int)tcp_client_->state());
            if (tcp_client_->connect(remote_addr_, remote_port_) != 0) {
                log_crit("Unable to connect to External Router TCP interface... will try again in 10 seconds");
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

        log_crit("Connected to DTNME server - state = %d", (int)tcp_client_->state());

        while (!should_stop() && (tcp_client_->state() == oasys::IPSocket::ESTABLISHED)) {
            // read the length field (4 bytes)

            //int cc = tcp_client_->readall(union_size.chars, 4);
            int ret;

            int data_to_read = 4;       
            //
            // get the size off the wire and then read the data
            //
            int data_ptr     = 0;
            union_size.int_size = -1;
            while (!should_stop() && (data_to_read > 0))
            {
                ret = tcp_client_->timeout_read((char *) &(union_size.chars[data_ptr]), data_to_read, 100);
                if ((ret == oasys::IOEOF) || (ret == oasys::IOERROR)) {   
//                    if (errno == EINTR) {
//                        continue;
//                    }
                    tcp_client_->close();
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

            if ((union_size.int_size < 0) || (union_size.int_size > 1000000)) {
                // received signal that other side is closing
                log_crit("Error reading TCP socket - message length = %d", union_size.int_size);
                tcp_client_->close();
                continue;
            }

            buffer.clear();
            buffer.reserve(union_size.int_size+1);  // allow for a terminating NULL character


            data_ptr = 0;
            data_to_read = union_size.int_size;
            while (!should_stop() && (data_to_read > 0)) {
                ret = tcp_client_->timeout_read((char*) buffer.buf()+data_ptr, data_to_read, 100);
                if ((ret == oasys::IOEOF) || (ret == oasys::IOERROR)) {   
                //    if (errno == EINTR) {
//                        continue;
//                    }
                    tcp_client_->close();
                    //set_should_stop();
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
            log_debug("Received message of length: %d", union_size.int_size);

            // add terminating NULL character
            memset(buffer.buf()+union_size.int_size, 0, 1);
            process_action((const char*) buffer.buf());

        }


    }

    oasys::ScopeLock l(&lock_, __FUNCTION__);
    if (tcp_sender_ != NULL ) tcp_sender_->set_should_stop();
}

void
EhsExternalRouterImpl::tcp_sender_closed(TcpSender* tcp_sender)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    if (tcp_sender_ == tcp_sender)
    {
        tcp_sender_ = NULL;
    }
}


EhsExternalRouterImpl::TcpSender::TcpSender(EhsExternalRouterImpl* parent)
    : Thread("tcpsndr"),
      Logger("EhsExternalRouterImpl::TcpSender", "tcpsndr")
{
    parent_ = parent;

    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    eventq_ = new oasys::MsgQueue< std::string * >(logpath_);
}

EhsExternalRouterImpl::TcpSender::~TcpSender()
{
    // free all pending events
    std::string *event;
    while (eventq_->try_pop(&event))
        delete event;

    delete eventq_;
}

void
EhsExternalRouterImpl::TcpSender::post(std::string* event)
{
    eventq_->push_back(event);
}

void
EhsExternalRouterImpl::TcpSender::run() 
{
     
    log_always("EhsExternalRouterImpl::TcpSender::run - started");

    // block on input from the socket and
    // on input from the bundle event list
    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    while (!should_stop()) {

        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 10);

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
            if (eventq_->try_pop(&event)) {
                ASSERT(event != NULL)

                parent_->send_tcp_msg(event);

                delete event;
            }
        }
    }

    parent_->tcp_sender_closed(this);

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

    if (tcp_client_->state() == oasys::IPSocket::ESTABLISHED) {
        union_size.int_size = htonl(msg->size());

        cc = tcp_client_->writeall(union_size.chars, 4);
        if (cc != 4) {
            log_err("error writing msg: %s", strerror(errno));
        } else {
            cc = tcp_client_->writeall( const_cast< char * >(msg->c_str()), msg->size());
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
#endif // XERCES_C_ENABLED && EHS_DP_ENABLED

#endif // EHSROUTER_ENABLED
