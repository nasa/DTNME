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

#ifndef _EHS_EXTERNAL_ROUTER_H_
#define _EHS_EXTERNAL_ROUTER_H_

#include <string>


// return status definitions
#define EHSEXTRTR_SUCCESS   0
#define EHSEXTRTR_FAILURE  -1
#define EHSEXTRTR_TIMEOUT  -2


namespace dtn {

class EhsExternalRouterImpl;


/**
 * Callback function definition to pass log messages back to the parent application
 */
typedef void (*LOG_CALLBACK_FUNC)(const std::string& path, int level, const std::string& msg);


/**
 * EhsFwdLinkIntervalStats - This structure defines the statistics that
 * are kept by Source to Destination node pairs for the Forward Link on
 * an interval basis - cleared at each retrieval.
 * An array of these structures is returned by the call to 
 * bundle_stats_by_src_dst().
 */
typedef struct EhsFwdLinkIntervalStats
{
    EhsFwdLinkIntervalStats() :
        source_node_id_(0),
        dest_node_id_(0),
        bundles_received_(0),
        bytes_received_(0),
        bundles_transmitted_(0),
        bytes_transmitted_(0)
    {};

    uint64_t source_node_id_;          ///< Source IPN Node Number for these stats
    uint64_t dest_node_id_;            ///< Destination IPN Node Number for these stats

    uint64_t bundles_received_;        ///< Bundles received in the interval
    uint64_t bytes_received_;          ///< Bytes (payload) received in the interval

    uint64_t bundles_transmitted_;     ///< Bundles transmitted in the interval
    uint64_t bytes_transmitted_;       ///< Bytes (payload) transmitted in the interval
} EhsFwdLinkIntervalStats;



/**
 * EhsBundleStats - This structure defines the statistics that
 * are kept by Source to Destination node pairs. An array of these 
 * structures is returned by the call to bundle_stats_by_src_dst().
 */
typedef struct EhsBundleStats
{
    EhsBundleStats() :
        source_node_id_(0),
        dest_node_id_(0),
        total_received_(0),
        total_transmitted_(0),
        total_transmit_failed_(0),
        total_rejected_(0),
        total_delivered_(0),
        total_expired_(0),
        total_pending_(0),
        total_custody_(0),
        total_bytes_(0),
        total_expedited_rcv_(0),
        total_expedited_xmt_(0),
        total_ttl_abuse_(0),
        total_pending_bulk_(0),
        total_pending_normal_(0),
        total_pending_expedited_(0),
        total_pending_reserved_(0),
        interval_stats_(NULL)
    {};

    ~EhsBundleStats() 
    {
        if (NULL != interval_stats_) {
            delete interval_stats_;
        }
    }

    uint64_t source_node_id_;          ///< Source IPN Node Number for these stats
    uint64_t dest_node_id_;            ///< Destination IPN Node Number for these stats

    uint64_t total_received_;          ///< Total bundles received (cumulative)
    uint64_t total_transmitted_;       ///< Total bundles transmitted (cumulative)
    uint64_t total_transmit_failed_;   ///< Total bundles transmit failed count (cumulative)
    uint64_t total_rejected_;          ///< Total bundles rejected (cumulative)
    uint64_t total_delivered_;         ///< Total bundles delivered (cumulative)
    uint64_t total_expired_;           ///< Total bundles expired (cumulative)

    uint64_t total_pending_;           ///< Total bundles currently pending in the database
    uint64_t total_custody_;           ///< Total bundles currently in local custody
    uint64_t total_bytes_;             ///< Total bytes currently in use in the database

    uint64_t total_expedited_rcv_;     ///< Total expedited bundles received (cumulative)
    uint64_t total_expedited_xmt_;     ///< Total expedited bundles transmitted (cumulative)
    uint64_t total_ttl_abuse_;         ///< Total bundles received that abused the max TTL (cumulative)

    uint64_t total_pending_bulk_;      ///< Total bulk bundles currently in the database
    uint64_t total_pending_normal_;    ///< Total normal bundles currently in the database
    uint64_t total_pending_expedited_; ///< Total expedited bundles currently in the database
    uint64_t total_pending_reserved_;  ///< Total reserved bundles currently in the database (should be none)

    EhsFwdLinkIntervalStats* interval_stats_; ///< Interval Stats used internally - will be returned NULL from API calls
} EhsBundleStats;




/**
 * EhsExternalRouter is an ExternalRouter implementation specific to MSFC and ISS
 */
class EhsExternalRouter
{
public:
    /**
     * Constructor
     *
     * Log messages will be returned to the calling application via the [log_msg] 
     * callback function if provided.
     * 
     * Valid log_level values:
     *     1  = Debug
     *     2  = Info
     *     3  = Notice
     *     4  = Warning
     *     5  = Error
     *     6  = Critical
     *     7  = "Log Always" Messages
     * > Only messages with a level greater than or equal to the log_level
     *   will be returned through the callback function.
     *
     * Set oasys_app to true if the calling application is an oasys based application
     * which initializes the logging facility.
     */
    EhsExternalRouter(LOG_CALLBACK_FUNC log_msg, int log_level=4, bool oasys_app=false);

    /**
     * Destructor
     */
    virtual ~EhsExternalRouter();

    /**
     * Change the log level for messages to be returned through the log callback function
     * > See the Constructor for valid values
     */
    virtual void set_log_level(int level);

    /**
     * Start the EHS External Router running
     */
    virtual void start();

    /**
     * Check to see if the EHS External Router has been started
     */
    virtual bool started();

    /**
     * Stop the EHS External Router
     */
    virtual void stop();


    /**
     * Issue shutdown request to the DTNME server(s)
     */
    virtual bool shutdown_server();


    // Configuration functions
    // 
    // NOTE: configure_* methods can be called before and/or after the start() method
    //
    // NOTE: If a configuration string has multiple fields 
    //       the back tic (`) is used as the separator character
    //

    /**
     * Configure whether or not to use the TCP interface instead of the Multicast interface
     */
    virtual bool configure_use_tcp_interface(std::string& val);

    /**
     * Configure the Multicast Address that the EHS External Router will
     * listen on and use for transmissions
     * > Default: 224.0.0.2
     */
    virtual bool configure_mc_address(std::string& val);

   /**
     * Configure the Port that the EHS External Router will
     * listen on and use for transmissions
     * > Default: 8001
     */
    virtual bool configure_mc_port(std::string& val);

    /**
     * Configure the Network Interface that the EHS External Router will
     * listen on and use for transmissions
     * > Default: 127.0.0.1
     * >> valid values: "any", "lo", "loopback", "eth#" or "<IP Address>"
     */
    virtual bool configure_network_interface(std::string& val);

    /**
     * Configure the path to the XML definition file used for messages 
     * between the DTNME server and the EHS External Router.
     * > Default: /etc/router.xsd
     */
    virtual bool configure_schema_file(std::string& val);


    /**
     * Configure whether or not to accept custody of bundles that match
     * the provided source-destination criteria.
     * > Default: accept custody of all bundles
     *
     * > Order of precedence when checking to determine if should accept custody:
     *     1) Exact match on Source/Destination
     *     2) Match on the Source and a wildcard Destination
     *     3) Match on wildcard Source and the Destination
     *     4) Default acceptance setting
     *
     * Valid string formats:
     *    "clear"
     *    "<true or false>`<source node ID>`<destination node ID list>"
     *        
     *        where:
     *              <source node ID> can be a valid node number or a wildcard "*"
     *              <destination node ID list> can be a comma separated list of node numbers
     *                                         and/or ranges specifed as <start node>-<end node>
     *                                         or a wildcard "*"
     *    
     * Examples:
     *     "clear"       - clear all entries 
     *
     *     "false`*`*"   - set default custody acceptance to false
     *     "true`*`*"    - set default custody acceptance to true
     *
     *     "false`32700`33100,33120-33150"    - do not accept accept custody of bundles with
     *                                          a source node of 32700 and a destination of 
     *                                          33100 or 33120 through 33150 inclusive
     *     
     *     "false`*`32700"      - do not accept accept custody of any bundle with
     *                            a destination node of 32700
     *     
     */
    virtual bool configure_accept_custody(std::string& val);


    /**
     * Configure (and identify) the Forward Link Convergence Layer
     * > Default: none
     *
     * Valid string formats:
     *     "<link ID>`<max bit rate>`<reachable node ID list>"
     *
     *        where:
     *              <link ID> is the name of the link specified in the DTN configuration file
     *              <max bit rate> is the max transmission rate allowed in bits per second
     *              <reachable node ID list> can be a comma separated list of node numbers
     *                                       and/or ranges specifed as <start node>-<end node>
     *
     * Examples:
     *    "iss_link_ltp`2000000`32969-32999,33000-33099,40300-40399"  
     *                  - "iss_link_ltp" is the name of the link which is the Forward Link and
     *                    was configured in the DTN configuration file
     *                  - The EHS External Router will throttle transmission to 2 Mbps
     *                  - All bundles with a destination node ID in one of the 3 ranges
     *                    (32969 thru 32999, 33000 thru 33099 or 40300 thru 40399) will
     *                    be routed to the iss_link_ltp link for transmission
     */
    virtual bool configure_forward_link(std::string& val);

    /**
     * Configure or Reconfigure the Forward Link Transmission Enabled List
     * > Default: All traffic is disabled
     *
     * Valid string formats:
     *    "<source node ID list>`<destination node ID list>"
     *
     *      where:
     *              <source node ID list> can be a comma separated list of node numbers
     *                                    and/or ranges specifed as <start node>-<end node>
     *              <destination node ID list> can be a comma separated list of node numbers
     *                                         and/or ranges specifed as <start node>-<end node>
     *       
     * Examples:
     *    "32774,32776`32969,33050-33053"  - Enable uplink transmission for all bundles with a
     *                                       source node of 32774 or 32776 and a destination of 
     *                                       32969, 33050, 33051, 33052 or 33053
     *      
     */
    virtual bool configure_fwdlink_transmit_enable(std::string& val);

    /**
     * Configure or Reconfigure the Forward Link Transmission Enabled List
     * by removing the specified set of source-destination combinations
     * > Default: All traffic is disabled
     *
     * Valid string formats:
     *    "<source node ID list>`<destination node ID list>"
     *
     *      where:
     *              <source node ID list> can be a comma separated list of node numbers
     *                                    and/or ranges specifed as <start node>-<end node>
     *              <destination node ID list> can be a comma separated list of node numbers
     *                                         and/or ranges specifed as <start node>-<end node>
     *       
     * Examples:
     *    "32774,32776`32969,33050-33053"  - Disable uplink transmission for all bundles with a
     *                                       source node of 32774 or 32776 and a destination of 
     *                                       32969, 33050, 33051, 33052 or 33053
     *      
     */
    virtual bool configure_fwdlink_transmit_disable(std::string& val);

    /**
     * Configure the Forward Link CLA to allow LTP acks while in the disabled state
     * (assuming the Forward Link CLA is the LTPUDP CLA)
     * > Default: false
     */
    virtual void configure_fwdlnk_allow_ltp_acks_while_disabled(bool allowed);

    /**
     * Configure to accept an incoming TCP connection from a specified IP address
     * > Default: All incoming links will be rejected
     *
     * Valid string formats:
     *    "<IP Address>`<false|true (connect out)>`<source node ID list>`<destination node ID list>"
     *
     *      where:
     *              <IP Address> is the IP address to allow for an incoming connection
     *              <false|true> whether or not to establish an outgoing connection to the
     *                           specified IP address using TCP port 4556
     *                           >> nominally false - user should be able to connect in and
     *                              retrieve their bundles through their connection
     *              <source node ID list> can be a comma separated list of node numbers
     *                                    and/or ranges specifed as <start node>-<end node>
     *              <destination node ID list> can be a comma separated list of node numbers
     *                                         and/or ranges specifed as <start node>-<end node>
     *       
     * Examples:
     *    "192.168.1.74`false`32774,32776`32969,33050-33053"  - Allow an incoming TCP connection from
     *                                                          192.168.1.74 and then accept bundles with
     *                                                          source node of 32774 or 32776 and destinations
     *                                                          of 32969, 33050, 33051, 33052 or 33053
     *                                                          >> do not establish an outgoing connection
     *
     */
    virtual bool configure_link_enable(std::string& val);

    /**
     * Close any existing connection(s) and do not allow a connection from a specified IP address
     * > Default: All incoming links will be rejected
     *
     * Valid string formats:
     *    "<IP Address>"
     *
     *      where:
     *              <IP Address> is the IP address from which to close and prevent new connections
     *                           >> close the outgoing connection it established as well
     *       
     * Examples:
     *    "192.168.1.74"  - Close any connections related to 192.168.1.74 and 
     *                      do not allow new connections (until configured to do so)
     *
     */
    virtual bool configure_link_disable(std::string& val);

    /**
     * Configure the maximum allowed expiration times (or time to live [TTL]) in seconds
     * > Default: 172800 (48 hours) for bundles received in the Return Link
     *            12000 (8) hours for bundles to be sent up the Forward Link
     * >> Only used for reporting statistics at this time (not overridden or rejected)
     *
     * Valid string formats:
     *    "<seconds>"   - number of seconds which is the maximum allowed bundle expiration
     *
     * Examples:
     *    "43200"    - Set the maximum expiration time to 43200 seconds (12 hours) 
     */
    virtual bool configure_max_expiration_fwd(std::string& val);
    virtual bool configure_max_expiration_rtn(std::string& val);



    //
    // Forward Link control functions
    //
    /**
     * Enable the Forward Link (transmit when AOS)
     * > Default: disabled
     *
     * timeout_ms is the timeout value in milliseconds
     *
     * return values: 0 = success, -1 = failure, -2 = timed out
     */
    virtual int set_fwdlnk_enabled(uint32_t timeout_ms);

    /**
     * Disable the Forward Link (do not forward bundles when AOS)
     * > Default: disabled
     * > use configure_fwdlnk_allow_ltp_acks_while_disabled to allow/disallow LTP Acks while AOS
     *
     * timeout_ms is the timeout value in milliseconds
     *
     * return values: 0 = success, -1 = failure, -2 = timed out
     */
    virtual int set_fwdlnk_disabled(uint32_t timeout_ms);

    /**
     * Signal that the Forward Link connection is AOS (allow transmission)
     * > Default: LOS
     *
     * timeout_ms is the timeout value in milliseconds
     *
     * return values: 0 = success, -1 = failure, -2 = timed out
     */
    virtual int set_fwdlnk_aos(uint32_t timeout_ms);

    /**
     * Signal that the Forward Link connection is LOS (stop transmission)
     * > Default: LOS
     *
     * timeout_ms is the timeout value in milliseconds
     *
     * return values: 0 = success, -1 = failure, -2 = timed out
     */
    virtual int set_fwdlnk_los(uint32_t timeout_ms);

    /**
     * Change the Forward Link transmission throttle (bits per second)
     * > Default: 2000000  (2 Mbps)
     *
     * timeout_ms is the timeout value in milliseconds
     *
     * return values: 0 = success, -1 = failure, -2 = timed out
     */
    virtual int set_fwdlnk_throttle(uint32_t bps, uint32_t timeout);



    //
    // Bundle management functions
    //
    /**
     * Delete bundles that match the specified source and destination node ID
     */
    virtual int bundle_delete(uint64_t source_node_id, uint64_t dest_node_id);

    /**
     * Delete all bundles in the DTN database
     */
    virtual int bundle_delete_all();



    //
    // Report/Status functions
    //
    /**
     * Retrieve the statistics for the bundles in the DTN Server database 
     * broken out by unique source to destination.
     *
     * NOTE: stats cannot be NULL and *stats must be NULL
     *
     * Return: [*stats] will be set to an allocated block of memory containing an
     *         array of [*count] EhsBundleStats objects.
     *
     * NOTE: Calling function must call bundle_stats_by_src_dst_free to release the
     *       memory allocated by bundle_stats_by_src_dst
     */
    virtual void bundle_stats_by_src_dst(int* count, EhsBundleStats** stats);
    /**
     * Free the memory allocated by bundle_stats_by_src_dst and sets *stats to NULL
     */
    virtual void bundle_stats_by_src_dst_free(int count, EhsBundleStats** stats);

    /**
     * Retrieve accumulated stats for the forward link and reset for next iteration
     */
    virtual void fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats);
    /**
     * Free the memory allocated by fwdlink_interval_stats and sets *stats to NULL
     */
    virtual void fwdlink_interval_stats_free(int count, EhsFwdLinkIntervalStats** stats);



    /** High level stats displayed by the test program:
     *     Nodes: 1 (AOS) Bundles Rcv: 0 Xmt: 0 Dlv: 0 Rjct: 0 Pend: 0 Cust: 0
     */
    virtual const char* update_statistics();
    virtual int num_dtn_nodes();  // Should just be 1 for the forseeable future

    virtual void set_link_statistics(bool enabled);


    // Other reports that may get tweaked..
    //
    virtual void bundle_stats(std::string& buf); // similar to the DTNME bundle stats command output
    virtual void bundle_list(std::string& buf);  // list all bundles -- need to filter/limit/range number returned
    virtual void link_dump(std::string& buf);    // similar to the DTNME link dump command output
    virtual void fwdlink_transmit_dump(std::string& buf); // list the src-dst combos that are enabled for uplink
    virtual void unrouted_bundle_stats_by_src_dst(std::string& buf);  // still need this? change to EhsBundleStats if so



    //
    // Debug and utility functions
    //
    /**
     * Get max bytes sent/received over the external router interface in a second
     */
    uint64_t max_bytes_sent();
    uint64_t max_bytes_recv();

protected:
    EhsExternalRouterImpl* ehs_ext_router_;

};

} // namespace dtn

//#endif // XERCES_C_ENABLED && EHS_DP_ENABLED

#endif //_EHS_EXTERNAL_ROUTER_H_
