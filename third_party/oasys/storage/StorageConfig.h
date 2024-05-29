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

#ifndef _OASYS_STORAGE_CONFIG_H_
#define _OASYS_STORAGE_CONFIG_H_

#include <string>
#include "../debug/DebugUtils.h"

namespace oasys {

/*!
 * Simple singleton class that just contains the storage-specific
 * configuration variables. An instance of this configuration is given
 * to Durable storage system to initialize it.
 */
struct StorageConfig {
    // General options that must be set (in the constructor)
    std::string cmd_;           ///< tcl command name for this instance
    std::string type_;          ///< storage type [berkeleydb/mysql/postgres/external]
    std::string dbname_;        ///< Database name (filename in berkeley db)
    std::string dbdir_;         ///< Path to the database files

    // Other general options
    bool        init_;          ///< Create new databases on init
    bool        tidy_;          ///< Prune out the database on init
    int         tidy_wait_;     ///< Seconds to wait before tidying
    bool        leave_clean_file_;///< Leave a .ds_clean file on clean shutdown
    bool		auto_commit_;   ///< True if database auto-commit (if supported) is on
    int         max_nondurable_transactions_; // Maximum number of non-durable
                                              // transactions before trying to close
                                              // and save durably.
    // Filesystem DB Specific options
    int         fs_fd_cache_size_; ///< If > 0, then this # of open
                                   /// fds will be cached

    // Berkeley DB Specific options
    bool        db_mpool_;      ///< Use DB mpool (default true)
    bool        db_log_;        ///< Use DB log subsystem
    bool        db_txn_;        ///< Use DB transaction
    int         db_max_tx_;     ///< Max # of active transactions (0 for default)
    int         db_max_locks_;  ///< Max # of active locks (0 for default)
    int         db_max_lockers_;///< Max # of active locking threads (0 for default)
    int         db_max_lockedobjs_;///< Max # of active locked objects (0 for default)
    int         db_max_logregion_; ///< Logging region max (0 for default)
    int         db_lockdetect_; ///< Frequency in msecs to check for deadlocks
                                ///< (locking disabled if zero)
    bool        db_sharefile_;  ///< Share a single DB file (and a lock)

    // External data store specific options
    u_int16_t   server_port_;   ///< server port to connect to (on localhost)
    std::string schema_;        ///< xml schema for remote interface

    // ODBC/SQL data store specific options
    bool		odbc_use_aux_tables_; 		///< Whether to use auxiliary tables
    std::string odbc_schema_pre_creation_;	///< Pathname for file with extra SQL
    								  	  	///< to initialize schema, such as
    										///< definitions of aux tables.
    std::string odbc_schema_post_creation_;	///< Pathname for file with extra SQL
    								  	  	///< to finalize schema, such as
    										///< definitions of triggers to be
    										///< addded once all tables are in place.
    u_int16_t	odbc_mysql_keep_alive_interval_;
    								  	    ///< Keep alive timer interval (MySQL only)

    StorageConfig(
        const std::string& cmd,
        const std::string& type,
        const std::string& dbname,
        const std::string& dbdir)
        
      : cmd_(cmd),
        type_(type),
        dbname_(dbname),
        dbdir_(dbdir),
        
        init_(false),
        tidy_(false),
        tidy_wait_(3),
        leave_clean_file_(true),

        auto_commit_(true),
        max_nondurable_transactions_(0),

        fs_fd_cache_size_(0),

        db_mpool_(true),
        db_log_(true),
        db_txn_(true),
        db_max_tx_(0),
        db_max_locks_(0),
        db_max_lockers_(0),
        db_max_lockedobjs_(0),
        db_max_logregion_(0),
        db_lockdetect_(5000),
        db_sharefile_(false),

        server_port_(0),

        odbc_use_aux_tables_(false),
        odbc_schema_pre_creation_(""),
        odbc_schema_post_creation_(""),
        odbc_mysql_keep_alive_interval_(10)

    {}
};

} // namespace oasys

#endif /* _STORAGE_CONFIG_H_ */
