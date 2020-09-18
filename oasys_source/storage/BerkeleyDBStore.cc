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
#  include <oasys-config.h>
#endif

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include <debug/DebugUtils.h>
#include <io/FileUtils.h>

#include <util/StringBuffer.h>
#include <util/Pointers.h>
#include <util/ScratchBuffer.h>

#include <serialize/MarshalSerialize.h>
#include <serialize/TypeShims.h>

#include "BerkeleyDBStore.h"
#include "StorageConfig.h"
#include "util/InitSequencer.h"

#if LIBDB_ENABLED

#define NO_TX  0 // for easily going back and changing TX id's later

namespace oasys {
/******************************************************************************
 *
 * BerkeleyDBStore
 *
 *****************************************************************************/
const std::string BerkeleyDBStore::META_TABLE_NAME("___META_TABLE___");
//----------------------------------------------------------------------------
BerkeleyDBStore::BerkeleyDBStore(const char* logpath)
    : DurableStoreImpl("BerkeleyDBStore", logpath),
      init_(false)
{}

//----------------------------------------------------------------------------
BerkeleyDBStore::~BerkeleyDBStore()
{
    StringBuffer err_str;

    err_str.append("Tables still open at deletion time: ");
    bool busy = false;

    for (RefCountMap::iterator iter = ref_count_.begin(); 
         iter != ref_count_.end(); ++iter)
    {
        if (iter->second != 0)
        {
            err_str.appendf("%s ", iter->first.c_str());
            busy = true;
        }
    }

    if (busy)
    {
        log_err("%s", err_str.c_str());
    }

    if (deadlock_timer_) {
        deadlock_timer_->cancel();
    }
    
    dbenv_->close(dbenv_, 0);
    dbenv_ = 0;
    log_info("db closed");
}

//----------------------------------------------------------------------------
int 
BerkeleyDBStore::init(const StorageConfig& cfg)
{
    std::string dbdir = cfg.dbdir_;
    FileUtils::abspath(&dbdir);

    db_name_ = cfg.dbname_;
    sharefile_ = cfg.db_sharefile_;

    // XXX/bowei need to expose options if needed later
    if (cfg.tidy_) {
        prune_db_dir(dbdir.c_str(), cfg.tidy_wait_);
    }

    bool db_dir_exists;
    int  err = check_db_dir(dbdir.c_str(), &db_dir_exists);
    if (err != 0)
    {
        return DS_ERR;
    }
    if (!db_dir_exists) 
    {
        if (cfg.init_) {
            if (create_db_dir(dbdir.c_str()) != 0) {
                return DS_ERR;
            }
        } else {
            log_crit("DB dir %s does not exist and not told to create!",
                     dbdir.c_str());
            return DS_ERR;
        }
    }

    db_env_create(&dbenv_, 0);
    if (dbenv_ == 0) 
    {
        log_crit("Can't create db env");
        return DS_ERR;
    }

    dbenv_->set_errcall(dbenv_, BerkeleyDBStore::db_errcall);

    log_info("initializing db name=%s (%s), dir=%s",
             db_name_.c_str(), sharefile_ ? "shared" : "not shared",
             dbdir.c_str());

#define SET_DBENV_OPTION(_opt, _fn)                     \
    if (cfg._opt != 0) {                                \
        err = dbenv_->_fn(dbenv_, cfg._opt);            \
                                                        \
        if (err != 0)                                   \
        {                                               \
            log_crit("DB: %s, cannot %s to %d",         \
                     db_strerror(err), #_fn, cfg._opt); \
            return DS_ERR;                              \
        }                                               \
    } 

    SET_DBENV_OPTION(db_max_tx_, set_tx_max);
    SET_DBENV_OPTION(db_max_locks_, set_lk_max_locks);
    SET_DBENV_OPTION(db_max_lockers_, set_lk_max_lockers);
    SET_DBENV_OPTION(db_max_lockedobjs_, set_lk_max_objects);
    SET_DBENV_OPTION(db_max_logregion_, set_lg_regionmax);

#undef SET_DBENV_OPTION

    int dbenv_opts =
        DB_CREATE  | 	// create new files
        DB_PRIVATE	// only one process can access the db
        ;

    if (cfg.db_lockdetect_ != 0) { // locking
        dbenv_opts |= DB_INIT_LOCK | DB_THREAD;
    }

    if (cfg.db_mpool_) { // memory pool
        dbenv_opts |= DB_INIT_MPOOL;
    }

    if (cfg.db_log_) { // logging
        dbenv_opts |= DB_INIT_LOG;
    }

    if (cfg.db_txn_) { // transactions / recovery
        dbenv_opts |= DB_INIT_TXN | DB_RECOVER;
    }
    
    err = dbenv_->open(dbenv_, dbdir.c_str(), dbenv_opts, 0 /* default mode */);
    
    if (err != 0) 
    {
        log_crit("DB: %s, cannot open database", db_strerror(err));
        return DS_ERR;
    }

    if (cfg.db_txn_) {
        err = dbenv_->set_flags(dbenv_,
                                DB_AUTO_COMMIT, // every operation is a tx
                                1);
        if (err != 0) 
        {
            log_crit("DB: %s, cannot set flags", db_strerror(err));
            return DS_ERR;
        }
    }

    err = dbenv_->set_paniccall(dbenv_, BerkeleyDBStore::db_panic);
    
    if (err != 0) 
    {
        log_crit("DB: %s, cannot set panic call", db_strerror(err));
        return DS_ERR;
    }

    if (cfg.db_lockdetect_ != 0) {
        deadlock_timer_ = new DeadlockTimer(logpath_, dbenv_, cfg.db_lockdetect_);
        deadlock_timer_->reschedule();
    } else {
        deadlock_timer_ = NULL;
    }
    
    init_ = true;

    return 0;
}

//----------------------------------------------------------------------------
int
BerkeleyDBStore::begin_transaction(void **txid)
{
#if 0
    DB_TXN *txid_local;
    u_int32_t flags = 0x0;
    int ret;

    ret = dbenv_->txn_begin(dbenv_, NULL, &txid_local, flags);
    if ( ret!=0 ) {
        if ( ret==DB_RUNRECOVERY ) {
            PANIC("RUN DB Recovery on fooDB.");
        }
        return(DS_ERR);
    }
    if ( txid != NULL ) {
        *txid = (void *)txid_local;
    }
#else
    if ( txid != nullptr) {
        *txid = nullptr;
    }
#endif

    return 0;
}

//----------------------------------------------------------------------------
int
BerkeleyDBStore::end_transaction(void *txid, bool be_durable)
{
#if 0
    int ret;
    u_int32_t flags = 0x0;
    DB_TXN *txp = (DB_TXN *) txid;

    log_debug("BerkeleyDBStore::end_transaction: txid = %p", txid);

    if ( be_durable ) {
    	log_debug("BerkeleyDBStore::end_transaction called with be_durable TRUE");
    }

    ret = txp->commit(txp, flags);
#else
    (void) txid;
    (void) be_durable;
    int ret = 0;
#endif

    return ret;
}

//----------------------------------------------------------------------------
void *
BerkeleyDBStore::get_underlying()
{
    return((void*) dbenv_);
}

//----------------------------------------------------------------------------
int
BerkeleyDBStore::get_table(DurableTableImpl** table,
                           const std::string& name,
                           int                flags,
                           PrototypeVector&   prototypes)
{
    (void)prototypes;
    
    DB* db;
    int err;
    DBTYPE db_type = DB_BTREE;
    u_int32_t db_flags;
                               
    ASSERT(init_);

    // grab a new database handle
    err = db_create(&db, dbenv_, 0);
    if (err != 0) {
        log_err("error creating database handle: %s", db_strerror(err));
        return DS_ERR;
    }
    
    // calculate the db type and creation flags
    db_flags = 0;
    
    if (flags & DS_CREATE) {
        db_flags |= DB_CREATE;

        if (flags & DS_EXCL) {
            db_flags |= DB_EXCL;
        }

        if (((flags & DS_BTREE) != 0) && ((flags & DS_HASH) != 0)) {
            PANIC("both DS_HASH and DS_BTREE were specified");
        }
        
        if (flags & DS_HASH)
        {
            db_type = DB_HASH;
        }
        else if (flags & DS_BTREE)
        {
            db_type = DB_BTREE;
        }
        else // XXX/demmer force type to be specified??
        {
            db_type = DB_BTREE;
        }

    } else {
        db_type = DB_UNKNOWN;
    }

    if (deadlock_timer_) {
        // locking is enabled
        db_flags |= DB_THREAD;
    }

 retry:
    if (sharefile_) {
        oasys::StaticStringBuffer<128> dbfile("%s.db", db_name_.c_str());
        err = db->open(db, NO_TX, dbfile.c_str(), name.c_str(),
                       db_type, db_flags, 0);
    } else {
        oasys::StaticStringBuffer<128> dbname("%s-%s.db",
                                              db_name_.c_str(), name.c_str());
        err = db->open(db, NO_TX, dbname.c_str(), NULL,
                       db_type, db_flags, 0);
    }
        
    if (err == ENOENT)
    {
        log_debug("get_table -- notfound database %s", name.c_str());
        db->close(db, 0);
        return DS_NOTFOUND;
    }
    else if (err == EEXIST)
    {
        log_debug("get_table -- already existing database %s", name.c_str());
        db->close(db, 0);
        return DS_EXISTS;
    }
    else if (err == DB_LOCK_DEADLOCK)
    {
        log_warn("deadlock in get_table, retrying operation");
        goto retry;
    }
    else if (err != 0)
    {
        log_err("DB internal error in get_table: %s", db_strerror(err));
        db->close(db, 0);
        return DS_ERR;
    }

    if (db_type == DB_UNKNOWN) {
        err = db->get_type(db, &db_type);
        if (err != 0) {
            log_err("DB internal error in get_type: %s", db_strerror(err));
            db->close(db, 0);
            return DS_ERR;
        }
    }
    
    log_debug("get_table -- opened table %s type %d", name.c_str(), db_type);

    *table = new BerkeleyDBTable(logpath_, this, name, (flags & DS_MULTITYPE), 
                                 db, db_type);

    return 0;
}

//----------------------------------------------------------------------------
int
BerkeleyDBStore::del_table(const std::string& name)
{
    int err;
    
    ASSERT(init_);

    if (ref_count_[name] != 0)
    {
        log_info("Trying to delete table %s with %d refs still on it",
                 name.c_str(), ref_count_[name]);
        
        return DS_BUSY;
    }

    log_info("deleting table %s", name.c_str());

    if (sharefile_) {
        oasys::StaticStringBuffer<128> dbfile("%s.db", db_name_.c_str());
        err = dbenv_->dbremove(dbenv_, NO_TX, dbfile.c_str(), name.c_str(), 0);
    } else {
        oasys::StaticStringBuffer<128> dbfile("%s-%s.db",
                                              db_name_.c_str(), name.c_str());
        err = dbenv_->dbremove(dbenv_, NO_TX, dbfile.c_str(), NULL, 0);
    }

    if (err != 0) {
        log_err("del_table %s", db_strerror(err));

        if (err == ENOENT) 
        {
            return DS_NOTFOUND;
        }
        else 
        {
            return DS_ERR;
        }
    }
    
    ref_count_.erase(name);

    return 0;
}

//----------------------------------------------------------------------------
int 
BerkeleyDBStore::get_table_names(StringVector* names)
{
    names->clear();
    
    if (sharefile_) 
    {
        BerkeleyDBTable* metatable;
        int err = get_meta_table(&metatable);
        
        if (err != DS_OK) {
            return err;
        }
        
        // unfortunately, we can't use the standard serialization stuff
        // for the metatable, because the string stored in the metatable are
        // not null-terminated
        DBC* cursor = 0;
        err = metatable->db_->cursor(metatable->db_, NO_TX, &cursor, 0);
        if (err != 0) 
        {
            log_err("cannot create iterator for metatable, err=%s",
                    db_strerror(err));
            return DS_ERR;
        }

        for (;;) 
        {
            DBTRef key, data;
            err = cursor->c_get(cursor, key.dbt(), data.dbt(), DB_NEXT);
            if (err == DB_NOTFOUND) 
            {
                break;
            }
            else if (err != 0)
            {
                log_err("error getting next item with iterator, err=%s",
                        db_strerror(err));
                return DS_ERR;
            }
            names->push_back(std::string(static_cast<char*>(key->data),
                                         key->size));
        }

        if (cursor) 
        {
            err = cursor->c_close(cursor);
            if (err != 0) 
            {
                log_err("DB: cannot close cursor, %s", db_strerror(err));
                return DS_ERR;
            }
        }
        delete_z(metatable);
    } 
    else 
    {
        // XXX/bowei -- TODO
        NOTIMPLEMENTED;
    }

    return 0;
}

//----------------------------------------------------------------------------
std::string 
BerkeleyDBStore::get_info() const
{
    StringBuffer desc;

    return "BerkeleyDB";
}

//----------------------------------------------------------------------------
int  
BerkeleyDBStore::get_meta_table(BerkeleyDBTable** table)
{
    DB* db;
    int err;
    
    ASSERT(init_);

    if (! sharefile_) {
        log_err("unable to open metatable for an unshared berkeley db");
        return DS_ERR;
    }

    err = db_create(&db, dbenv_, 0);
    if (err != 0) {
        log_err("Can't create db pointer");
        return DS_ERR;
    }
    
    oasys::StaticStringBuffer<128> dbfile("%s.db", db_name_.c_str());    
    err = db->open(db, NO_TX, dbfile.c_str(), 
                   NULL, DB_UNKNOWN, DB_RDONLY, 0);
    if (err != 0) {
        log_err("unable to open metatable - DB: %s", db_strerror(err));
        return DS_ERR;
    }

    DBTYPE type;
    err = db->get_type(db, &type);
    if (err != 0) {
        log_err("unable to get metatable type - DB: %s", db_strerror(err));
        return DS_ERR;
    }
    
    *table = new BerkeleyDBTable(logpath_, this, META_TABLE_NAME, false, db, type);
    
    return 0;
}

//----------------------------------------------------------------------------
int
BerkeleyDBStore::acquire_table(const std::string& table)
{
    ASSERT(init_);

    ++ref_count_[table];
    ASSERT(ref_count_[table] >= 0);

    log_debug("table %s, +refcount=%d", table.c_str(), ref_count_[table]);

    return ref_count_[table];
}

//----------------------------------------------------------------------------
int
BerkeleyDBStore::release_table(const std::string& table)
{
    ASSERT(init_);

    --ref_count_[table];
    ASSERT(ref_count_[table] >= 0);

    log_debug("table %s, -refcount=%d", table.c_str(), ref_count_[table]);

    return ref_count_[table];
}

//----------------------------------------------------------------------------
#if (DB_VERSION_MINOR >= 3) || (DB_VERSION_MAJOR >= 5)
void
BerkeleyDBStore::db_errcall(const DB_ENV* dbenv,
                            const char* errpfx,
                            const char* msg)
{
    (void)dbenv;
    (void)errpfx;
    log_err_p("/storage/berkeleydb", "DB internal error: %s", msg);
}

#else

//----------------------------------------------------------------------------
void
BerkeleyDBStore::db_errcall(const char* errpfx, char* msg)
{
    (void)errpfx;
    log_err_p("/storage/berkeleydb", "DB internal error: %s", msg);
}

#endif

//----------------------------------------------------------------------------
void
BerkeleyDBStore::db_panic(DB_ENV* dbenv, int errval)
{
    (void)dbenv;
    PANIC("fatal berkeley DB internal error: %s", db_strerror(errval));
}

//----------------------------------------------------------------------------
void
BerkeleyDBStore::DeadlockTimer::reschedule()
{
    log_debug("rescheduling in %d msecs", frequency_);
    schedule_in(frequency_);
}

//----------------------------------------------------------------------------
void
BerkeleyDBStore::DeadlockTimer::timeout(const struct timeval& now)
{
    (void)now;
    int aborted = 0;
    log_debug("running deadlock detection");
    dbenv_->lock_detect(dbenv_, 0, DB_LOCK_YOUNGEST, &aborted);

    if (aborted != 0) {
        log_warn("deadlock detection found %d aborted transactions", aborted);
    }

    reschedule();
}

/******************************************************************************
 *
 * BerkeleyDBTable
 *
 *****************************************************************************/
BerkeleyDBTable::BerkeleyDBTable(const char* logpath,
                                 BerkeleyDBStore* store,
                                 const std::string& table_name,
                                 bool multitype,
                                 DB* db, DBTYPE db_type)
    : DurableTableImpl(table_name, multitype),
      Logger("BerkeleyDBTable", "%s/%s", logpath, table_name.c_str()),
      db_(db), db_type_(db_type), store_(store)
{
    store_->acquire_table(table_name);
}

//----------------------------------------------------------------------------
BerkeleyDBTable::~BerkeleyDBTable() 
{
    // Note: If we are to multithread access to the same table, this
    // will have potential concurrency problems, because close can
    // only happen if no other instance of Db is around.
    store_->release_table(name());

    log_debug("closing db %s", name());
    db_->close(db_, 0); // XXX/bowei - not sure about comment above
    db_ = NULL;
}

//----------------------------------------------------------------------------
int 
BerkeleyDBTable::get(const SerializableObject& key, 
                     SerializableObject*       data)
{
    ASSERTF(!multitype_, "single-type get called for multi-type table");

    ScratchBuffer<u_char*, 256> key_buf;
    size_t key_buf_len = flatten(key, &key_buf);
    ASSERT(key_buf_len != 0);

    DBTRef k(key_buf.buf(), key_buf_len);
    DBTRef d;

    int err = db_->get(db_, NO_TX, k.dbt(), d.dbt(), 0);
     
    if (err == DB_NOTFOUND) 
    {
        return DS_NOTFOUND;
    }
    else if (err != 0)
    {
        log_err("DB: %s", db_strerror(err));
        return DS_ERR;
    }

    u_char* bp = (u_char*)d->data;
    size_t  sz = d->size;
    
    Unmarshal unmarshaller(Serialize::CONTEXT_LOCAL, bp, sz);
    
    if (unmarshaller.action(data) != 0) {
        log_err("DB: error unserializing data object");
        return DS_ERR;
    }

    return 0;
}

//----------------------------------------------------------------------------
int
BerkeleyDBTable::get(const SerializableObject&   key,
                     SerializableObject**        data,
                     TypeCollection::Allocator_t allocator)
{
    ASSERTF(multitype_, "multi-type get called for single-type table");
    
    ScratchBuffer<u_char*, 256> key_buf;
    size_t key_buf_len = flatten(key, &key_buf);
    if (key_buf_len == 0) 
    {
        log_err("zero or too long key length");
        return DS_ERR;
    }

    DBTRef k(key_buf.buf(), key_buf_len);
    DBTRef d;

    int err = db_->get(db_, NO_TX, k.dbt(), d.dbt(), 0);
     
    if (err == DB_NOTFOUND) 
    {
        return DS_NOTFOUND;
    }
    else if (err != 0)
    {
        log_err("DB: %s", db_strerror(err));
        return DS_ERR;
    }

    u_char* bp = (u_char*)d->data;
    size_t  sz = d->size;

    TypeCollection::TypeCode_t typecode;
    size_t typecode_sz = MarshalSize::get_size(&typecode);

    Builder b;
    UIntShim type_shim(b);
    Unmarshal type_unmarshaller(Serialize::CONTEXT_LOCAL, bp, typecode_sz);

    if (type_unmarshaller.action(&type_shim) != 0) {
        log_err("DB: error unserializing type code");
        return DS_ERR;
    }
    
    typecode = type_shim.value();

    bp += typecode_sz;
    sz -= typecode_sz;

    err = allocator(typecode, data);
    if (err != 0) {
        *data = NULL;
        return DS_ERR;
    }

    ASSERT(*data != NULL);

    Unmarshal unmarshaller(Serialize::CONTEXT_LOCAL, bp, sz);
    
    if (unmarshaller.action(*data) != 0) {
        log_err("DB: error unserializing data object");
        delete *data;
       *data = NULL;
        return DS_ERR;
    }
    
    return DS_OK;
}

//----------------------------------------------------------------------------
int 
BerkeleyDBTable::put(const SerializableObject&  key,
                     TypeCollection::TypeCode_t typecode,
                     const SerializableObject*  data,
                     int                        flags)
{
    ScratchBuffer<u_char*, 256> key_buf;
    size_t key_buf_len = flatten(key, &key_buf);
    int err;

    // flatten and fill in the key
    DBTRef k(key_buf.buf(), key_buf_len);

    // if the caller does not want to create new entries, first do a
    // db get to see if the key already exists
    if ((flags & DS_CREATE) == 0) {
        DBTRef d;
        err = db_->get(db_, NO_TX, k.dbt(), d.dbt(), 0);
        if (err == DB_NOTFOUND) {
            return DS_NOTFOUND;
        } else if (err != 0) {
            log_err("put -- DB internal error: %s", db_strerror(err));
            return DS_ERR;
        }
    }

    // figure out the size of the data
    MarshalSize sizer(Serialize::CONTEXT_LOCAL);
    if (sizer.action(data) != 0) {
        log_err("error sizing data object");
        return DS_ERR;
    }
    size_t object_sz = sizer.size();

    // and the size of the type code (if multitype)
    size_t typecode_sz = 0;
    if (multitype_) {
        typecode_sz = MarshalSize::get_size(&typecode);
    }

    // XXX/demmer -- one little optimization would be to pass the
    // calculated size out to the caller (the generic DurableTable),
    // so we don't have to re-calculate it in the object cache code
    
    log_debug("put: serializing %zu byte object (plus %zu byte typecode)",
              object_sz, typecode_sz);

    ScratchBuffer<u_char*, 1024> scratch;
    u_char* buf = scratch.buf(typecode_sz + object_sz);
    DBTRef d(buf, typecode_sz + object_sz);
    
    // if we're a multitype table, marshal the type code
    if (multitype_) 
    {
        Marshal typemarshal(Serialize::CONTEXT_LOCAL, buf, typecode_sz);
        UIntShim type_shim(typecode);
            
        if (typemarshal.action(&type_shim) != 0) {
            log_err("error serializing type code");
            return DS_ERR;
        }
    }
        
    Marshal m(Serialize::CONTEXT_LOCAL, buf + typecode_sz, object_sz);
    if (m.action(data) != 0) {
        log_err("error serializing data object");
        return DS_ERR;
    }
    
    int db_flags = 0;
    if (flags & DS_EXCL) {
        db_flags |= DB_NOOVERWRITE;
    }
        
    err = db_->put(db_, NO_TX, k.dbt(), d.dbt(), db_flags);

    if (err == DB_KEYEXIST) {
        return DS_EXISTS;
    } else if (err != 0) {
        log_err("DB internal error: %s", db_strerror(err));
        return DS_ERR;
    }

    return 0;
}

//----------------------------------------------------------------------------
int 
BerkeleyDBTable::del(const SerializableObject& key)
{
    u_char key_buf[256];
    size_t key_buf_len;

    key_buf_len = flatten(key, key_buf, 256);
    if (key_buf_len == 0) 
    {
        log_err("zero or too long key length");
        return DS_ERR;
    }

    DBTRef k(key_buf, key_buf_len);
    
    int err = db_->del(db_, NO_TX, k.dbt(), 0);
    
    if (err == DB_NOTFOUND) 
    {
        return DS_NOTFOUND;
    } 
    else if (err != 0) 
    {
        log_err("DB internal error: %s", db_strerror(err));
        return DS_ERR;
    }

    return 0;
}

//----------------------------------------------------------------------------
size_t
BerkeleyDBTable::size() const
{
    int err;
    int flags = 0;

    union {
        void* ptr;
        struct __db_bt_stat* btree_stats;
        struct __db_h_stat*  hash_stats;
    } stats;

    stats.ptr = 0;
    
#if ((DB_VERSION_MAJOR == 4) && (DB_VERSION_MINOR <= 2))
    err = db_->stat(db_, &stats.ptr, flags);
#else
    err = db_->stat(db_, NO_TX, &stats.ptr, flags);
#endif
    if (err != 0) {
        log_crit("error in DB::stat: %d", errno);
        ASSERT(stats.ptr == 0);
        return 0;
    }
    
    ASSERT(stats.ptr != 0);

    size_t ret;
    
    switch(db_type_) {
    case DB_BTREE:
        ret = stats.btree_stats->bt_nkeys;
        break;
    case DB_HASH:
        ret = stats.hash_stats->hash_nkeys;
        break;
    default:
        PANIC("illegal value for db_type %d", db_type_);
    }

    free(stats.ptr);

    return ret;
}

//----------------------------------------------------------------------------
DurableIterator*
BerkeleyDBTable::itr()
{
    return new BerkeleyDBIterator(this);
}

//----------------------------------------------------------------------------
int 
BerkeleyDBTable::key_exists(const void* key, size_t key_len)
{
    DBTRef k(const_cast<void*>(key), key_len);
    DBTRef d;

    int err = db_->get(db_, NO_TX, k.dbt(), d.dbt(), 0);
    if (err == DB_NOTFOUND) 
    {
        return DS_NOTFOUND;
    }
    else if (err != 0)
    {
        log_err("DB: %s", db_strerror(err));
        return DS_ERR;
    }

    return 0;
}

/******************************************************************************
 *
 * BerkeleyDBIterator
 *
 *****************************************************************************/
BerkeleyDBIterator::BerkeleyDBIterator(BerkeleyDBTable* t)
    : Logger("BerkeleyDBIterator", "%s/iter", t->logpath()),
      cur_(0), valid_(false)
{
    int err = t->db_->cursor(t->db_, NO_TX, &cur_, 0);
    if (err != 0) {
        log_err("DB: cannot create a DB iterator, err=%s", db_strerror(err));
        cur_ = 0;
    }

    if (cur_)
    {
        valid_ = true;
    }
}

//----------------------------------------------------------------------------
BerkeleyDBIterator::~BerkeleyDBIterator()
{
    valid_ = false;
    if (cur_) 
    {
        int err = cur_->c_close(cur_);

        if (err != 0) {
            log_err("Unable to close cursor, %s", db_strerror(err));
        }
    }
}

//----------------------------------------------------------------------------
int
BerkeleyDBIterator::next()
{
    ASSERT(valid_);

    memset(&key_, 0, sizeof(key_));
    memset(&data_, 0, sizeof(data_));

    int err = cur_->c_get(cur_, key_.dbt(), data_.dbt(), DB_NEXT);

    if (err == DB_NOTFOUND) {
        valid_ = false;
        return DS_NOTFOUND;
    } 
    else if (err != 0) {
        log_err("next() DB: %s", db_strerror(err));
        valid_ = false;
        return DS_ERR;
    }

    return 0;
}

//----------------------------------------------------------------------------
int
BerkeleyDBIterator::get_key(SerializableObject* key)
{
    ASSERT(key != NULL);
    oasys::Unmarshal un(oasys::Serialize::CONTEXT_LOCAL,
                        static_cast<u_char*>(key_->data), key_->size);

    if (un.action(key) != 0) {
        log_err("error unmarshalling");
        return DS_ERR;
    }
    
    return 0;
}

} // namespace oasys

#endif // LIBDB_ENABLED
