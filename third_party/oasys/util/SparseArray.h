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

#ifndef __SPARSE_RANGE_H__
#define __SPARSE_RANGE_H__

#include <list>

/*!  
 * SparseArray defines a logical array of _Type which is expected to
 * be sparse.  SparseArray can be very smart, but right now is dumb
 * but gets things right.
 * 
 */
template<typename _Type> class SparseArray { 
public: 
    class Block {
    public:
        size_t offset_;
        size_t size_;
        _Type* data_;

        Block(size_t offset = 0, size_t size = 0)
            : offset_(offset), size_(size), data_(0)
        {
            data_ = static_cast<_Type*>(calloc(size, sizeof(_Type)));
        }
        
        struct BlockCompare {
            bool operator()(const Block& a, const Block& b) 
            {
                return a.offset_ < b.offset_;
            }
        };
    };
    typedef std::list<Block> BlockList;

    ~SparseArray() 
    {
        for (typename BlockList::iterator itr = blocks_.begin();
             itr != blocks_.end(); ++itr)
        {
            free(itr->data_);
            itr->data_ = 0;
        }
    }
    
    /*!  
     * In the empty sparse area, the SparseArray returns an element
     * which is the default constructor of _Type.
     */
    _Type operator[](size_t offset) const
    {
        for (typename BlockList::const_iterator itr = blocks_.begin();
             itr != blocks_.end(); ++itr)
        {
            if (itr->offset_ <= offset &&
                itr->offset_ + itr->size_ > offset)
            {
                return itr->data_[offset - itr->offset_];
            }
            
            if (itr->offset_ > offset)
            {
                break;
            }
        }        

        return _Type();
    }
    
    /*!
     * Write into the sparse map. Allocates a block if needed.
     */
    void range_write(size_t offset, size_t elts, const _Type* in)
    {
        _Type* dest = allocate(offset, elts);
        ASSERT(dest != 0);

        for (size_t i = 0; i<elts; ++i)
        {
            dest[i] = in[i];
        }
    }

    /*!
     * @return size of the SparseArray, which is from 0...last written
     * to range.
     */
    size_t size() const
    {
        if (blocks_.empty())
        {
            return 0;
        }

        typename BlockList::const_iterator i = blocks_.end();
        --i;

        return i->offset_ + i->size_;
    }

    /*!
     * @return List of the blocks.
     */
    const BlockList& blocks() const 
    { 
        return blocks_;
    }
    
private:
    //! List of blocks_ sorted in ascending order by offset
    BlockList blocks_;

    /*!
     * Allocate a block starting at offset with elts. Will merge
     * blocks that it finds overlapping.
     *
     * @return Pointer to allocated region.
     */
    _Type* allocate(size_t offset, size_t size)
    {
        bool merge = false;
        
        //
        // Take care of any existing blocks which overlap the new
        // block like this:
        //
        // |-- old block --|
        //                   |- new block -| case 1
        //   |-new block-|                   case 2
        //     |---- new block ----|         case 3
        typename BlockList::iterator itr = blocks_.begin();
        while (itr != blocks_.end())
        {
            // case 1: block occurs before, don't merge if bordering
            if (itr->offset_ + itr->size_ <= offset)
            {
                ++itr;
                continue;
            }              

            // case 2: new block is completely contained in an existing block
            if (itr->offset_ <= offset && 
                (itr->offset_ + itr->size_ >= offset + size))
            {
                return &itr->data_[offset - itr->offset_];
            }

            // case 3: current block overlaps part of the new block
            if (itr->offset_ <= offset && 
                (itr->offset_ + itr->size_ > offset) &&
                (itr->offset_ + itr->size_ < offset + size))
            {
                merge = true;
                break;
            }

            // we stepped over to past the block
            if (itr->offset_ > offset)
            {
                break;
            }

            NOTREACHED;
        }

        Block new_block;
        if (merge) 
        {
            ASSERT(itr != blocks_.end());

            size_t new_size = offset + size - itr->offset_;
            itr->data_ = static_cast<_Type*>
                         (realloc(itr->data_, sizeof(_Type) * new_size));
            itr->size_ = new_size;
            new_block = *itr;
        }
        else
        {
            new_block = Block(offset, size);
            blocks_.insert(itr, new_block);
            --itr;
        }
        
        typename BlockList::iterator tail = itr;
        ++tail;
        
        //
        // fixup the tail, which includes any blocks like this:
        //
        // |---- new block ----|
        //    |- old block -|             case 1
        //         |---- old block ----|  case 2
        //
        while (tail != blocks_.end())
        {
            // stepped past the end of the new_block
            if (tail->offset_ > itr->offset_ + itr->size_) 
            {
                break;
            }
            
            // case 1
            if (tail->offset_ <= itr->offset_ + itr->size_ &&
                tail->offset_ + tail->size_ <= itr->offset_ + itr->size_)
            {
                // copy over the old data
                size_t offset = tail->offset_ - itr->offset_;
                for (size_t i = 0; i<tail->size_; ++i)
                {
                    itr->data_[offset + i] = tail->data_[i];
                }
                free(tail->data_);
                blocks_.erase(tail++);
                continue;
            }
            
            // case 2
            if (tail->offset_ <= itr->offset_ + itr->size_)
            {
                size_t new_size = tail->offset_ + tail->size_ - itr->offset_;
                itr->data_ = static_cast<_Type*>(realloc(itr->data_, new_size));
                itr->size_ = new_size;
                
                // copy over the old data
                size_t offset = tail->offset_ - itr->offset_;
                for (size_t i = 0; i<tail->size_; ++i)
                {
                    itr->data_[offset + i] = tail->data_[i];
                }
                free(tail->data_);
                blocks_.erase(tail++);
                continue;
            }

            NOTREACHED;
        }

        return &itr->data_[offset - itr->offset_];
    }
    
};

#endif // __SPARSE_RANGE_H__
