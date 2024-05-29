/*
 *    Copyright 2005-2006 Intel Corporation
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
#  include <oasys-config.h>
#endif

#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "DurableStore.h"
#include "ExternalDurableStore.h"
#include "BerkeleyDBStore.h"
#include "ODBCMySQL.h"
#include "ODBCSQLite.h"
#include "ODBCStore.h"
#include "FileSystemStore.h"
#include "MemoryStore.h"
#include "StorageConfig.h"

namespace oasys {

template <>
DurableStore* Singleton<DurableStore, false>::instance_ = NULL;

DurableStore::~DurableStore()
{ 
    delete impl_; 
    impl_ = 0;

    if (clean_shutdown_file_ != "") {
        // try to remove it if it exists
        unlink(clean_shutdown_file_.c_str());
        
        int fd = creat(clean_shutdown_file_.c_str(), S_IRUSR);
        if (fd < 0) {
            log_err("error creating shutdown file '%s': %s",
                    clean_shutdown_file_.c_str(), strerror(errno));
        } else {
            log_debug("successfully created clean shutdown file '%s'",
                      clean_shutdown_file_.c_str());
            close(fd);
        }
    }
}

int
DurableStore::create_store(const StorageConfig& config,
                           bool*                clean_shutdown)
{
    ASSERT(impl_ == NULL);
    
    log_debug("At beginning of DurableStore::create_store: open_txid_ = %p.", open_txid_ );
    durably_close_next_transaction_ = false;
    num_nondurable_transactions_ = 0;
    max_nondurable_transactions_ = config.max_nondurable_transactions_;

    if (0) {} // symmetry

    // filesystem store
    else if (config.type_ == "filesysdb")
    {
        impl_ = new FileSystemStore(logpath_);
    }

    // memory backed store
    else if (config.type_ == "memorydb")
    {
        impl_ = new MemoryStore(logpath_);
    }

#if LIBDB_ENABLED
    // berkeley db
    else if (config.type_ == "berkeleydb")
    {
        impl_ = new BerkeleyDBStore(logpath_);
    }
#endif

#if LIBODBC_ENABLED
    else if (config.type_ == "odbc-sqlite")
    {
        impl_ = new ODBCDBSQLite(logpath_);
        log_debug("impl_ set to new ODBCDBSQLite");
    }
    else if (config.type_ == "odbc-mysql")
    {
        impl_ = new ODBCDBMySQL(logpath_);
        log_debug("impl_ set to new ODBCDBMySQL");
    }
#endif

#if defined(EXTERNAL_DS_ENABLED) && defined(XERCES_C_ENABLED)
    // external data store
    else if (config.type_ == "external")
    {
        impl_ = new ExternalDurableStore(logpath_);
    }
#endif

#if MYSQL_ENABLED
#error Native Mysql support not yet added to oasys - use ODBC/MySQL instead
#endif // MYSQL_ENABLED

#if POSTGRES_ENABLED
#error Postgres support not yet added to oasys
#endif // POSTGRES_ENABLED
        
    else
    {
        log_crit("configured storage type '%s' not implemented, exiting...",
                 config.type_.c_str());
        exit(1);
    }
    
    int err = impl_->init(config);
    if (err != 0)
    {
        log_err("can't initialize %s %d",
                config.type_.c_str(), err);
        return DS_ERR;
    }

    if (config.leave_clean_file_) {
        clean_shutdown_file_ = config.dbdir_;
        clean_shutdown_file_ += "/.ds_clean";
        
        // try to remove the clean shutdown file
        err = unlink(clean_shutdown_file_.c_str());
        if ((err == 0) ||
            (errno == ENOENT && config.init_ == true))
        {
            log_info("datastore %s was cleanly shut down",
                     config.dbdir_.c_str());
            if (clean_shutdown) {
                *clean_shutdown = true;
            }
        } else {
            log_info("datastore %s was not cleanly shut down",
                     config.dbdir_.c_str());
            if (clean_shutdown) {
                *clean_shutdown = false;
            }
        }
    }
    log_debug("At end of DurableStore::create_store: open_txid_ = %p.", open_txid_ );
    
    return DS_OK;
}

//----------------------------------------------------------------------------
int
DurableStore::create_finalize(const StorageConfig& config)
{
	ASSERT(impl_ != NULL);
	return impl_->create_finalize(config);
}

//----------------------------------------------------------------------------
int
DurableStore::begin_transaction(void **txid)
{
    int ret;

    ASSERT(impl_ != NULL);

//     if (transaction_lock_.is_locked_by_me()) {
//         log_debug("DurableStore::begin_transaction called while holding lock.");
//         if (txid!=NULL ) {
//             *txid = open_txid_;
//         }
//         return(0);
//     } else {
//         log_debug("DurableStore::begin_transaction taking transaction lock.");
//         transaction_lock_.lock("begin_transaction");
//     }

    log_debug("DurableStore::begin_transaction entered");

    if ( open_txid_ ) {
        log_debug("DurableStore::begin_transaction called with Tx already open.");
        if (txid!=NULL ) {
            *txid = open_txid_;
        }
        return(DS_OK);
    } else {
	    tx_counter_++;
	    log_debug("DurableStore::begin_transaction calling implementation begin_transaction for transaction %u", tx_counter_);
        ret = impl_->begin_transaction(&open_txid_);
        if ( ret==DS_ERR ) {
            log_warn("error in begin_transaction; releasing lock and DS_ERR");
            // transaction_lock_.unlock();
            return(DS_ERR);
        } else {
            if (txid!=NULL) {
                *txid=open_txid_;
            }
            return(DS_OK);
        }
    }

}

//----------------------------------------------------------------------------
int
DurableStore::end_transaction()
{
    int ret;

    ASSERT(impl_ != NULL);
//    ASSERT(transaction_lock_.is_locked_by_me());

    log_debug("DurableStore::end_transaction - durable (%d/%d), transaction %u.",
    		  num_nondurable_transactions_, max_nondurable_transactions_,
    		  tx_counter_);

    if (++num_nondurable_transactions_>max_nondurable_transactions_) {
    	durably_close_next_transaction_ = true;
    	log_debug("DurableStore::EndTranaction: Committing this time.");
    }

    ret = impl_->end_transaction(open_txid_, durably_close_next_transaction_);
    open_txid_ = NULL;
    log_debug("DurableStore::end_transaction - releasing transaction lock.");
    // transaction_lock_.unlock();
	if (durably_close_next_transaction_) {
		log_debug("DurableStore::end_transaction -- resetting durable count.");
		durably_close_next_transaction_ = false;
		num_nondurable_transactions_ = 0;
	}
    if ( ret == DS_OK ) {
    	return(ret);
    } else {
    	return(DS_BUSY);  //  Elwyn: Why not DS_ERR??
    }

}

//----------------------------------------------------------------------------
int
DurableStore::is_transaction_open()
{
	if ( open_txid_ != NULL ) {
		log_debug("DurableStore::is_transaction_open returning true.");
        return true;
    } else {
		log_debug("DurableStore::is_transaction_open returning false.");
        return false;
    }
}

//----------------------------------------------------------------------------
int 
DurableStore::get_table(StaticTypedDurableTable** table, 
                        std::string               table_name,
                        int                       flags,
                        DurableObjectCache<SerializableObject>* cache)
{
    ASSERT(impl_ != NULL);
    ASSERT(cache == 0); // no cache for now

    // XXX/bowei -- can't support tables that require 
    // prototyping...
    PrototypeVector prototypes;  

    DurableTableImpl* table_impl;
    int err = impl_->get_table(&table_impl, table_name, flags, prototypes);
    if (err != 0) {
        return err;
    }

    *table = new StaticTypedDurableTable(table_impl, table_name);
    return 0;
}

//----------------------------------------------------------------------------
int
DurableStore::get_table_names(StringVector* table_names)
{
    ASSERT(impl_ != NULL);
    int err = impl_->get_table_names(table_names);
    return err;
}

//----------------------------------------------------------------------------
std::string
DurableStore::get_info() const
{
    ASSERT(impl_ != NULL);
    return impl_->get_info();
}

//----------------------------------------------------------------------------
void
DurableStore::make_transaction_durable()
{
	durably_close_next_transaction_ = true;
}

//----------------------------------------------------------------------------
bool
DurableStore::aux_tables_available()
{
    ASSERT(impl_ != NULL);
	return impl_->aux_tables_available();
}

} // namespace oasys
