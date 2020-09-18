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

#ifndef __BUFFEREDSERIALIZEACTION_H__
#define __BUFFEREDSERIALIZEACTION_H__

#include "Serialize.h"
#include "../debug/Log.h"

namespace oasys {

struct ExpandableBuffer;

//////////////////////////////////////////////////////////////////////////////
/**
 * Common base class for Marshal and Unmarshal that manages the flat
 * buffer.
 */
class BufferedSerializeAction : public SerializeAction, public Logger {
public:
    /**
     * Constructor with a fixed-length buffer.
     */
    BufferedSerializeAction(action_t action, context_t context,
                            u_char* buf, size_t length, 
                            int options = 0);

    /**
     * Constructor with an expandable buffer.
     */
    BufferedSerializeAction(action_t action, context_t context,
                            ExpandableBuffer* buf,
                            int options = 0);

    /** 
     * Since BufferedSerializeAction ignores the name field, calling
     * process() on a contained object is the same as just calling the
     * contained object's serialize() method.
     */
    virtual void process(const char* name, SerializableObject* object)
    {
        (void)name;
        object->serialize(this);
    }
    
protected:
    /**  
     * Get the next R/W length of the buffer.
     *
     * @return R/W buffer of size length or NULL on error
     */
    u_char* next_slice(size_t length);
    
    /** @return buffer */
    u_char* buf();

    /** @return buffer length */
    size_t length();
    
    /** @return offset into the buffer */
    size_t offset();
    
 private:
    /// Expandable buffer
    ExpandableBuffer* expandable_buf_;

    // Fields used for fixed length buffer
    
    u_char* buf_;	///< Buffer that is un/marshalled
    size_t  length_;	///< Length of the buffer.
    size_t  offset_;	///< Offset into the buffer
};

} // namespace oasys

#endif /* __BUFFEREDSERIALIZEACTION_H__ */
