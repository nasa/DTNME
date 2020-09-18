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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "debug/DebugUtils.h"
#include "TclListSerialize.h"

namespace oasys {

TclListSerialize::TclListSerialize(Tcl_Interp* interp,
                                   Tcl_Obj*    list_obj,
                                   context_t   context,
                                   int         options)
    : SerializeAction(Serialize::MARSHAL, context, options),
      interp_(interp), list_obj_(list_obj)
{
}

TclListSerialize::~TclListSerialize()
{
}

#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 4
void
TclListSerialize::process(const char* name, u_int64_t* i)
{
    // support for 64 bit ints is only available in tcl 8.4 and
    // beyond, so we just use a normal int and possibly lose precision
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewIntObj(*i));
}

#else

void
TclListSerialize::process(const char* name, u_int64_t* i)
{
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewWideIntObj(*i));
}

#endif

void
TclListSerialize::process(const char* name, u_int32_t* i)
{
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewIntObj(*i));
}

void
TclListSerialize::process(const char* name, u_int16_t* i)
{
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewIntObj(*i));
}

void
TclListSerialize::process(const char* name, u_int8_t* i)
{
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewIntObj(*i));
}

void
TclListSerialize::process(const char* name, bool* b)
{
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewBooleanObj(*b));
}

void
TclListSerialize::process(const char* name, u_char* bp, u_int32_t len)
{
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewByteArrayObj(bp, len));
}


void 
TclListSerialize::process(const char*            name, 
                          BufferCarrier<u_char>* carrier)
{
    Tcl_ListObjAppendElement(interp_, 
                             list_obj_, 
                             Tcl_NewStringObj(name, -1));

    Tcl_ListObjAppendElement(interp_, 
                             list_obj_,
                             Tcl_NewStringObj(reinterpret_cast<char*>(carrier->buf()),
                                              carrier->len()));
}

void 
TclListSerialize::process(const char*            name,
                          BufferCarrier<u_char>* carrier,
                          u_char                 terminator)
{
    size_t size = 0;
    while (carrier->buf()[size] != terminator)
    {
        ++size;
    }
    carrier->set_len(size);
    process(name, carrier);
}

void
TclListSerialize::process(const char* name, std::string* s)
{
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp_, list_obj_,
                             Tcl_NewStringObj(s->data(), s->length()));
}

void
TclListSerialize::process(const char* name, SerializableObject* object)
{
    Tcl_Obj* old_list_obj = list_obj_;
    Tcl_Obj* new_list_obj = Tcl_NewListObj(0, NULL);

    list_obj_ = new_list_obj;
    object->serialize(this);
    list_obj_ = old_list_obj;
    
    Tcl_ListObjAppendElement(interp_, list_obj_, Tcl_NewStringObj(name, -1));

    int length = 0;
    int ok = Tcl_ListObjLength(interp_, new_list_obj, &length);
    ASSERT(ok == TCL_OK);
    
    if ((length != 2) || (options_ & KEEP_SINGLETON_SUBLISTS)) {
        Tcl_ListObjAppendElement(interp_, list_obj_, new_list_obj);
    } else {
        Tcl_Obj* obj;
        int ok = Tcl_ListObjIndex(interp_, new_list_obj, 1, &obj);
        ASSERT(ok == TCL_OK);
        
        Tcl_ListObjAppendElement(interp_, list_obj_, obj);
        Tcl_IncrRefCount(new_list_obj);
        Tcl_DecrRefCount(new_list_obj); // free it
    }
}


} // namespace oasys
