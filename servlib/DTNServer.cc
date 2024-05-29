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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <third_party/oasys/storage/BerkeleyDBStore.h>
#include <third_party/oasys/io/FileUtils.h>
#include <third_party/oasys/thread/SpinLock.h>

#include "DTNServer.h"

#include "bundling/BundleDaemon.h"

#include "contacts/InterfaceTable.h"
#include "contacts/ContactManager.h"
#include "contacts/ContactPlanner.h"

#include "cmd/CompletionNotifier.h"
#include "cmd/AcsCommand.h"
#include "cmd/BPCommand.h"
#include "cmd/BundleCommand.h"
#include "cmd/InterfaceCommand.h"
#include "cmd/LinkCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/RegCommand.h"
#include "cmd/RegistrationCommand.h"
#include "cmd/RouteCommand.h"
#include "cmd/ShutdownCommand.h"
#include "cmd/StorageCommand.h"
#include "cmd/ContactCommand.h"
#include "cmd/VersionCommand.h"

#ifdef BARD_ENABLED
    #include "cmd/BundleRestagingCommand.h"
#endif  // BARD_ENABLED


#include "conv_layers/ConvergenceLayer.h"

#include "naming/SchemeTable.h"

#include "reg/AdminRegistration.h"
#include "reg/RegistrationTable.h"

#include "routing/BundleRouter.h"

#include "storage/BundleStore.h"
#include "storage/IMCRegionGroupRecStore.h"
#include "storage/LinkStore.h"
#include "storage/GlobalStore.h"
#include "storage/PendingAcsStore.h"
#include "storage/RegistrationStore.h"
#include "storage/DTNStorageConfig.h"


#ifdef BARD_ENABLED
#    include "storage/BARDQuotaStore.h"
#endif  // BARD_ENABLED

#ifdef DTPC_ENABLED
#    include "dtpc/DtpcDaemon.h"
#    include "cmd/DtpcCommand.h"
#    include "dtpc/DtpcProfileStore.h"
#    include "dtpc/DtpcTopicStore.h"
#    include "dtpc/DtpcDataPduCollectorStore.h"
#    include "dtpc/DtpcPayloadAggregatorStore.h"
#endif // DTPC_ENABLED

namespace dtn {

DTNServer::DTNServer(const char* logpath,
                     DTNStorageConfig* storage_config)
    : Logger("DTNServer", "%s", logpath),
      in_shutdown_(0),
      storage_config_(storage_config),
      store_(0)
{}

DTNServer::~DTNServer()
{
    log_notice("daemon exiting...");
}

void
DTNServer::init()
{
    ASSERT(oasys::Thread::start_barrier_enabled());

    init_commands();
    init_components();
}

void
DTNServer::set_console_info(in_addr_t console_addr, u_int16_t console_port, bool console_stdio)
{
    console_addr_ = console_addr;
    console_port_ = console_port;
    console_stdio_ = console_stdio;
}


void
DTNServer::start()
{
    BundleDaemon* daemon = BundleDaemon::instance();
    daemon->set_console_info(console_addr_, console_port_, console_stdio_);
    daemon->start();
    log_debug("started dtn server");
}

bool
DTNServer::init_datastore()
{
    log_debug("Initializing datastore.");

    // flag whether the storage tidy option was invoked (for Restage CLs)
    BundleDaemon::params_.storage_tidy_ = storage_config_->tidy_;


    if (storage_config_->tidy_) 
    {
        storage_config_->init_ = true; // init is implicit with tidy
    }
    store_ = new oasys::DurableStore("/dtn/storage");
    int err = store_->create_store(*storage_config_);
    if (err != 0) {
        log_crit("error creating storage system");
        return false;
    }

    if (storage_config_->odbc_use_aux_tables_ && !store_->aux_tables_available())
    {
    	// User requested use of auxiliary tables but not supported by storage mechanism
    	log_warn("odbc_use_aux_tables set true but auxiliary tables not available with storage type %s - ignoring.",
    			 store_->get_info().c_str());
    }

    if (storage_config_->tidy_)
    {
        // remove bundle data directory (the db contents are cleaned
        // up by the implementation)
        if (!tidy_dir(storage_config_->payload_dir_.c_str())) {
            return false;
        }
    }

    if (storage_config_->init_)
    {
        if (!init_dir(storage_config_->payload_dir_.c_str())) {
            return false;
        }
    }

    if (!validate_dir(storage_config_->payload_dir_.c_str())) {
        return false;
    }

    if ((GlobalStore::init(*storage_config_, store_)       != 0) || 
        (BundleStore::init(*storage_config_, store_)       != 0) ||
        (LinkStore::init(*storage_config_, store_)         != 0) ||
        (RegistrationStore::init(*storage_config_, store_) != 0))
    {
        log_crit("error initializing data store");
        return false;
    }

    if (IMCRegionGroupRecStore::init(*storage_config_, store_)    != 0)  
    {
        log_crit("error initializing IMC Routing data store");
        return false;
    }


    if (PendingAcsStore::init(*storage_config_, store_)    != 0)  
    {
        log_crit("error initializing Pending ACS data store");
        return false;
    }

#ifdef BARD_ENABLED
    if (BARDQuotaStore::init(*storage_config_, store_)    != 0)  
    {
        log_crit("error initializing BARDQuota data store");
        return false;
    }
#endif  // BARD_ENABLED


#ifdef DTPC_ENABLED
    if ((DtpcProfileStore::init(*storage_config_, store_)    != 0))
    {
        log_crit("error initializing DtpcProfile data store");
        return false;
    }
    if ((DtpcTopicStore::init(*storage_config_, store_)    != 0))
    {
        log_crit("error initializing DtpcTopic data store");
        return false;
    }
    if ((DtpcDataPduCollectorStore::init(*storage_config_, store_)    != 0))
    {
        log_crit("error initializing DtpcDataPduCollector data store");
        return false;
    }
    if ((DtpcPayloadAggregatorStore::init(*storage_config_, store_)    != 0))
    {
        log_crit("error initializing DtpcPayloadAggregator data store");
        return false;
    }

#endif //DTPC_ENABLED

	if (store_->create_finalize(*storage_config_) != 0)
	{
		log_crit("error running post table creation database commands");
		return false;
	}

    // load in the global store here since that will check the
    // database version and exit if there's a mismatch
    if (!GlobalStore::instance()->load()) {
        return false;
    }

    return true;
}
 
bool
DTNServer::parse_conf_file(std::string& conf_file,
                           bool         conf_file_set)
{
    // Check the supplied config file and/or check for defaults, as
    // long as the user didn't explicitly call with no conf file 
    if (conf_file.size() != 0) 
    {
        if (!oasys::FileUtils::readable(conf_file.c_str(), logpath()))
        {
            log_err("configuration file \"%s\" not readable",
                    conf_file.c_str());
            return false;
        }
    }
    else if (!conf_file_set) 
    {
        const char* default_conf[] = { INSTALL_SYSCONFDIR "/dtnme_daemon.cfg", 
                                       "daemon/dtnme_daemon.cfg", 
                                       0 };
        conf_file.clear();
        for (int i=0; default_conf[i] != 0; ++i) 
        {
            if (oasys::FileUtils::readable(default_conf[i], logpath())) 
            {
                conf_file.assign(default_conf[i]);
                break;
            }
        }
        if (conf_file.size() == 0)
        {
            log_warn("can't read default config file "
                     "(tried " INSTALL_SYSCONFDIR "/dtnme_daemon.cfg "
                     "and daemon/dtnme_daemon.cfg)...");
        }
    }

    if (conf_file.size() == 0) {
        log_info("No config file specified.");
        return false;
    }

    log_info("parsing configuration file %s...", conf_file.c_str());
    if (oasys::TclCommandInterp::instance()->exec_file(conf_file.c_str()) != 0)
    {
        // instead of aborting just output a warning
        fprintf(stderr, "\n\nError processing config file: %s - configuration is probably incomplete\n",  conf_file.c_str());
        fprintf(stderr, "Check log file for details\n\n");
        fflush(stdout);
        //return false;
    }

    return true;
}

void
DTNServer::init_commands()
{
    oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();

    CompletionNotifier::create();
    interp->reg(new BPCommand());
    interp->reg(new BundleCommand());
    interp->reg(new InterfaceCommand());
    interp->reg(new LinkCommand());
    interp->reg(new ParamCommand());

#ifdef BARD_ENABLED
    interp->reg(new BundleRestagingCommand());
#endif  // BARD_ENABLED

    interp->reg(new RegCommand());
    interp->reg(new RegistrationCommand());
    interp->reg(new RouteCommand());
    interp->reg(new ShutdownCommand(this, "shutdown"));
    interp->reg(new StorageCommand(storage_config_));
    interp->reg(new AcsCommand());
    interp->reg(new ContactCommand());
    interp->reg(new VersionCommand());

#ifdef DTPC_ENABLED
    interp->reg(new DtpcCommand());
#endif

    log_debug("registered dtn commands");
}

void
DTNServer::init_components()
{
    ContactPlanner::init();
    SchemeTable::create();
    BundleDaemon::init();
    ConvergenceLayer::init_clayers();
    InterfaceTable::init();

#ifdef DTPC_ENABLED
    DtpcDaemon::init();
#endif

    
    // Override SpinLock contention warnings by default
    // - can be renable with "param set warn_on_spinlock_contention true" command
    oasys::SpinLock::warn_on_contention_ = false;
    

    log_debug("intialized dtn components");
}
   
void
DTNServer::close_datastore()
{
    log_notice("closing persistent data store");
    
    IMCRegionGroupRecStore::instance()->close();
    PendingAcsStore::instance()->close();

#ifdef BARD_ENABLED
    BARDQuotaStore::instance()->close();
#endif  // BARD_ENABLED

    RegistrationStore::instance()->close();
    LinkStore::instance()->close();
    BundleStore::instance()->close();
    GlobalStore::instance()->close();
    
#ifdef DTPC_ENABLED
    DtpcProfileStore::instance()->close();
    DtpcTopicStore::instance()->close();
    DtpcDataPduCollectorStore::instance()->close();
    DtpcPayloadAggregatorStore::instance()->close();
#endif
    
    // now delete the singletons
    delete IMCRegionGroupRecStore::instance();
    delete PendingAcsStore::instance();
    delete RegistrationStore::instance();
    delete LinkStore::instance();
    delete BundleStore::instance();
    
#ifdef BARD_ENABLED
    delete BARDQuotaStore::instance();
#endif  // BARD_ENABLED

#ifdef DTPC_ENABLED
    delete DtpcProfileStore::instance();
    delete DtpcTopicStore::instance();
    delete DtpcDataPduCollectorStore::instance();
    delete DtpcPayloadAggregatorStore::instance();
#endif
    
//    delete GlobalStore::instance();
    delete_z(store_);


    // and this will cause a double delete
    if(!getenv("OASYS_CLEANUP_SINGLETONS")) {
       delete_z(store_);
    }
}

void
DTNServer::shutdown()
{
    log_notice("shutting down dtn server");
    
    // make sure only one thread does this
    u_int32_t old_val = atomic_incr_ret(&in_shutdown_);
    if (old_val != 1) {
        log_warn("second thread called DTNServer::shutdown... spinning forever");
        while (1) {
            sleep(1000000);
        }
    }

    oasys::Notifier done("/dtnserver/shutdown");
    log_info("DTNServer shutdown called, posting shutdown request to daemon");
    ShutdownRequest* event_to_post;
    event_to_post = new ShutdownRequest();
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::instance()->post_and_wait(sptr_event_to_post, &done);
    BundleDaemon::instance()->cleanup_allocations();

    close_datastore();

    CompletionNotifier::reset();

    //dzdebug - close_datastore cancels a timer if Berkeley so do this afterwards
    oasys::SharedTimerThread::instance()->shutdown();
}

void
DTNServer::set_app_shutdown(ShutdownProc proc, void* data)
{
    BundleDaemon::instance()->set_app_shutdown(proc, data);
}

bool
DTNServer::init_dir(const char* dirname)
{
    struct stat st;
    int statret;
    
    statret = stat(dirname, &st);
    if (statret == -1 && errno == ENOENT)
    {
        if (mkdir(dirname, 0777) != 0) {
            log_crit("can't create directory %s: %s",
                     dirname, strerror(errno));
            return false;
        }
    }
    else if (statret == -1)
    {
        log_crit("invalid path %s: %s", dirname, strerror(errno));
        return false;
    }

    return true;
}

bool
DTNServer::tidy_dir(const char* dir)
{
    char cmd[256];
    struct stat st;

    std::string dirname(dir);
    oasys::FileUtils::abspath(&dirname);
    
    if (stat(dirname.c_str(), &st) == 0)
    {
        snprintf(cmd, sizeof(cmd), "/bin/rm -rf %s", dirname.c_str());
        log_notice("tidy option removing directory '%s'", cmd);
        
        if (system(cmd))
        {
            log_crit("error removing directory %s", dirname.c_str());
            return false;
        }
        
    }
    else if (errno == ENOENT)
    {
        log_debug("directory already removed %s", dirname.c_str());
    }
    else
    {
        log_crit("invalid directory name %s: %s", dirname.c_str(), strerror(errno));
        return false;
    }

    return true;
}

bool
DTNServer::validate_dir(const char* dirname)
{
    struct stat st;
    
    if (stat(dirname, &st) == 0 && S_ISDIR(st.st_mode))
    {
        log_debug("directory validated: %s", dirname);
    }
    else
    {
        log_crit("invalid directory name %s: %s", dirname, strerror(errno));
        return false;
    }

    if (access(dirname, R_OK | W_OK | X_OK) == 0)
    {
        log_debug("directory access validated: %s", dirname);
    }
    else
    {
        log_crit("access failed on directory %s: %s",
                 dirname, strerror(errno));
        return false;
    }

    struct statvfs vstats;
    if (statvfs(dirname, &vstats) == 0) {
        storage_config_->block_size_ = vstats.f_bsize;
        log_always("Internal storage block size = %zu bytes", storage_config_->block_size_);
    } else  {
        storage_config_->block_size_ = 4096;
        log_always("Internal storage block size defaulting to %zu bytes", storage_config_->block_size_);
    }

    return true;
}

} // namespace dtn
