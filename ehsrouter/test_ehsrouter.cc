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

#include <stdio.h>


#include "EhsExternalRouter.h"


//#define TEST_NON_DTN_APP_MODE
#if TEST_NON_DTN_APP_MODE
//----------------------------------------------------------------------
static void
do_log_callback(const std::string& path, int level, const std::string& msg)
{
}

int
main(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    printf("Create external router...\n");

    dtn::EhsExternalRouter* ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();

    sleep(5);

//    printf("Delete external router...\n");
//    delete ehs_ext_router_;
    printf("Stop external router...\n");
    ehs_ext_router_->stop();
    ehs_ext_router_ = NULL;

    sleep(5);

    printf("Re-Create external router...\n");
    ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();

    sleep(5);

    printf("Stop external router...\n");
    ehs_ext_router_->stop();
    ehs_ext_router_ = NULL;
    sleep(1);
    printf("Re-Create external router...\n");
    ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();
    printf("Stop external router...\n");
    ehs_ext_router_->stop();
    ehs_ext_router_ = NULL;
    sleep(1);
    printf("Re-Create external router...\n");
    ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();
    printf("Stop external router...\n");
    ehs_ext_router_->stop();
    ehs_ext_router_ = NULL;
    sleep(1);
    printf("Re-Create external router...\n");
    ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();
    printf("Stop external router...\n");
    ehs_ext_router_->stop();
    ehs_ext_router_ = NULL;
    sleep(1);
    printf("Re-Create external router...\n");
    ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();
    printf("Stop external router...\n");
    ehs_ext_router_->stop();
    ehs_ext_router_ = NULL;
    sleep(1);
    printf("Re-Create external router...\n");
    ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();
    printf("Stop external router...\n");
    ehs_ext_router_->stop();
    ehs_ext_router_ = NULL;
    sleep(1);
    printf("Re-Create external router...\n");
    ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();
    printf("Stop external router...\n");
    ehs_ext_router_->stop();
    ehs_ext_router_ = NULL;
    sleep(1);
    printf("Re-Create external router...\n");
    ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();
    printf("Stop external router...\n");
    ehs_ext_router_->stop();
    ehs_ext_router_ = NULL;
    sleep(1);
    printf("Re-Create external router...\n");
    ehs_ext_router_ = new dtn::EhsExternalRouter(&do_log_callback, 1, false);
    ehs_ext_router_->start();




    printf("Delete external router and exit...\n");
    ehs_ext_router_->stop();
    sleep(5);

    return 0;
}

#else // ! TEST_NON_DTN_APP_MODE

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED) && defined(EHSROUTER_ENABLED)

#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#include <oasys/debug/Log.h>
#include <oasys/io/FileUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Getopt.h>
#include <oasys/util/StringUtils.h>

#include "EhsExternalRouter.h"
#include "test_ehsrouter.h"


extern const char* dtn_version;


namespace oasys {
    template <> dtn::TestEhsExternalRouter* oasys::Singleton<dtn::TestEhsExternalRouter>::instance_ = 0;
}

namespace dtn {


static const char* CFG_EXTERNAL_ROUTER_USE_TCP_INTERFACE = "EXTERNAL_ROUTER_USE_TCP_INTERFACE";
static const char* CFG_EXTERNAL_ROUTER_MC_ADDRESS = "EXTERNAL_ROUTER_MC_ADDRESS";
static const char* CFG_EXTERNAL_ROUTER_PORT = "EXTERNAL_ROUTER_PORT";
static const char* CFG_NETWORK_INTERFACE = "NETWORK_INTERFACE";
static const char* CFG_SCHEMA_FILE = "SCHEMA_FILE";

static const char* CFG_FORWARD_LINK = "FORWARD_LINK";
static const char* CFG_FWDLINK_TRANSMIT_ENABLE = "FWDLINK_TRANSMIT_ENABLE";
static const char* CFG_FWDLINK_TRANSMIT_DISABLE = "FWDLINK_TRANSMIT_DISABLE";
static const char* CFG_FWDLINK_THROTTLE = "FWDLINK_THROTTLE";
static const char* CFG_LINK_ENABLE = "LINK_ENABLE";
static const char* CFG_LINK_DISABLE = "LINK_DISABLE";
static const char* CFG_MAX_EXPIRATION_FWD = "MAX_EXPIRATION_FWD";
static const char* CFG_MAX_EXPIRATION_RTN = "MAX_EXPIRATION_RTN";

static const char* CFG_ACCEPT_CUSTODY = "ACCEPT_CUSTODY";


bool g_show_interval_stats = false;
int  g_interval_index = 0;

//----------------------------------------------------------------------
TestEhsExternalRouter::TestEhsExternalRouter()
    : App("TestEhsExternalRouter", "/ehs/extrtr/testapp", dtn_version)
{
    // override default logging setting
    debugpath_ = "~/.ehsrouterdebug";
    loglevel_ = oasys::LOG_INFO;

    ehs_ext_router_ = NULL;

    config_file_set_ = false;
    user_mode_ = false;
    user_abort_ = false;
    do_stats_ = true;
    manual_mode_ = false;
    link_stats_ = false;
}


//----------------------------------------------------------------------
TestEhsExternalRouter::~TestEhsExternalRouter()
{
    sleep(1);
    instance_ = NULL;
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::fill_options()
{
    App::fill_default_options(DAEMONIZE_OPT);
    
    opts_.addopt(
        new oasys::StringOpt('c', "config", &config_file_, "<file>",
                             "configuration file",
                             &config_file_set_));

    opts_.addopt(
        new oasys::BoolOpt('m', "manual", &manual_mode_, 
                           "manual mode - do not autostart"));
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::validate_options(int argc, char* const argv[], int remainder)
{
    (void) argc;
    (void) argv;
    (void) remainder;

//    if (!config_file_set_) {
//        fprintf(stderr, "Aborting - no configuration file specified");
//        print_usage_and_exit();
//    }
}

//----------------------------------------------------------------------
int
TestEhsExternalRouter::main(int argc, char* argv[])
{
    init_app(argc, argv);

    log_info("TestEhsExternalRouter starting up...");

    // if we've daemonized, now is the time to notify our parent
    // process that we've successfully initialized
    if (daemonize_) {
        daemonizer_.notify_parent(0);
    }

    init_router();

    run();

    return 0;
}


//----------------------------------------------------------------------
bool
TestEhsExternalRouter::init_router()
{
    ehs_ext_router_ = new EhsExternalRouter(&TestEhsExternalRouter::log_callback, (int)loglevel_, true);
    return true;
}


//----------------------------------------------------------------------
void
TestEhsExternalRouter::output_header_provider_mode()
{
    printf("\n\n\n\n\n\n\n\n");
    printf("EHS External Router Test Program\n\n");

    printf("Valid keyboard inputs:\n");

    if (!ehs_ext_router_->started()) {
        printf("    s = start external router\n");
        printf("    x = exit\n\n");
    } else {
        printf("    b = bundle stats                      1 = log level debug\n");
        printf("    B = bundle list                       3 = log level notice\n");
        printf("    r = bundle stats: src-dst             5 = log level error\n");
        printf("    l = link dump                         i = toggle link statistics (%s)\n",
               link_stats_?"true":"false");
        printf("    f = fwdlink transmit dump             u = unrouted stats: src-dst\n");
        printf("    e = bundle delete 32774->32969        a = delete all bundles\n");
        printf("\n");
        printf("    w = enable forward link               W = disable forward link\n");
        printf("    A = Simulate AOS                      L = Simulate LOS\n");
        printf("\n");
        printf("    t = toggle statistics update          z = send/recv stats\n");
        printf("    d = display this header               c = process config: update.cfg\n");
        printf("    y = toggle show interval stats\n");
        printf("    x = exit                              h = shutdown DTNME server\n\n");
    }

    printf("States and Statistics:\n");
    fflush(stdout);
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::update_statistics()
{
    if (ehs_ext_router_->started()) {
        if (!g_show_interval_stats) {
            printf("\r%-100s", ehs_ext_router_->update_statistics());
        } else {
            int count = 0;
            EhsFwdLinkIntervalStats* stats = NULL;

            ehs_ext_router_->fwdlink_interval_stats(&count, &stats);

            if (0 == count) {
                printf("\r%-100s", "<no interval stats available>");
            } else {
                if (++g_interval_index >= count) {
                    g_interval_index = 0;
                }

                double rcv_rate = stats[g_interval_index].bytes_received_ * 8.0 / 1000000.0;
                double xmt_rate = stats[g_interval_index].bytes_transmitted_ * 8.0 / 1000000.0;
                printf("\r%d: src: %" PRIu64 " dst: %" PRIu64 "  rcv: %" PRIu64 "B / %.3fMbps  xmt: %" PRIu64 "B / %.3f Mbps %-60s", 
                       g_interval_index, stats[g_interval_index].source_node_id_, stats[g_interval_index].dest_node_id_, 
                       stats[g_interval_index].bundles_received_, rcv_rate,
                       stats[g_interval_index].bundles_transmitted_, xmt_rate, "");
            }
        
            ehs_ext_router_->fwdlink_interval_stats_free(count, &stats);
        }
    } else {
        printf("\r%-100s", "Not Started");
    }
    fflush(stdout);
}

//----------------------------------------------------------------------
int
TestEhsExternalRouter::run()
{

    //XXX/dz - this is not working yet
    if (daemonize_) {

        // if we've daemonized, now is the time to notify our parent
        // process that we've successfully initialized
        if (daemonize_) {
            daemonizer_.notify_parent(0);
        }

        start_external_router();
        ehs_ext_router_->set_fwdlnk_aos(1000);
    
        while (1) {
            sleep(1);
        }
        return 0;
    }


    struct termios oldSettings, newSettings;

    tcgetattr( fileno( stdin ), &oldSettings );
    newSettings = oldSettings;
    newSettings.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr( fileno( stdin ), TCSANOW, &newSettings );
    char key;
    fd_set set;
    struct timeval tv;
    int cnt;

    if (!manual_mode_) {
        start_external_router();
        sleep(1);
        ehs_ext_router_->set_fwdlnk_aos(1000);
    }
    output_header_provider_mode();

    bool done = false;
    while ( !done )
    {
        if (do_stats_) update_statistics();

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO( &set );
        FD_SET( fileno( stdin ), &set );

        int res = select( fileno( stdin )+1, &set, NULL, NULL, &tv );

        if( res > 0 )
        {
            read( fileno( stdin ), &key, 1 );
            switch (key) {
                case 'x':
                case 'X':
                case 'q':
                case 'Q':
                    send_recv_stats();
                    ehs_ext_router_->stop();
                    // router will delete itself when the thread terminates
                    ehs_ext_router_ = NULL;
                    sleep(1);
                    done = true;
                    break;

                case 'A':
                    ehs_ext_router_->set_fwdlnk_aos(1000);
                    break;

                case 'a':
                    cnt = ehs_ext_router_->bundle_delete_all();
                    printf("\nBundles deleted: %d\n", cnt);
                    break;

                case 'b':
                    bundle_stats();
                    break;

                case 'B':
                    bundle_list();
                    break;

                case 'c':
                    process_update_cfg();
                    break;

                case 'd':
                case 'D':
                    output_header_provider_mode();
                    break;

                case 'e':
                    cnt = ehs_ext_router_->bundle_delete(32774, 32969);
                    printf("\nBundles deleted: %d\n", cnt);
                    break;

                case 'f':
                    fwdlink_transmit_dump();
                    break;

                case 'h':
                    shutdown_server();
                    break;

                case 'i':
                    link_stats_ = !link_stats_;
                    ehs_ext_router_->set_link_statistics(link_stats_);
                    break;

                case 'L':
                    ehs_ext_router_->set_fwdlnk_los(1000);
                    break;

                case 'l':
                    link_dump();
                    break;

                case 'r':
                    bundle_stats_by_src_dst();
                    break;

                case 's':
                    start_external_router();
                    sleep(1);
                    output_header_provider_mode();
                    break;

                case 'u':
                    unrouted_bundle_stats_by_src_dst();
                    break;

                case 't':
                case 'T':
                    do_stats_ = !do_stats_;
                    if (!do_stats_) {
                        printf("\rStats update stopped");
                        fflush(stdout);
                    }
                    break;

                case 'w':
                    ehs_ext_router_->set_fwdlnk_enabled(1000);
                    break;

                case 'W':
                    ehs_ext_router_->set_fwdlnk_disabled(1000);
                    break;

                case 'y':
                    g_show_interval_stats = ! g_show_interval_stats;
                    break;

                case 'z':
                    send_recv_stats();
                    break;

                case '1':
                    ehs_ext_router_->set_log_level(1);
                    break;

                case '3':
                    ehs_ext_router_->set_log_level(3);
                    break;

                case '5':
                    ehs_ext_router_->set_log_level(5);
                    break;

                default:
                    break;
            }
        }
    }

    log_info("TestEhsExternalRouter exiting...");

    tcsetattr( fileno( stdin ), TCSANOW, &oldSettings );

    printf("\n\n");
    return 0;
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::start_external_router()
{
    if (!ehs_ext_router_->started()) {
        if (config_file_set_) {
            // make sure config file exists and is readable
            if (!oasys::FileUtils::readable(config_file_.c_str())) {
                log_crit("Config file is not readable: %s", config_file_.c_str());
                fprintf(stderr, "Config file is not readable: %s", config_file_.c_str());
                exit(1);
            }

            // open the config file
            FILE *cfg;

            cfg = fopen(config_file_.c_str(), "r");
            if (NULL == cfg) {
                log_crit("Error opening configuration file: %s", config_file_.c_str());
                fprintf(stderr, "Error opening configuration file: %s", config_file_.c_str());
                exit(1);
            }

            log_notice("parsing configuration file: %s", config_file_.c_str());

            size_t len = 1000;
            char* line = (char*) malloc(1000);

            // parse the file
            bool config_status = true;
            while (getline(&line, &len, cfg) > 0) {
                config_status = parse_line(line);
                if (!config_status) {
                    break;
                }
            }
            free(line);

            fclose(cfg);

            if (!config_status) {
                fprintf(stderr, "\nError configuring EhsExternalRouter - file: %s\n\n", config_file_.c_str());
                fflush(stderr);
                return;
            }
        }

        // Finally, start the router running...
        ehs_ext_router_->set_fwdlnk_enabled(1000);
        ehs_ext_router_->set_fwdlnk_aos(1000);
        ehs_ext_router_->start();
    }
}

//----------------------------------------------------------------------
void 
TestEhsExternalRouter::process_update_cfg()
{
    std::string cfgfile = "update.cfg";

    // make sure config file exists and is readable
    if (!oasys::FileUtils::readable(cfgfile.c_str())) {
        log_crit("Config file is not readable: %s", cfgfile.c_str());
        fprintf(stderr, "\nConfig file is not readable: %s\n", cfgfile.c_str());
        return;
    }

    // open the config file
    FILE *cfg;

    cfg = fopen(cfgfile.c_str(), "r");
    if (NULL == cfg) {
        log_crit("Error opening configuration file: %s", cfgfile.c_str());
        fprintf(stderr, "\nError opening configuration file: %s\n", cfgfile.c_str());
        return;
    }

    log_notice("parsing configuration file: %s", cfgfile.c_str());

    size_t len = 1000;
    char* line = (char*) malloc(1000);

    // parse the file
    bool config_status = true;
    while (getline(&line, &len, cfg) > 0) {
        config_status = parse_line(line);
        if (!config_status) {
            break;
        }
    }
    free(line);

    fclose(cfg);

    if (!config_status) {
        fprintf(stderr, "\nError parsing configuration file: %s\n\n", cfgfile.c_str());
        fflush(stderr);
        return;
    }
}

//----------------------------------------------------------------------
bool
TestEhsExternalRouter::parse_line(char* line)
{
    std::vector<std::string> tokens;

    int toks = oasys::tokenize(line, "= \r\n", &tokens);

    if (toks > 1) {
        // skip blank lines and comments
        if ((0 == tokens[0].size()) || ('#' == tokens[0][0]) || (';' == tokens[0][0])) {
            return true;
        }
    } else {
        return true;
    }

    log_debug("parse line: %s", line);

    bool result = false;

    if (0 == tokens[0].compare(CFG_EXTERNAL_ROUTER_MC_ADDRESS)) {
        result = ehs_ext_router_->configure_mc_address(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_EXTERNAL_ROUTER_PORT)) {
        result = ehs_ext_router_->configure_mc_port(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_NETWORK_INTERFACE)) {
        result = ehs_ext_router_->configure_network_interface(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_EXTERNAL_ROUTER_USE_TCP_INTERFACE)) {
        result = ehs_ext_router_->configure_use_tcp_interface(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_SCHEMA_FILE)) {
        result = ehs_ext_router_->configure_schema_file(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_ACCEPT_CUSTODY)) {
        result = ehs_ext_router_->configure_accept_custody(tokens[1]);



    } else if (0 == tokens[0].compare(CFG_FORWARD_LINK)) {
        result = ehs_ext_router_->configure_forward_link(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_FWDLINK_TRANSMIT_ENABLE)) {
        result = ehs_ext_router_->configure_fwdlink_transmit_enable(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_FWDLINK_TRANSMIT_DISABLE)) {
        result = ehs_ext_router_->configure_fwdlink_transmit_disable(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_FWDLINK_THROTTLE)) {
        char* endc;
        uint32_t rate = strtoull(tokens[1].c_str(), &endc, 10);
        if ('\0' == *endc) {
            ehs_ext_router_->set_fwdlnk_throttle(rate, 1000);
            result = true;
        } else {
            printf("\nError processing FWDLINK_THROTTLE rate value: %s\n", tokens[1].c_str());
            fflush(stdout);
        }
    } else if (0 == tokens[0].compare(CFG_LINK_ENABLE)) {
        result = ehs_ext_router_->configure_link_enable(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_LINK_DISABLE)) {
        result = ehs_ext_router_->configure_link_disable(tokens[1]);

    } else if (0 == tokens[0].compare(CFG_MAX_EXPIRATION_FWD)) {
        result = ehs_ext_router_->configure_max_expiration_fwd(tokens[1]);
    } else if (0 == tokens[0].compare(CFG_MAX_EXPIRATION_RTN)) {
        result = ehs_ext_router_->configure_max_expiration_rtn(tokens[1]);

    } else {
        log_warn("Unknown configuration token: %s", line);
        result = true;
    }

    if (!result) {
        log_crit("Invalid %s in config file: %s",
                 tokens[0].c_str(), line); \
        fprintf(stderr, "Invalid %s in config file: %s",
                tokens[0].c_str(), line);
    }

    return result;
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::link_dump()
{
    bool save_do_stats = do_stats_;
    do_stats_ = false;

    std::string buf;
    ehs_ext_router_->link_dump(buf);
    printf("\n%s\n", buf.c_str());
    fflush(stdout);

    do_stats_ = save_do_stats;
}


//----------------------------------------------------------------------
void
TestEhsExternalRouter::fwdlink_transmit_dump()
{
    bool save_do_stats = do_stats_;
    do_stats_ = false;

    std::string buf;
    ehs_ext_router_->fwdlink_transmit_dump(buf);
    printf("\n%s\n", buf.c_str());
    fflush(stdout);

    do_stats_ = save_do_stats;
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::shutdown_server()
{
    bool save_do_stats = do_stats_;
    do_stats_ = false;

    ehs_ext_router_->shutdown_server();

    do_stats_ = save_do_stats;
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::bundle_stats()
{
    bool save_do_stats = do_stats_;
    do_stats_ = false;

    std::string buf;
    ehs_ext_router_->bundle_stats(buf);
    printf("\n%s\n", buf.c_str());
    fflush(stdout);

    do_stats_ = save_do_stats;
}


//----------------------------------------------------------------------
void
TestEhsExternalRouter::bundle_list()
{
    bool save_do_stats = do_stats_;
    do_stats_ = false;

    std::string buf;
    ehs_ext_router_->bundle_list(buf);
    printf("\n%s\n", buf.c_str());
    fflush(stdout);

    do_stats_ = save_do_stats;
}


//----------------------------------------------------------------------
std::string
TestEhsExternalRouter::fmt_bytes(uint64_t bytes)
{
    char buf[10];
    double dbytes = bytes;

    if (bytes > 1000000000000LL)
    {
        snprintf(buf, sizeof(buf), "%.3fT", dbytes / 1000000000000.0);
    }
    else if (bytes > 1000000000LL)
    {
        snprintf(buf, sizeof(buf), "%.3fG", dbytes / 1000000000.0);
    }
    else if (bytes > 1000000LL)
    {
        snprintf(buf, sizeof(buf), "%.3fM", dbytes / 1000000.0);
    }
    else if (bytes > 1000LL)
    {
        snprintf(buf, sizeof(buf), "%.3fK", dbytes / 1000.0);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%.3f B", dbytes / 1000.0);
    }

    std::string result = buf;
    return result;
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::bundle_stats_by_src_dst()
{
    int count = 0;
    EhsBundleStats* stats = NULL;

    bool save_do_stats = do_stats_;
    do_stats_ = false;

    ehs_ext_router_->bundle_stats_by_src_dst(&count, &stats);

    printf("\n\nBundle Stats by Source-Dest:\n");
    printf("Source  Dest   Received Transmit Pending   Bytes   Custody   ExpRcv   ExpXmt  Deliverd Expired  TTLAbuse Rejected\n");
    printf("------ ------  -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------\n");
    for (int ix=0; ix<count; ++ix) {
        printf("%6" PRIu64 " %6" PRIu64 "  %8" PRIu64 " %8" PRIu64 " %8" PRIu64 " %8.8s %8" PRIu64
               " %8" PRIu64 " %8" PRIu64 " %8" PRIu64 " %8" PRIu64 " %8" PRIu64 " %8" PRIu64 "\n",
               stats[ix].source_node_id_, stats[ix].dest_node_id_,
               stats[ix].total_received_, stats[ix].total_transmitted_, 
               stats[ix].total_pending_, fmt_bytes(stats[ix].total_bytes_).c_str(),
               stats[ix].total_custody_,
               stats[ix].total_expedited_rcv_, stats[ix].total_expedited_xmt_,
               stats[ix].total_delivered_, stats[ix].total_expired_,
               stats[ix].total_ttl_abuse_, stats[ix].total_rejected_);
//        stats[ix].total_pending_bulk_,
//        stats[ix].total_pending_normal_,
//        stats[ix].total_pending_expedited_,
//        stats[ix].total_pending_reserved_,
    }
    printf("\n\n");
    fflush(stdout);

    ehs_ext_router_->bundle_stats_by_src_dst_free(count, &stats);

    do_stats_ = save_do_stats;
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::unrouted_bundle_stats_by_src_dst()
{
    bool save_do_stats = do_stats_;
    do_stats_ = false;

    std::string buf;
    ehs_ext_router_->unrouted_bundle_stats_by_src_dst(buf);
    printf("\n%s\n", buf.c_str());
    fflush(stdout);

    do_stats_ = save_do_stats;
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::send_recv_stats()
{
    uint64_t bytes_recv = ehs_ext_router_->max_bytes_recv();
    uint64_t bytes_sent = ehs_ext_router_->max_bytes_sent();
    double recv_mbps = bytes_recv / 1000000.0;
    double sent_mbps = bytes_sent / 1000000.0;

    printf("\nMax bytes recv/sec: %" PRIu64 " (%.3f Mbps)   sent/sec: %" PRIu64 " (%.3f Mbps)\n", 
           bytes_recv, recv_mbps, bytes_sent, sent_mbps);
    fflush(stdout);
}

//----------------------------------------------------------------------
void
TestEhsExternalRouter::do_log_callback(const std::string& path, int level, const std::string& msg)
{
    oasys::log(path, (oasys::log_level_t)level, msg, false);
}

} // namespace dtn






int
main(int argc, char** argv)
{
    dtn::TestEhsExternalRouter::create();
    dtn::TestEhsExternalRouter::instance()->destroy_singletons_on_exit_ = true;
    dtn::TestEhsExternalRouter::instance()->main(argc, argv);
//    delete dtn::TestEhsExternalRouter::instance();

    sleep(1);

    return 0;
}

#else

int
main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    printf("EHS Router not built: \n");
    printf("  1. Configure oasys with Xerces C 2.8.0 installed on the sytem or using the --with-xerces-c=<dir> option\n");
    printf("  2. Configure DTNME with the  --with-ehsrouter   option and without the   --disable-edp   option\n");
    return 0;
}

#endif // defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED) && defined(EHSROUTER_ENABLED)

#endif // TEST_NON_DTN_APP_MODE
