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

#include "DebugUtils.h"
#include "Formatter.h"
#include "DebugDumpBuf.h"
#include "../serialize/DebugSerialize.h"

//----------------------------------------------------------------------------
void
oasys::Breaker::break_here()
{
    oasys_break();
}

//----------------------------------------------------------------------------
void 
oasys_break() 
{}

//----------------------------------------------------------------------------
const char*
oasys_dump(const void* obj)
{
    const oasys::Formatter* fobj = 
        reinterpret_cast<const oasys::Formatter*>(obj);

#ifndef NDEBUG
    if (fobj->format_magic_ != FORMAT_MAGIC) 
    {
        return "Pointer doesn't point to Formatter";
    }
#endif // NDEBUG
    
    fobj->format(oasys::DebugDumpBuf::buf_, oasys::DebugDumpBuf::size_);
    return oasys::DebugDumpBuf::buf_;
}

//----------------------------------------------------------------------------
const char* 
oasys_sdump(const void* obj)
{
    oasys::DebugSerialize s(oasys::Serialize::CONTEXT_LOCAL,
                            oasys::DebugDumpBuf::buf_, 
                            oasys::DebugDumpBuf::size_);
    s.action((oasys::SerializableObject*)obj);
    return oasys::DebugDumpBuf::buf_;
}

//----------------------------------------------------------------------------
const char* 
oasys_sdumpn(const void* obj)
{
    oasys::DebugSerialize s(oasys::Serialize::CONTEXT_NETWORK,
                            oasys::DebugDumpBuf::buf_, 
                            oasys::DebugDumpBuf::size_);
    s.action((oasys::SerializableObject*)obj);
    return oasys::DebugDumpBuf::buf_;
}
