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

// Included by DurableTable.h

//----------------------------------------------------------------------------
template <typename _Type>
inline int
DurableTable<_Type>::del(const SerializableObject& key)
{
    if (this->cache_ != 0) {
        int err = this->cache_->del(key);
        if (err == DS_ERR)
            return err; // otherwise fall through
    }
    
    return this->impl_->del(key);
}

//----------------------------------------------------------------------------
template <typename _Type>
inline size_t
DurableTable<_Type>::size()
{
    return this->impl_->size();
}

//----------------------------------------------------------------------------
template <typename _Type>
int
DurableTable<_Type>::cleanup_put_flags(int flags)
{
    if (flags & DS_EXCL) {
        if (! (flags & DS_CREATE)) {
            // XXX/demmer find a better log target
            log_warn_p("/durabletable", "DS_EXCL without DS_CREATE");
        }
        flags |= DS_CREATE;
    }

    return flags;
}

//----------------------------------------------------------------------------
template <typename _DataType>
inline int
SingleTypeDurableTable<_DataType>::get(const SerializableObject& key,
                                       _DataType**               data,
                                       bool*                     from_cache)
{
    int err;

    if (this->cache_ != 0) {
        err = this->cache_->get(key, data);
        if (err == DS_OK) {
            ASSERT(*data != 0);

            if (from_cache != 0) {
                *from_cache = true;
            }

            return DS_OK;
        }
    }
    
    _DataType* d = new _DataType(Builder());

    err = this->impl_->get(key, d);
    if (err != 0) {
        delete d;
        return err;
    }

    if (this->cache_ != 0) {
        err = this->cache_->put(key, d, DS_CREATE | DS_EXCL);
        ASSERT(err == DS_OK);
    }

    *data   = d;
    if (from_cache != 0) {
        *from_cache = false;
    }

    return 0;
}

//----------------------------------------------------------------------------
template <typename _DataType>
inline int
SingleTypeDurableTable<_DataType>::get_copy(const SerializableObject& key,
                                            _DataType* data)
{
    ASSERT(data != 0);

    if (this->cache_ != 0) 
    {
        _DataType* cache_data;
        int err = this->cache_->get(key, &cache_data);

        if (err == DS_OK) {
            ASSERT(cache_data != 0);
            
            *data = *cache_data;
            return DS_OK;
        }
    } 

    return this->impl_->get(key, data);
}

//----------------------------------------------------------------------------
template <typename _DataType>
inline int
SingleTypeDurableTable<_DataType>::put(const SerializableObject& key,
                                       const _DataType*          data,
                                       int                       flags)
{
    flags = DurableTable<_DataType>::cleanup_put_flags(flags);

    int ret = this->impl_->put(key, TypeCollection::UNKNOWN_TYPE, data, flags);

    if (ret != DS_OK) {
        return ret;
    }
    
    if (this->cache_ != 0) {
        ret = this->cache_->put(key, data, flags);
        ASSERT(ret == DS_OK);
    }
    
    return ret;
}

//----------------------------------------------------------------------------
template <typename _BaseType, typename _Collection>
inline int
MultiTypeDurableTable<_BaseType, _Collection>::get(
    const SerializableObject& key,
    _BaseType**               data,
    bool*                     from_cache
    )
{
    int err;

    if (this->cache_ != 0) {
        err = this->cache_->get(key, data);
        if (err == DS_OK) {
            ASSERT(*data != NULL);

            if (from_cache != 0) {
                *from_cache = true;
            }
            
            return DS_OK;
        }
    }

    SerializableObject* generic_data = NULL;
    err = this->impl_->get(key, &generic_data, &new_object);
    if (err != DS_OK) {
        *data = NULL;
        return err;
    }

    *data = dynamic_cast<_BaseType*>(generic_data);
    ASSERT(*data != NULL);
    
    if (from_cache != 0) {
        *from_cache = false;
    }

    if (this->cache_ != 0) {
        err = this->cache_->put(key, *data, DS_CREATE | DS_EXCL);
        ASSERT(err == DS_OK);
    }
    
    return DS_OK;
}

//----------------------------------------------------------------------------
template <typename _BaseType, typename _Collection>
inline int
MultiTypeDurableTable<_BaseType, _Collection>::new_object(
    TypeCollection::TypeCode_t typecode,
    SerializableObject** generic_object)
{
    _BaseType* object = NULL;
    int err = TypeCollectionInstance<_Collection>::instance()->
              new_object(typecode, &object);
    if (err != 0) {
        return err;
    }

    *generic_object = object; // downcast
    return 0;
}


//----------------------------------------------------------------------------
template <typename _BaseType, typename _Collection>
inline int
MultiTypeDurableTable<_BaseType, _Collection>::put(
    const SerializableObject& key,
    TypeCollection::TypeCode_t type,
    const _BaseType* data,
    int flags
    )
{
    flags = DurableTable<_BaseType>::cleanup_put_flags(flags);
    
    int ret = this->impl_->put(key, type, data, flags);

    if (ret != DS_OK) {
        return ret;
    }
    
    if (this->cache_ != 0) {
        ret = this->cache_->put(key, data, flags);
        ASSERT(ret == DS_OK);
    }

    return ret;
}

//----------------------------------------------------------------------------
template<typename _Type>
inline int 
StaticTypedDurableTable::put(
    const SerializableObject& key,
    const _Type*              data,
    int                       flags
    )
{
    flags = cleanup_put_flags(flags);

    ASSERT(this->cache_ == 0); // XXX/bowei - don't support caches for now
    int ret = this->impl_->put(key, TypeCollection::UNKNOWN_TYPE, 
                               data, flags);

    return ret;
}

//----------------------------------------------------------------------------
template<typename _Type>
inline int 
StaticTypedDurableTable::get(
    const SerializableObject& key,
    _Type**                   data
    )
{
    ASSERT(this->cache_ == 0); // XXX/bowei - don't support caches for now

    _Type* new_obj = new _Type(Builder());
    ASSERT(new_obj != 0);
    int err = this->impl_->get(key, new_obj);

    if (err != DS_OK) {
        delete_z(new_obj);
	*data = 0;

        return err;
    }
	
    *data = new_obj;

    return DS_OK;
}
