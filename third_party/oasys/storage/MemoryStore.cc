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

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include <debug/DebugUtils.h>
#include <util/StringBuffer.h>
#include <util/Pointers.h>
#include <serialize/MarshalSerialize.h>
#include <serialize/TypeShims.h>

#include "MemoryStore.h"
#include "StorageConfig.h"

namespace oasys {
/******************************************************************************
 *
 * MemoryStore
 *
 *****************************************************************************/

MemoryStore::MemoryStore(const char* logpath) 
    : DurableStoreImpl("MemoryStore", logpath),
      init_(false)
{}

//----------------------------------------------------------------------------
MemoryStore::~MemoryStore()
{
    log_info("db closed");
}

//----------------------------------------------------------------------------
int 
MemoryStore::init(const StorageConfig& cfg)
{
    if (cfg.tidy_) {
        tables_.clear();
    }
 
    init_ = true;

    return 0;
}

//----------------------------------------------------------------------------
int
MemoryStore::get_table(DurableTableImpl**  table,
                       const std::string&  name,
                       int                 flags,
                       PrototypeVector&    prototypes)
{
    (void)prototypes;
    
    // XXX/bowei -- || access?
    TableMap::iterator iter = tables_.find(name);

    MemoryTable::ItemMap* items;
    
    if (iter == tables_.end()) {
        if (! (flags & DS_CREATE)) {
            return DS_NOTFOUND;
        }
        
        tables_[name] = MemoryTable::ItemMap();
        items = &tables_[name];
    } else {
        if (flags & DS_EXCL) {
            return DS_EXISTS;
        }

        items = &iter->second;
    }

    *table = new MemoryTable(logpath_, items, name, (flags & DS_MULTITYPE) != 0);

    return DS_OK;
}

//----------------------------------------------------------------------------
int
MemoryStore::del_table(const std::string& name)
{
    // XXX/bowei -- busy tables?
    log_info("deleting table %s", name.c_str());
    tables_.erase(name);
    return 0;
}

//----------------------------------------------------------------------------
int 
MemoryStore::get_table_names(StringVector* names)
{
    names->clear();
    for (TableMap::const_iterator itr = tables_.begin();
         itr != tables_.end(); ++itr)
    {
        names->push_back(itr->first);
    }

    return 0;
}

//----------------------------------------------------------------------------
std::string 
MemoryStore::get_info() const
{
    StringBuffer desc;

    return "Memory";
}

/******************************************************************************
 *
 * MemoryTable
 *
 *****************************************************************************/
MemoryTable::MemoryTable(const char* logpath, ItemMap* items,
                         const std::string& name, bool multitype)
    : DurableTableImpl(name, multitype),
      Logger("MemoryTable", "%s/%s", logpath, name.c_str()),
      items_(items)
{
}

//----------------------------------------------------------------------------
MemoryTable::~MemoryTable() 
{
}

//----------------------------------------------------------------------------
int 
MemoryTable::get(const SerializableObject& key, 
                 SerializableObject*       data)
{
    ASSERTF(!multitype_, "single-type get called for multi-type table");
    
    StringSerialize serialize(Serialize::CONTEXT_LOCAL,
                              StringSerialize::DOT_SEPARATED);
    if (serialize.action(&key) != 0) {
        PANIC("error sizing key");
    }
    std::string table_key;
    table_key.assign(serialize.buf().data(), serialize.buf().length());

    ItemMap::iterator iter = items_->find(table_key);
    if (iter == items_->end()) {
        return DS_NOTFOUND;
    }

    Item* item = iter->second;
    Unmarshal unm(Serialize::CONTEXT_LOCAL,
                  item->data_.buf(), item->data_.len());

    if (unm.action(data) != 0) {
        log_err("error unserializing data object");
        return DS_ERR;
    }

    return 0;
}

//----------------------------------------------------------------------------
int
MemoryTable::get(const SerializableObject&   key,
                 SerializableObject**        data,
                 TypeCollection::Allocator_t allocator)
{
    ASSERTF(multitype_, "multi-type get called for single-type table");
    
    StringSerialize serialize(Serialize::CONTEXT_LOCAL,
                              StringSerialize::DOT_SEPARATED);
    if (serialize.action(&key) != 0) {
        PANIC("error sizing key");
    }
    std::string table_key;
    table_key.assign(serialize.buf().data(), serialize.buf().length());

    ItemMap::iterator iter = items_->find(table_key);
    if (iter == items_->end()) {
        return DS_NOTFOUND;
    }

    Item* item = iter->second;
    
    int err = allocator(item->typecode_, data);
    if (err != 0) {
        return DS_ERR;
    }

    Unmarshal unm(Serialize::CONTEXT_LOCAL,
                  item->data_.buf(), item->data_.len());

    if (unm.action(*data) != 0) {
        log_err("error unserializing data object");
        return DS_ERR;
    }

    return DS_OK;
}

//----------------------------------------------------------------------------
int 
MemoryTable::put(const SerializableObject& key,
                 TypeCollection::TypeCode_t typecode,
                 const SerializableObject* data,
                 int                       flags)
{
    StringSerialize serialize(Serialize::CONTEXT_LOCAL,
                              StringSerialize::DOT_SEPARATED);
    if (serialize.action(&key) != 0) {
        PANIC("error sizing key");
    }
    std::string table_key;
    table_key.assign(serialize.buf().data(), serialize.buf().length());

    ItemMap::iterator iter = items_->find(table_key);

    Item* item;
    if (iter == items_->end()) {
        if (! (flags & DS_CREATE)) {
            return DS_NOTFOUND;
        }

        item = new Item();
        (*items_)[table_key] = item;

    } else {
        if (flags & DS_EXCL) {
            return DS_EXISTS;
        }

        item = iter->second;
    }

    item->typecode_ = typecode;

    { // first the key
        log_debug("put: serializing key");
    
        Marshal m(Serialize::CONTEXT_LOCAL, &item->key_);
        if (m.action(&key) != 0) {
            log_err("error serializing key object");
            return DS_ERR;
        }
    }

    { // then the data
        log_debug("put: serializing object");
    
        Marshal m(Serialize::CONTEXT_LOCAL, &item->data_);
        if (m.action(data) != 0) {
            log_err("error serializing data object");
            return DS_ERR;
        }
    }

    item->typecode_ = typecode;

    return DS_OK;
}

//----------------------------------------------------------------------------
int 
MemoryTable::del(const SerializableObject& key)
{ 
    StringSerialize serialize(Serialize::CONTEXT_LOCAL,
                              StringSerialize::DOT_SEPARATED);
    if (serialize.action(&key) != 0) {
        PANIC("error sizing key");
    }
    std::string table_key;
    table_key.assign(serialize.buf().data(), serialize.buf().length());

    ItemMap::iterator iter = items_->find(table_key);
    if (iter == items_->end()) {
        return DS_NOTFOUND;
    }

    Item* item = iter->second;
    items_->erase(iter);
    delete item;
    
    return DS_OK;
}

//----------------------------------------------------------------------------
size_t
MemoryTable::size() const
{
    return items_->size();
}

//----------------------------------------------------------------------------
DurableIterator*
MemoryTable::itr()
{
    return new MemoryIterator(logpath_, this);
}


/******************************************************************************
 *
 * MemoryIterator
 *
 *****************************************************************************/
MemoryIterator::MemoryIterator(const char* logpath, MemoryTable* t)
    : Logger("MemoryIterator", "%s/iter", logpath)
{
    table_ = t;
    first_ = true;
}

//----------------------------------------------------------------------------
MemoryIterator::~MemoryIterator()
{
}

//----------------------------------------------------------------------------
int
MemoryIterator::next()
{
    if (first_) {
        first_ = false;
        iter_ = table_->items_->begin();
    } else {
        ++iter_;
    }

    if (iter_ == table_->items_->end()) {
        return DS_NOTFOUND;
    }

    return 0;
}

//----------------------------------------------------------------------------
int
MemoryIterator::get_key(SerializableObject* key)
{
    ASSERT(key != NULL);

    MemoryTable::Item* item = iter_->second;
    
    oasys::Unmarshal un(oasys::Serialize::CONTEXT_LOCAL,
                        item->key_.buf(), item->key_.len());
    
    if (un.action(key) != 0) {
        log_err("error unmarshalling");
        return DS_ERR;
    }
    
    return 0;
}

} // namespace oasys

