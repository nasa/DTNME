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

#include "StorageCommand.h"
#include "bundling/BundlePayload.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleDaemonStorage.h"
#include "bundling/FormatUtils.h"
#include "storage/BundleStore.h"
#include "storage/DTNStorageConfig.h"

namespace dtn {

StorageCommand::StorageCommand(DTNStorageConfig* cfg)
    : TclCommand(cfg->cmd_.c_str()),
      config_(cfg)
{
    static oasys::EnumOpt::Case PayloadLocationCases[] = {
        {"memory",   BundlePayload::MEMORY},
        {"disk",     BundlePayload::DISK},
        {"nodata",   BundlePayload::NODATA},
        {0, 0}
    };
    
    inited_ = false;
    
    bind_var(new oasys::StringOpt("type", &cfg->type_,
				"type", "What storage system to use "
				"(default is berkeleydb).\n"
		"	valid options:\n"
		"			berkeleydb\n"
		"			external"));

    bind_var(new oasys::StringOpt("dbname", &cfg->dbname_,
				"name", "The database name (default "
				"DB)\n"
		"	valid options:	string"));

    bind_var(new oasys::StringOpt("dbdir", &cfg->dbdir_,
				"dir", "The database directory (default "
				"/var/dtn/db).\n"
		"	valid options:	directory"));

    bind_var(new oasys::BoolOpt("init_db", &cfg->init_,
				"Same as the --init-db argument to dtnd. "
				"Initialize the database on startup "
				"(default false)\n"
		"	valid options:	true or false"));

    bind_var(new oasys::BoolOpt("tidy", &cfg->tidy_,
				"Same as the --tidy argument to dtnd. "
				"Cleans out the database and the "
				"bundle directories (default false)\n"
		"	valid options:	true or false"));

    bind_var(new oasys::IntOpt("tidy_wait", &cfg->tidy_wait_,
				"time",
				"How long to wait before really doing "
				"the tidy operation (default 3)\n"
		"	valid options:	number"));

    bind_var(new oasys::BoolOpt("leave_clean_file", &cfg->leave_clean_file_,
				"Leave a .ds_clean file on clean "
				"shutdown (default true)\n"
		"	valid options:	true or false"));

    bind_var(new oasys::IntOpt("fs_fd_cache_size", &cfg->fs_fd_cache_size_, 
				"num", "number of open fds to cache "
				"using Filesysyem DB. If > zero then "
				"this # of open fds will be cached "
				"(default 0)\n"
		"	valid options:	number"));

    bind_var(new oasys::BoolOpt("auto_commit", &cfg->auto_commit_,
				"whether auto-commit (if supported) is on or not "
    		    "default is true (auto-commit on)\n"
		"	valid options:	true or false"));

    bind_var(new oasys::IntOpt("max_nondurable_transactions", &cfg->max_nondurable_transactions_,
				"num", "max number of database "
				"transactions before forcing a sync to"
				"disk (default 0 (sync every trans))\n"
		"	valid options:	number"));

    bind_var(new oasys::BoolOpt("db_mpool", &cfg->db_mpool_,
				"use mpool in Berkeley DB (default "
				"true)"
		"	valid options:	true or false"));

    bind_var(new oasys::BoolOpt("db_log", &cfg->db_log_,
				"use logging in Berkeley DB (default "
				"true)\n"
		"	valid options:	true or false"));

    bind_var(new oasys::BoolOpt("db_txn", &cfg->db_txn_,
				"use transactions in Berkeley DB "
				"(default true)\n"
		"	valid options:	true or false"));

    bind_var(new oasys::BoolOpt("db_sharefile", &cfg->db_sharefile_,
				"use shared database file (and a "
				"lock) in Berkeley DB (default false)\n"
		"	valid options:	true or false"));

    bind_var(new oasys::IntOpt("db_max_tx", &cfg->db_max_tx_,
				"num", "max # of active transactions "
				"in Berkeley DB (default 0)\n"
		"	valid options:	number"));

    bind_var(new oasys::IntOpt("db_max_locks", &cfg->db_max_locks_,
				"num", "max # of active locks in "
				"Berkeley DB (default 0)\n"
		"	valid options:	number"));

    bind_var(new oasys::IntOpt("db_max_lockers", &cfg->db_max_lockers_,
				"num", "max # of active locking threads in "
				"Berkeley DB (default 0)\n"
		"	valid options:	number"));

    bind_var(new oasys::IntOpt("db_max_lockedobjs", &cfg->db_max_lockedobjs_,
				"num", "max # of active locked objects in "
				"Berkeley DB (default 0)\n"
		"	valid options:	number"));

    bind_var(new oasys::IntOpt("db_lockdetect", &cfg->db_lockdetect_,
				"num", "frequency to check for Berkeley "
				"DB deadlocks (default 5000 and "
                               "zero disables locking)\n"
		"	valid options:	number"));

    bind_var(new oasys::StringOpt("payloaddir", &cfg->payload_dir_,
	                           "dir", "directory for payloads while in "
		    		"transit (default is /var/dtn/bundles)\n"
		"	valid options:	directory"));
    
    bind_var(new oasys::SizeOpt("payload_quota",
                                  &cfg->payload_quota_,
                                  "bytes", "storage quota in bytes for bundle "
				"payloads (default 0 - is unlimited; magnitude chars allowed)\n"
		"	valid options:	number[K | M | G]"));
    
    bind_var(new oasys::UIntOpt("payload_fd_cache_size",
                                &cfg->payload_fd_cache_size_,
                                "num", "number of payload file descriptors to keep "
                                "open in a cache (default 32)\n"
		"	valid options:	number"));

    bind_var(new oasys::UInt16Opt("server_port",
                                  &cfg->server_port_,
                                  "port number",
                                  "TCP port for IPC to external data "
				"store on localhost (default 0)\n"
		"	valid options:	number"));

    bind_var(new oasys::StringOpt("schema", &cfg->schema_,
                                  "pathname", "File containing the XML schema for the "
                                  "external/remote data store interface\n"
		"	valid options:	filename"));

    bind_var(new oasys::BoolOpt("odbc_use_aux_tables", &cfg->odbc_use_aux_tables_,
				"write broken out fields from stored items into "
				"auxiliary database table(s) in ODBC/SQL DBs (default false)\n"
		"	valid options:	true or false"));

    bind_var(new oasys::StringOpt("odbc_schema_pre_creation", &cfg->odbc_schema_pre_creation_,
                                  "pathname", "File containing SQL Commands "
                                  "to initialize database schema before main tables created.\n"
		"	valid options:	filename"));

    bind_var(new oasys::StringOpt("odbc_schema_post_creation", &cfg->odbc_schema_post_creation_,
                                  "pathname", "File containing SQL Commands "
                                  "to initialize database schema after main tables created.\n"
		"	valid options:	filename"));

    bind_var(new oasys::UInt16Opt("odbc_mysql_keep_alive_interval",
    		 &cfg->odbc_mysql_keep_alive_interval_, "num",
			 "set interval in minutes between keep alive pings to "
			 "MySQL server to avoid connections being terminated (default 10)\n"
    		 "	valid options:	positive integer"));

    bind_var(new oasys::UIntOpt("interval",
                                &BundleDaemonStorage::params_.db_storage_ms_interval_,
                                "milliseconds",
                                "Millisecond interval between database updates"));

    bind_var(new oasys::BoolOpt("db_log_auto_removal",
                                &BundleDaemonStorage::params_.db_log_auto_removal_,
				"use log file auto removal "
				"in Berkeley DB (default false)\n"
        		"	valid options:	true or false"));

    bind_var(new oasys::BoolOpt("db_storage_enabled",
                                &BundleDaemonStorage::params_.db_storage_enabled_,
				"enable or disable writing to the database (default true)\n"
                "        (payload still written to disk)\n"
                "	valid options:	true or false"));

    bind_var(new oasys::BoolOpt("db_force_sync_to_disk",
                                &BundleDaemonStorage::params_.db_force_sync_to_disk_,
				"whether to force payload files to sync to disk (default true)\n"
        		"	valid options:	true or false"));

    bind_var(new oasys::EnumOpt("payload_location",
                                PayloadLocationCases,
                                (int*)&BundleDaemonStorage::params_.payload_location_,
                                "memory | disk | nodata",
                                "where bundle payloads should be stored (nodata is only used for testing)"));
    
    add_to_help("usage", "print the current storage usage");
    add_to_help("stats", "print storage statistics");
    add_to_help("heap", "print heap statistics");
}

//----------------------------------------------------------------------
int
StorageCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        resultf("need a storage subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (!strcmp(cmd, "usage")) {
        // storage usage
        size_t total_size = BundleStore::instance()->total_size();
        size_t max_size = BundleStore::instance()->max_size();

        size_t total_disk_size = BundleStore::instance()->total_disk_size();
        size_t max_disk_size = BundleStore::instance()->max_disk_size();

        resultf("payload bytes: %14" PRIu64 "  (%s)\n"
                "          max: %14" PRIu64 "  (%s)\n\n" 

                "    directory: %s\n"
                "   file bytes: %14" PRIu64 "  (%s)\n"
                "        quota: %14" PRIu64 "  (%s)\n"
                "          max: %14" PRIu64 "  (%s)\n", 

                total_size,
                FORMAT_WITH_MAG(total_size).c_str(),
                max_size,
                FORMAT_WITH_MAG(max_size).c_str(),

                BundleStore::instance()->payload_dir().c_str(),
                total_disk_size,
                FORMAT_WITH_MAG(total_disk_size).c_str(),
                config_->payload_quota_,
                FORMAT_WITH_MAG(config_->payload_quota_).c_str(),
                max_disk_size,
                FORMAT_WITH_MAG(max_disk_size).c_str());

        return TCL_OK;
    }
    else if (!strcmp(cmd, "stats")) {
        // storage statistics
        oasys::StringBuffer buf("Storage Statistics: ");
        BundleDaemon::instance()->storage_get_stats(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    }
    else if (!strcmp(cmd, "heap")) {
        // heap statistics
        malloc_stats();
        malloc_info(0, stdout);
        oasys::StringBuffer buf("Heap statistics should have been written to stdout");
        return TCL_OK;
    }

    resultf("unknown storage subcommand %s", cmd);
    return TCL_ERROR;
}

} // namespace dtn
