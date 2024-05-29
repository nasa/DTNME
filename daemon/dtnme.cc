/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#include <malloc.h>

#include <errno.h>
#include <string>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/tclcmd/ConsoleCommand.h>
#include <third_party/oasys/tclcmd/TclCommand.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/util/App.h>
#include <third_party/oasys/util/Getopt.h>
#include <third_party/oasys/util/StringBuffer.h>

#include "applib/APIServer.h"
#include "cmd/TestCommand.h"
#include "servlib/DTNServer.h"
#include "storage/DTNStorageConfig.h"
#include "bundling/BundleDaemon.h"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <fstream>

extern const char* dtn_version;

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace ssl = boost::asio::ssl;               // from <boost/asio/ssl.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

/**
 * Namespace for the dtn daemon source code.
 */
namespace dtn {

// Echoes back all received WebSocket messages
class session : public std::enable_shared_from_this<session>
{
    tcp::socket socket_;
    websocket::stream<ssl::stream<tcp::socket&>> ws_;
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
    boost::beast::multi_buffer buffer_; 
    bool cmd_console_active_;

public:
    // Take ownership of the socket
    session(tcp::socket socket, ssl::context& ctx, bool cmd_console_active)
        : socket_(std::move(socket))
        , ws_(socket_, ctx)
        , strand_(ws_.get_executor())
        , cmd_console_active_(cmd_console_active)
    {
    }

    // Start the asynchronous operation
    void
    run()
    {
        // Perform the SSL handshake
        ws_.next_layer().async_handshake(
            ssl::stream_base::server,
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &session::on_handshake,
                    shared_from_this(),
                    std::placeholders::_1)));
    }

    void
    on_handshake(boost::system::error_code ec)
    {
        if(ec)
            return;

        // Accept the websocket handshake
        ws_.async_accept(
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &session::on_accept,
                    shared_from_this(),
                    std::placeholders::_1)));
    }

    void
    on_accept(boost::system::error_code ec)
    {
        if(ec)
            return;

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &session::on_read,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }

    void
    on_read(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if(ec == websocket::error::closed)
            return;

        //Get text and submit to tclinterpreter

        oasys::TclCommandInterp::instance()->exec_command(boost::beast::buffers_to_string(buffer_.data()).c_str());

        buffer_.consume(buffer_.size());

        //get result and write to websocket

        std::string result = oasys::TclCommandInterp::instance()->get_result();

        boost::beast::ostream(buffer_) << result;

        // Echo the message
        ws_.async_write(
            buffer_.data(),
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &session::on_write,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));

        if (cmd_console_active_)
        {
            //clear out the last command so the result does not get used as input by the command console
            oasys::TclCommandInterp::instance()->exec_command(" ");
        }
        // else it is a nice feature to be able to repeat the command by just hitting <enter> in dtnme_cli
    }

    void
    on_write(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return;

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    ssl::context& ctx_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    bool cmd_console_active_;

public:
    listener(
        boost::asio::io_context& ioc,
        ssl::context& ctx,
        tcp::endpoint endpoint,
        bool cmd_console_active)
        : ctx_(ctx)
        , acceptor_(ioc)
        , socket_(ioc)
        , cmd_console_active_(cmd_console_active)
    {
        boost::system::error_code ec;


        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            return;
        }

        // Allow the port to be reused immediately for quick recycles of the DTNME server
        typedef boost::asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT> reuse_port;
        acceptor_.set_option(reuse_port(true));


        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            boost::asio::socket_base::max_listen_connections, ec);
        if(ec)
        {
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        if(! acceptor_.is_open())
            return;
        do_accept();
    }

    void
    do_accept()
    {
        acceptor_.async_accept(
            socket_,
            std::bind(
                &listener::on_accept,
                shared_from_this(),
                std::placeholders::_1));
    }

    void
    on_accept(boost::system::error_code ec)
    {
        if(ec)
        {
        }
        else
        {
            // Create the session and run it
            std::make_shared<session>(std::move(socket_), ctx_, cmd_console_active_)->run();
        }

        // Accept another connection
        do_accept();
    }

    void
    do_shutdown()
    {
        acceptor_.close();
    }
};


class WebsocketServer : public oasys::Thread,
                        public oasys::Logger
{
public:
    /**
     * Constructor.
     */
    WebsocketServer(in_addr_t console_addr, u_int16_t console_port, bool cmd_console_active)
        : Thread("WebsockSrvr"),
          Logger("WebsockSrvr", "/dtn/websocksrvr"),
          console_addr_(console_addr),
          console_port_(console_port),
          cmd_console_active_(cmd_console_active)
    {
    }

    ~WebsocketServer()
    {
    }

    void run()
    {
        char threadname[16] = "WebsocketSrvr";
        pthread_setname_np(pthread_self(), threadname);

        auto const address = boost::asio::ip::make_address(intoa(console_addr_));
        auto const port = static_cast<unsigned short>(console_port_);
        auto const threads = std::max<int>(1, 3);

        // The io_context is required for all I/O
        boost::asio::io_context ioc{threads};
        
        // The SSL context is required, and holds certificates
        ssl::context ctx{ssl::context::tlsv12};

        // This holds the self-signed certificate used by the server
        //load_server_certificate(ctx);

        boost::system::error_code ec1, ec2, ec3;

        ctx.set_password_callback(
            [](std::size_t,
                boost::asio::ssl::context_base::password_purpose)
            {
                return "test";
            });

        ctx.set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::single_dh_use);

        ctx.use_certificate_chain_file(BundleDaemon::params_.console_chain_.c_str(),ec1);

        ctx.use_private_key_file(
            BundleDaemon::params_.console_key_.c_str(),
            boost::asio::ssl::context::file_format::pem,ec2);

        ctx.use_tmp_dh_file(BundleDaemon::params_.console_dh_.c_str(),ec3);

        if(!ec1 && !ec2 && !ec3){
            // Create and launch a listening port
            sptr_listener_ = std::make_shared<listener>(ioc, ctx, tcp::endpoint{address, port}, cmd_console_active_);
            sptr_listener_->run();

            // Run the I/O service on the requested number of threads
            /*std::vector<std::thread> v;
            v.reserve(threads);
            for(auto i = threads; i > 0; --i)
                v.emplace_back(
                [&ioc]
                {
                    ioc.run();
                });*/

            log_always("Websocket Console Started");

            try
            {
                ioc.run();
            }
            catch (...)
            {
                log_err("Caught error closing down the Websocket Console");
            }

        } else {
            if(ec1){
                char message[] = "something went wrong with the Console Certificate Chain File: ";
                strcat(message,ec1.message().c_str());
                log_err(message);
            }
            if(ec2){
                char message[] = "something went wrong with the Console Private Key File: ";
                strcat(message,ec2.message().c_str());
                log_err(message);
            }
            if(ec3){
                char message[] = "something went wrong with the Console DH Parameters File: ";
                strcat(message,ec3.message().c_str());
                log_err(message);
            }
            sleep(3);
        }

        log_always("Web Socket Console Shutdown");
                
    }

    void do_shutdown()
    {
        if (sptr_listener_) {
            sptr_listener_->do_shutdown();
        }
    }

private:    
    in_addr_t console_addr_;
    u_int16_t console_port_;
    std::shared_ptr<listener> sptr_listener_;
    bool cmd_console_active_;
};


class CommandConsoleThread : public oasys::Thread,
                             public oasys::Logger
{
public:
    /**
     * Constructor.
     */
    CommandConsoleThread(bool daemonize, bool stdio, const char* prompt, 
                         in_addr_t console_addr, u_int16_t console_port)
        : Thread("CommandConsole"),
          Logger("CommandConsole", "/dtn/cmdconsole"),
          daemonize_(daemonize),
          stdio_(stdio),
          prompt_(prompt),
          console_addr_(console_addr),
          console_port_(console_port)
    {
    }

    ~CommandConsoleThread()
    {
    }

    void run()
    {
        char threadname[16] = "CommandConsole";
        pthread_setname_np(pthread_self(), threadname);

        // start the TCP server TCL command server?
        if (console_port_ != 0) {
            oasys::TclCommandInterp::instance()->command_server(prompt_.c_str(), console_addr_, console_port_);
        }


        // start the stdio TCL command console?
        if (stdio_ && !daemonize_) {
            try
            {
                oasys::TclCommandInterp::instance()->command_loop(prompt_.c_str());
            }
            catch (...)
            {
                log_err("Caught error closing down the TCL command Console");
            }

            // need this in case user used the exit command instead of the shutdown command
            BundleDaemon::set_user_initiated_shutdown();
        }
        else if (console_port_ != 0)
        {
            oasys::TclCommandInterp::instance()->event_loop();
        }
    }

    void do_shutdown()
    {
        //oasys::TclCommandInterp::instance()->exit_event_loop();
    }

private:
    bool daemonize_;
    bool stdio_;
    std::string prompt_;
    in_addr_t console_addr_;
    u_int16_t console_port_;
};




/**
 * Thin class that implements the daemon itself.
 */
class DTNME : public oasys::App {
public:
    DTNME();
    int main(int argc, char* argv[]);

    

protected:
    TestCommand*          testcmd_;
    oasys::ConsoleCommand* consolecmd_;
    DTNStorageConfig      storage_config_;
    
    // virtual from oasys::App
    void fill_options();
    
    void output_welcome_message();
    void init_testcmd(int argc, char* argv[]);
    void run_console();

private:
    std::shared_ptr<WebsocketServer> sptr_websock_server;
    std::shared_ptr<CommandConsoleThread> sptr_command_console_;
};

//----------------------------------------------------------------------
DTNME::DTNME()
    : App("dtnme", "dtnme", dtn_version),
      testcmd_(NULL),
      consolecmd_(NULL),
      storage_config_("storage",			// command name
                      "berkeleydb",			// storage type
                      "DTN",				// DB name
                      INSTALL_LOCALSTATEDIR "/dtn/db")	// DB directory
{
    //umask(0002); // set deafult mask to prevent world write/delete

    // override default logging settings
    loglevel_ = oasys::LOG_NOTICE;
    debugpath_ = "~/.dtnmedebug";
    
    // override defaults from oasys storage config
    storage_config_.db_max_tx_ = 1000;

    testcmd_    = new TestCommand();
    consolecmd_ = new oasys::ConsoleCommand("dtnme% ");
}

//----------------------------------------------------------------------
void
DTNME::fill_options()
{
    fill_default_options(DAEMONIZE_OPT | CONF_FILE_OPT);
    
    opts_.addopt(
        new oasys::BoolOpt('t', "tidy", &storage_config_.tidy_,
                           "clear database and initialize tables on startup"));

    opts_.addopt(
        new oasys::BoolOpt(0, "init-db", &storage_config_.init_,
                           "initialize database on startup"));

    opts_.addopt(
        new oasys::InAddrOpt(0, "console-addr", &consolecmd_->addr_, "<addr>",
                             "set the console listening addr (default off)"));
    
    opts_.addopt(
        new oasys::UInt16Opt(0, "console-port", &consolecmd_->port_, "<port>",
                             "set the console listening port (default off)"));
    
    opts_.addopt(
        new oasys::IntOpt('i', 0, &testcmd_->id_, "<id>",
                          "set the test id"));
}

//----------------------------------------------------------------------
void
DTNME::init_testcmd(int argc, char* argv[])
{
    for (int i = 0; i < argc; ++i) {
        testcmd_->argv_.append(argv[i]);
        testcmd_->argv_.append(" ");
    }

    testcmd_->bind_vars();
    oasys::TclCommandInterp::instance()->reg(testcmd_);
}

//----------------------------------------------------------------------
void
DTNME::run_console()
{
    bool websocket_server_active = false;

    // launch the console server
    if (consolecmd_->port_ != 0) {
        log_always("starting %s console on %s:%d", 
                   BundleDaemon::params_.console_type_.c_str(), intoa(consolecmd_->addr_), consolecmd_->port_);

        if (BundleDaemon::params_.console_type_ == "websocket") {
            bool cmd_console_active = (consolecmd_->stdio_ && !daemonize_);
            sptr_websock_server = std::make_shared<WebsocketServer>(consolecmd_->addr_, consolecmd_->port_, cmd_console_active);
            sptr_websock_server->start();

            websocket_server_active = true;

        } else {
            // need to start TCP command server in same thread as the command console
        } 
    }

    // start command console thread and TCP command server if so configured
    if (websocket_server_active) {
        // prevent starting a TCP command server by setting port to zero
        sptr_command_console_ = std::make_shared<CommandConsoleThread>(daemonize_, consolecmd_->stdio_,
                                                                       consolecmd_->prompt_.c_str(), 0, 0);
    } else {
        sptr_command_console_ = std::make_shared<CommandConsoleThread>(daemonize_, consolecmd_->stdio_,
                                                                       consolecmd_->prompt_.c_str(),
                                                                       consolecmd_->addr_, consolecmd_->port_);
    }
    sptr_command_console_->start();

    // wait for a user to invoke the shutdown command...
    while (!BundleDaemon::user_initiated_shutdown()) {
        usleep(100);
    }


    if (websocket_server_active) {
        sptr_websock_server->do_shutdown();
    }

    sptr_command_console_->do_shutdown();
}

//----------------------------------------------------------------------
void
DTNME::output_welcome_message()
{
    std::string msg1, msg2, msg3, msg4, msg5, msg6;

    msg1 = "=====================  NOTICE  =====================";
    msg2 = "     DTNME supports both BPv6 and BPv7 bundles.";
                                
    if (BundleDaemon::params_.api_send_bp_version7_) {
        msg3 = "     By default, apps will generate BPv7 bundles";
        msg4 = "     Use the -V6 option to generate BPv6 bundles";
        msg5 = "     or change the default with the command:";
        msg6 = "           param set api_send_bp7 false";
    } else {
        msg3 = "     By default, apps will generate BPv6 bundles";
        msg4 = "     Use the -V7 option to generate BPv7 bundles";
        msg5 = "     or change the default with the command:";
        msg6 = "           param set api_send_bp7 true";
    }


    // output to the log file
    log_always(msg1.c_str());
    log_always(msg2.c_str());
    log_always(msg3.c_str());
    log_always(msg4.c_str());
    log_always(msg5.c_str());
    log_always(msg6.c_str());
    log_always(msg1.c_str());

    /*// output to stdout
    printf("\n\n%s\n", msg1.c_str());
    printf("%s\n\n", msg2.c_str());
    printf("%s\n", msg3.c_str());
    printf("%s\n", msg4.c_str());
    printf("%s\n", msg5.c_str());
    printf("%s\n", msg6.c_str());
    printf("%s\n\n", msg1.c_str());*/

}

//----------------------------------------------------------------------
int
DTNME::main(int argc, char* argv[])
{
    init_app(argc, argv);

    log_notice("DTN daemon starting up... (pid %d)", getpid());


    if (oasys::TclCommandInterp::init(argv[0], "/dtn/tclcmd") != 0)
    {
        log_crit("Can't init TCL");
        notify_and_exit(1);
    }

    // stop thread creation b/c of initialization dependencies
    oasys::Thread::activate_start_barrier();

    DTNServer* dtnserver = new DTNServer("/dtnme", &storage_config_);
    APIServer* apiserver = new APIServer();

    dtnserver->init();

    oasys::TclCommandInterp::instance()->reg(consolecmd_);
    init_testcmd(argc, argv);

    if (! dtnserver->parse_conf_file(conf_file_, conf_file_set_)) {
        log_err("error in configuration file, exiting...");
        notify_and_exit(1);
    }

    // configure the umask for controlling file permissions
    // - the mask is inverted when applied so zero bits allow the particular access
    uint16_t mask = ~BundleDaemon::params_.file_permissions_;
    // - allow owner full access regardless of what the user specified
    //mask &= 0x077; 
    umask(mask); // set deafult mask to prevent world write

    log_info("file permissions: 0%3.3o", (~mask & 0777));

    output_welcome_message();

    if (storage_config_.init_)
    {
        log_notice("initializing persistent data store");
    }

    if (! dtnserver->init_datastore()) {
        log_err("error initializing data store, exiting...");
        notify_and_exit(1);
    }
    
    // If we're running as --init-db, make an empty database and exit
    if (storage_config_.init_ && !storage_config_.tidy_)
    {
        dtnserver->close_datastore();
        log_info("database initialization complete.");
        notify_and_exit(0);
    }
    
    if (BundleDaemon::instance()->local_eid() == BD_NULL_EID())
    {
        log_err("no local eid specified; use the 'route local_eid' command");
        notify_and_exit(1);
    }

    // if we've daemonized, now is the time to notify our parent
    // process that we've successfully initialized
    
    if (daemonize_) {
        daemonizer_.notify_parent(0);
    }
    
    
    log_notice("starting APIServer on %s:%d", 
                 intoa(apiserver->local_addr()), apiserver->local_port());

    // set console info before starting the dtnserver (BundleDaemon)
    dtnserver->set_console_info(consolecmd_->addr_, consolecmd_->port_, consolecmd_->stdio_);

    dtnserver->start();
    if (apiserver->enabled()) {
        apiserver->bind_listen_start(apiserver->local_addr(), 
                                     apiserver->local_port());
    }
    oasys::Thread::release_start_barrier(); // run blocked threads

    // if the test script specified something to run for the test,
    // then execute it now
    if (testcmd_->initscript_.length() != 0) {
        oasys::TclCommandInterp::instance()->
            exec_command(testcmd_->initscript_.c_str());
    }

    // allow startup messages to be flushed to standard-out before
    // the prompt is displayed
    oasys::Thread::yield();
    
    run_console();

    log_notice("command loop exited... shutting down daemon");

    apiserver->stop();
    apiserver->shutdown_hook();
    
    //dzdebug   oasys::TclCommandInterp::shutdown();  - segfault
    dtnserver->shutdown();
    
    // close out servers
    delete dtnserver;

    // don't delete apiserver; keep it around so slow APIClients can finish
    // dzdebug  delete apiserver;  -  error closing socket in state LISTENING: Bad file descriptor ??


    // give other threads (like logging) a moment to catch up before exit
    
    // kill logging
    //dzdebug oasys::Log::shutdown();
    
    while (! BundleDaemon::instance()->is_stopped()) {
        oasys::Thread::yield();
    }

    BundleDaemon::reset();


    return 0;
}

} // namespace dtn

int
main(int argc, char* argv[])
{
    dtn::DTNME dtnme;

    //dzdebug
    int status = mallopt(M_ARENA_MAX, 48);
    if (status == 0) {
        printf("error setting M_ARENA_MAX to 48\n");
    } else {
        //printf("success setting M_ARENA_MAX to 48\n");
    }

    int val = 1024*1024*4*8;
    status = mallopt(M_MMAP_THRESHOLD, val);
    if (status == 0) {
        printf("error setting M_MMAP_THRESHOLD to %d\n", val);
    } else {
        //printf("success setting M_MMAP_THRESHOLD to %d\n", val);
    }


    dtnme.main(argc, argv);
}

