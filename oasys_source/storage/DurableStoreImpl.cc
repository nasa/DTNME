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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "DurableStore.h"
#include "debug/DebugUtils.h"
#include "serialize/MarshalSerialize.h"

namespace oasys {

int
DurableStoreImpl::begin_transaction(void **txid)
{
	(void) txid;

    log_debug("DurableStoreImpl::begin_transaction not implemented.");
    /*
     * Even if the underlying implementation doens't support transactions,
     * we track the number of open transactions so that
     * developing against non-transactionalized databases has a lower
     * risk of breaking transactionalized ones.
     */
    return(DS_OK);
}

int
DurableStoreImpl::end_transaction(void *txid, bool be_durable)
{
	(void) be_durable;
	(void) txid;
    log_debug("DurableStoreImpl::end_transaction not implemented.");
    /*
     * Even if the underlying implementation doens't support transactions,
     * we track the number of open transactions so that
     * developing against non-transactionalized databases has a lower
     * risk of breaking transactionalized ones.
     */
    return(DS_OK);
}

void *
DurableStoreImpl::get_underlying()
{
    log_debug("DurableStoreImpl::get_underlying not implemented.");
    return(NULL);
}

void
DurableStoreImpl::prune_db_dir(const char* dir, int tidy_wait)
{
    char cmd[256];
    for (int i = tidy_wait; i > 0; --i) 
    {
        log_warn("PRUNING CONTENTS OF %s IN %d SECONDS", dir, i);
        sleep(1);
    }
    sprintf(cmd, "/bin/rm -rf %s", dir);
    log_notice("tidy option removing directory '%s'", cmd);
    system(cmd);
}

int
DurableStoreImpl::check_db_dir(const char* db_dir, bool* dir_exists)
{
    *dir_exists = false;

    struct stat f_stat;
    if (stat(db_dir, &f_stat) == -1)
    {
        if (errno == ENOENT)
        {
            *dir_exists = false;
        }
        else 
        {
            log_err("error trying to stat database directory %s: %s",
                    db_dir, strerror(errno));
            return DS_ERR;
        }
    }
    else
    {
        *dir_exists = true;
    }

    return 0;
}

int
DurableStoreImpl::create_db_dir(const char* db_dir)
{
    // create database directory
    char pwd[PATH_MAX];
    
    log_notice("creating new database directory %s%s%s",
               db_dir[0] == '/' ? "" : getcwd(pwd, PATH_MAX),
               db_dir[0] == '/' ? "" : "/",
               db_dir);
            
    if (mkdir(db_dir, 0700) != 0) 
    {
        log_crit("can't create datastore directory %s: %s",
                 db_dir, strerror(errno));
        return DS_ERR;
    }
    return 0;
}

// Default implementation - auxiliary tables are not available unless specifically
// implemented by derived class and enabled by configuration.
bool
DurableStoreImpl::aux_tables_available()
{
	return false;
}

int
DurableTableImpl::get(const SerializableObject&   key,
                      SerializableObject**        data,
                      TypeCollection::Allocator_t allocator)
{
    (void)key;
    (void)data;
    (void)allocator;
    PANIC("Generic DurableTableImpl get method called for "
          "multi-type tables");
}

size_t
DurableTableImpl::flatten(const SerializableObject& key, 
                          u_char* key_buf, size_t size)
{
    MarshalSize sizer(Serialize::CONTEXT_LOCAL);
    sizer.action(&key);

    if (sizer.size() > size)
    {
        return 0;
    }

    Marshal marshaller(Serialize::CONTEXT_LOCAL, key_buf, size);
    marshaller.action(&key);
    
    return sizer.size();
}

} // namespace oasys
