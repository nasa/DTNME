/*
 *    Copyright 2006 Intel Corporation
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


#ifndef _INTERNAL_KEY_DURABLE_TABLE_H_
#error This file should only be included by InternalKeyDurableTable.h
#endif

// for slightly improved readability (and to help function definitions
// fit on one line), the following macro is used as the class name for
// the functions below

#define _InternalKeyDurableTableClass \
   InternalKeyDurableTable<_ShimType, _KeyType, _DataType>


//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
inline
_InternalKeyDurableTableClass::InternalKeyDurableTable(const char* classname,
                                                       const char* logpath,
                                                       const char* datatype,
                                                       const char* table_name)
    : Logger(classname, "%s", logpath),
      datatype_(datatype), table_name_(table_name)
{
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
inline
_InternalKeyDurableTableClass::~InternalKeyDurableTable()
{
    close();
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
inline int
_InternalKeyDurableTableClass::do_init(const StorageConfig& cfg,
                                       DurableStore*        store)
{
    int flags = 0;
    int shim_length = _ShimType::shim_length();
    ASSERT((shim_length & ~KEY_LEN_MASK) == 0);
    flags = (shim_length & KEY_LEN_MASK) << KEY_LEN_SHIFT;
    log_debug("shim_length for table %s is %d (shifted %x)", table_name_, shim_length, flags);

    if (cfg.init_)
        flags |= DS_CREATE;

    int err = store->get_table(&table_, table_name_, flags);

    if (err != 0) {
        log_err("error initializing durable store");
        return err;
    }
    
    return 0;
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
inline int
_InternalKeyDurableTableClass::do_init_aux(const StorageConfig& cfg,
                                           DurableStore*        store)
{
    int flags = 0;
    int shim_length = _ShimType::shim_length();
    ASSERT((shim_length & ~KEY_LEN_MASK) == 0);
    flags = (shim_length & KEY_LEN_MASK) << KEY_LEN_SHIFT;
    log_debug("shim_length for table %s is %d (shifted %x)", table_name_, shim_length, flags);
    
    flags |= DS_AUX_TABLE;

    if (cfg.init_)
        flags |= DS_CREATE;

    int err = store->get_table(&table_, table_name_, flags);

    if (err != 0) {
        log_err("error initializing durable store");
        return err;
    }
    
    return 0;
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
inline bool
_InternalKeyDurableTableClass::add(_DataType* data)
{
    _ShimType shim(data->durable_key());
    int err = table_->put(shim, data, DS_CREATE | DS_EXCL);

    if (err == DS_EXISTS) {
        log_err("add %s *%p: already exists", datatype_, &shim);
        return false;
    }

    if (err != 0) {
        PANIC("%s::add(*%p): fatal database error", classname_, &shim);
    }
    
    log_debug("add(*%p): success", &shim);
    return true;
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
inline _DataType*
_InternalKeyDurableTableClass::get(_KeyType key)
{
    _DataType* data = NULL;
    _ShimType shim(key);
    int err = table_->get(shim, &data);

    if (err == DS_NOTFOUND) {
        log_warn("get(*%p): %s doesn't exist", &shim, datatype_);
        return NULL;
    }
    
    if (err != 0) {
        PANIC("%s::get(*%p): fatal database error", classname_, &shim);
    }
    
    ASSERT(data != NULL);
    ASSERT(data->durable_key() == key);
    
    log_debug("get(*%p): success", &shim);
    return data;
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
bool
_InternalKeyDurableTableClass::update(_DataType* data)
{
    _ShimType shim(data->durable_key());
    int err = table_->put(shim, data, 0);

    if (err == DS_NOTFOUND) {
        log_err("update(*%p): %s doesn't exist in table",
                &shim, datatype_);
        return false;
    }
    
    if (err != 0) {
        PANIC("%s::update(*%p): fatal database error", classname_, &shim);
    }
    
    log_debug("update(*%p): success", &shim);
    return true;
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
bool
_InternalKeyDurableTableClass::del(_KeyType key)
{
    _ShimType shim(key);

    int err = table_->del(shim);
    
    if (err == DS_NOTFOUND) {
        log_err("del(*%p): %s doesn't exist in table",
                &shim, datatype_);
        return false;
    }
    
    if (err != 0) {
        PANIC("%s::del(*%p): fatal database error", classname_, &shim);
    }
    
    log_debug("del(*%p): success", &shim);
    return true;
}

//---------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
inline void
_InternalKeyDurableTableClass::close()
{
    if (table_) {
        log_debug("closing %s store", datatype_);
        
        delete table_;
        table_ = NULL;
    }
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
_InternalKeyDurableTableClass::iterator::iterator(table_t* table,
                                                  DurableIterator* iter)
    : table_(table), iter_(iter), cur_val_(), done_(0)
{
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
_InternalKeyDurableTableClass::iterator::~iterator()
{
    delete iter_;
    iter_ = NULL;
}

//----------------------------------------------------------------------
template <typename _ShimType, typename _KeyType, typename _DataType>
int
_InternalKeyDurableTableClass::iterator::next()
{
    int err = iter_->next();
    if (err == DS_NOTFOUND)
    {
        done_ = true;
        return err; // all done
    }
    
    else if (err != DS_OK)
    {
        log_err_p(table_->logpath(), "error in iterator next");
        return err;
    }

    err = iter_->get_key(&cur_val_);
    if (err != 0)
    {
        log_err_p(table_->logpath(), "error in iterator get_key");
        return DS_ERR;
    }

    return DS_OK;
}
