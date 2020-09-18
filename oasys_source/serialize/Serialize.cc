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

#include "Serialize.h"
#include "debug/DebugUtils.h"

namespace oasys {

//----------------------------------------------------------------------
SerializeAction::SerializeAction(action_t  action, 
                                 context_t context, 
                                 int       options)
    : action_(action), 
      context_(context), 
      options_(options), 
      log_(0), 
      error_(false)
{
}

//----------------------------------------------------------------------
SerializeAction::~SerializeAction()
{
}

//----------------------------------------------------------------------
int
SerializeAction::action(SerializableObject* object)
{
    error_ = false;

    begin_action();
    object->serialize(this);
    end_action();
    
    if (error_ == true)
        return -1;
    
    return 0;
}


//----------------------------------------------------------------------
void
SerializeAction::begin_action()
{
}

//----------------------------------------------------------------------
void
SerializeAction::end_action()
{
}

//----------------------------------------------------------------------
void 
SerializeAction::process(const char* name, SerializableObject* object)
{
    (void)name;
    object->serialize(this);
}

//----------------------------------------------------------------------
void
SerializeAction::process(const char* name, int64_t* i)
{
    process(name, (u_int64_t*)i);
}

//----------------------------------------------------------------------------    
void
SerializeAction::process(const char* name, int32_t* i)
{
    process(name, (u_int32_t*)i);
}

//----------------------------------------------------------------------------    
void
SerializeAction::process(const char* name, int16_t* i)
{
    process(name, (u_int16_t*)i);
}

//----------------------------------------------------------------------------    
void
SerializeAction::process(const char* name, int8_t* i)
{
    process(name, (u_int8_t*)i);
}

//----------------------------------------------------------------------------    
void
SerializeAction::process(const char* name, char* bp, u_int32_t len)
{
    process(name, (u_char*)bp, len);
}

//----------------------------------------------------------------------
#ifdef __CYGWIN__
void
SerializeAction::process(const char* name, int* i)
{
    process(name, (int32_t*)i);
}
#endif

//----------------------------------------------------------------------------    
void 
SerializeAction::process(const char*          name, 
                         BufferCarrier<char>* carrier)
{
    BufferCarrier<u_char> uc;
    BufferCarrier<u_char>::convert(&uc, *carrier);
    process(name, &uc);
    BufferCarrier<char>::convert(carrier, uc);

    // Clear the temporary
    uc.reset();
}

//----------------------------------------------------------------------------    
void 
SerializeAction::process(const char*          name,
                         BufferCarrier<char>* carrier,
                         char                 terminator)
{
    BufferCarrier<u_char> uc;
    BufferCarrier<u_char>::convert(&uc, *carrier);
    process(name, &uc, static_cast<u_char>(terminator));
    BufferCarrier<char>::convert(carrier, uc);

    uc.reset();
}

//----------------------------------------------------------------------
void 
SerializeAction::process(const char* name, const InAddrPtr& a)
{
#ifdef __CYGWIN__
    process(name, (u_int32_t*)(a.addr()));
#else
    process(name, static_cast<u_int32_t*>(a.addr()));
#endif
}

//----------------------------------------------------------------------
Builder Builder::static_builder_;

} // namespace oasys
