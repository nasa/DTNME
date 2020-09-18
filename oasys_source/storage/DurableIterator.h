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



#ifndef __OASYS_DURABLE_STORE_INTERNAL_HEADER__
#error DurableIterator.h must only be included from within DurableStore.h
#endif


/**
 * Abstract base class for a table iterator. Note: It is important
 * that iterators do NOT outlive the tables they point into.
 */
class DurableIterator {
public:
    // virtual destructor
    virtual ~DurableIterator() {}

    /**
     * Advance the pointer. An initialized iterator will be pointing
     * right before the first element in the list, so iteration code
     * will always be:
     *
     * @code
     * MyKeyType key;
     * DurableIterator* i = table->itr();
     * while (i->next() == 0) 
     * {
     *    i->get(&key);
     *    // ... do stuff
     * }
     * 
     * // Remember to delete the Iterator! Bad things happen if the
     * table disappears when the iterator is still open.
     * delete_z(i);
     * @endcode
     *
     * @return DS_OK, DS_NOTFOUND if no more elements, DS_ERR if an
     * error occurred while iterating.
     */
    virtual int next() = 0;

    /**
     * Unserialize the current element into the given key object.
     */
    virtual int get_key(SerializableObject* key) = 0;
};

//----------------------------------------------------------------------------
/*!
 * Template class for a "filtered" iterator which only iterates over
 * desired patterns.
 *
 * _filter: struct { accept: DurableIterator -> bool }
 */
template<typename _filter>
class DurableFilterIterator : public DurableIterator {
public:
    DurableFilterIterator(DurableIterator* itr)
        : itr_(itr) {}

    // virtual from DurableIterator
    int next() 
    {
        for (;;) 
        {
            int ret = itr_->next();
            if (ret != DS_OK) {
                return ret;
            }
            
            if (_filter::accept(itr_)) {
                return DS_OK;
            }
        }
    }

    int get_key(SerializableObject* key) 
    {
        return itr_->get_key(key);
    }
    
private:
    DurableIterator* itr_;
};
