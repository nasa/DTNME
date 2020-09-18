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

#ifndef _OASYS_TCL_LIST_SERIALIZE_H_
#define _OASYS_TCL_LIST_SERIALIZE_H_

#include <tcl.h>
#include "Serialize.h"

namespace oasys {

/**
 * TclListSerialize is a SerializeAction that marshals an object into
 * a tcl list of alternating name/object pairs.
 *
 * For contained serializable objects, a new list is created for the
 * sub object.
 */
class TclListSerialize : public SerializeAction {
public:
    /**
     * Options for the list serialization.
     */
    enum {
        KEEP_SINGLETON_SUBLISTS = 1,	///< don't remove one-element sub lists
    };
        
    /**
     * Constructor
     */
    TclListSerialize(Tcl_Interp* interp, Tcl_Obj* list_obj,
                     context_t context, int options);

    /**
     * Destructor.
     */
    ~TclListSerialize();
 
    /**
     * We can tolerate a const object.
     */
    using SerializeAction::action;
    int action(const SerializableObject* const_object)
    {
        SerializableObject* object = (SerializableObject*)const_object;
        return SerializeAction::action(object);
    }
    
    /// @{
    /// Virtual functions inherited from SerializeAction
    void process(const char* name, u_int64_t* i);
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, u_int32_t len);
    void process(const char*          name, 
                 BufferCarrier<u_char>* carrier);
    void process(const char*          name,
                 BufferCarrier<u_char>* carrier,
                 u_char                 terminator);
    void process(const char* name, std::string* s);
    void process(const char* name, SerializableObject* object);
    /// @}

private:
    Tcl_Interp* interp_;
    Tcl_Obj*    list_obj_;
};

} // namespace oasys

#endif /* _OASYS_TCL_LIST_SERIALIZE_H_ */
