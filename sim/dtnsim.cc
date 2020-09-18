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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <errno.h>
#include <string>
#include <sys/time.h>

#include <oasys/debug/Log.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/util/Getopt.h>
#include <oasys/util/Random.h>

#include "dtn-version.h"
#include "ConnCommand.h"
#include "Simulator.h"
#include "SimCommand.h"
#include "SimConvergenceLayer.h"
#include "contacts/ContactManager.h"
#include "cmd/RouteCommand.h"
#include "cmd/StorageCommand.h"
#include "naming/SchemeTable.h"

#include "servlib/bundling/BundleTimestamp.h"
#include "servlib/bundling/BundleProtocol.h"
#include "oasys/storage/MemoryStore.h"
#include "oasys/storage/StorageConfig.h"
#include "oasys/util/App.h"

using namespace dtn;
using namespace dtnsim;

class DTNSim : public oasys::App {
public:
    DTNSim();
    void fill_options();
    void validate_options(int argc,
                          char* const argv[],
                          int remainder);
    int main(int argc, char* const argv[]);

protected:
    bool console_;
    std::string testopts_;
};
    
//----------------------------------------------------------------------
DTNSim::DTNSim()
    : App("DTNSim", "dtnsim", dtn_version),
      console_(false)
{
    random_seed_     = 123456789;
    random_seed_set_ = true;
}

//----------------------------------------------------------------------
void
DTNSim::fill_options()
{
    App::fill_default_options(CONF_FILE_OPT);

    opts_.addopt(
        new oasys::BoolOpt('C', "console", &console_,
                           "run the tcl console after simulation exits"));

    opts_.addopt(
        new oasys::StringOpt('O', "opts", &testopts_,
                             "<opts>", "simulation options"));
}

//----------------------------------------------------------------------
void
DTNSim::validate_options(int argc,
                         char* const argv[],
                         int remainder)
{
    if (!conf_file_set_ && remainder != argc) {
        conf_file_.assign(argv[remainder]);
        conf_file_set_ = true;
        remainder++;
    }

    if (remainder != argc) {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
        opts_.usage("dtnsim");
        exit(1);
    }

    if (!conf_file_set_) {
        fprintf(stderr, "must set the simulator conf file\n");
        opts_.usage("dtnsim");
        exit(1);
    }
}

//----------------------------------------------------------------------
int
DTNSim::main(int argc, char* const argv[])
{
    init_app(argc, argv);
    
    // reset timeval conversion, so timestamps are ok 
    dtn::BundleTimestamp::TIMEVAL_CONVERSION = 0;
    
    // Initialize the simulator first and foremost since it's needed
    // for LogSim::gettimeofday
    Simulator::create();
    
    // Set up the command interpreter and all global commands (i.e.
    // all the ones that aren't node-specific)
    if (oasys::TclCommandInterp::init(argv[0]) != 0)
    {
        log_crit("Can't init TCL");
        exit(1);
    }
    oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();
    interp->reg(new ConnCommand()); 
    interp->reg(new dtn::RouteCommand());
    interp->reg(new SimCommand());
    log_info("registered dtnsim commands");

    // set up all the real singleton components
    SchemeTable::create();
    SimConvergenceLayer::init();
    ConvergenceLayer::add_clayer(SimConvergenceLayer::instance());
    BundleProtocol::init_default_processors();
    log_info("intialized dtnsim components");

    // seed random number generator in tcl as well
    oasys::StringBuffer seed_cmd("expr srand(%u)", random_seed_);
    interp->exec_command(seed_cmd.c_str());
    
    // pass command line options through to the simulation test file
    oasys::StringBuffer opts_cmd("set opts \"%s\"", testopts_.c_str());
    interp->exec_command(opts_cmd.c_str());

    // parse the simulation file
    log_info("parsing configuration file %s...", conf_file_.c_str());
    if (interp->exec_file(conf_file_.c_str()) != 0) {
        log_err("error in configuration file, exiting...");
        exit(1);
    }

    // Run the event loop of simulator
    Simulator::instance()->run();

    if (console_) {
        // Run the command interpreter loop
        oasys::TclCommandInterp::instance()->exec_command(
            "puts \"Simulation complete -- entering console (Control-D to exit)...\"");
        Simulator::instance()->run_console(true);
    }

    return 0;
}

//----------------------------------------------------------------------
int main(int argc, char* const argv[])
{
    DTNSim d;
    return d.main(argc, argv);
}
