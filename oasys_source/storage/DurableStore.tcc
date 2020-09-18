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

// included by DurableStore.h

inline const char* durable_strerror(int r)
{
    DurableStoreResult_t result = (DurableStoreResult_t)r;
    
    switch(result) {
    case DS_OK:       return "success";
    case DS_NOTFOUND: return "element not found";
    case DS_BUFSIZE:  return "buffer too small.";
    case DS_BUSY:     return "table still open, can't delete";
    case DS_EXISTS:   return "key already exists";
    case DS_BADTYPE:  return "type collection error";
    case DS_ERR:      return "unknown error";
    }
    NOTREACHED;
}

template <typename _DataType>
inline int
DurableStore::get_table(SingleTypeDurableTable<_DataType>** table,
                        std::string         table_name,
                        int                 flags,
                        DurableObjectCache<_DataType>* cache)
{
    int err;

    // build the (single-element) vector of prototypes
    PrototypeVector prototypes;
    prototypes.push_back(new _DataType(Builder()));

    DurableTableImpl* table_impl;
    err = impl_->get_table(&table_impl, table_name, flags, prototypes);

    // clean up the prototype vector
    delete prototypes.back();
    prototypes.pop_back();
    ASSERT(prototypes.size() == 0);

    // check for errors from the implementation
    if (err != 0) {
        return err;
    }
    
    *table = new SingleTypeDurableTable<_DataType>
             (table_impl, table_name, cache);

    return 0;
}

template <typename _BaseType, typename _Collection>
inline int
DurableStore::get_table(MultiTypeDurableTable<_BaseType, _Collection>** table,
                        std::string         table_name,
                        int                 flags,
                        DurableObjectCache<_BaseType>* cache)
{
    int err;
    PrototypeVector prototypes;

    // XXX/demmer this needs to fold into the TypeCollection proper
    // somehow since the typecode range need not be tightly packed (as
    // this code assumes). instead, TypeCollection::reg should build
    // up the vector of prototypes once and have it as a member
    // variable. since we don't need this for berkeley db (only needed
    // for sql), just punt on figure this out
    
/*
    TypeCollection<_Collection> *collection =
        TypeCollection<_Collection>::instance();
    

    typename TypeCollection<_Collection>::TypeCode_t code, low, high;
    _BaseType* obj;

    // Find the range of type codes
    low  = TypeCollectionCode<_Collection, _BaseType>::TYPECODE_LOW;
    high = TypeCollectionCode<_Collection, _BaseType>::TYPECODE_HIGH;

    // Now for each defined element in the range, create an instance
    // and stuff it in the vector of protypes. 

    for (code = low; code <= high; ++code)
    {
        err = collection->new_object(code, &obj);

        if (err == 0)
        {
            prototypes.push_back(obj);
        }
        else if (err == TypeCollectionErr::TYPECODE)
        {
            continue;
        }
        else
        {
            log_crit("/typecollection",
                     "unknown error from TypeCollection::new_object");
            return DS_ERR;
        }
    }
*/

    // set the multitype flag for the implementation
    flags |= DS_MULTITYPE;

    DurableTableImpl* table_impl;
    err = impl_->get_table(&table_impl, table_name, flags, prototypes);
    if (err != 0) {
        return err;
    }

    *table = new MultiTypeDurableTable<_BaseType, _Collection>(table_impl,
                                                               table_name,
                                                               cache);
    return 0;
}

/**
 * Delete the table (by id) from the datastore.
 */
inline int
DurableStore::del_table(std::string db_name)
{
    return impl_->del_table(db_name);
}
