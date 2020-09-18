/*
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You
 * may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 */

/*
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "BP_Tag.h"

namespace dtn {

BP_Tag::BP_Tag()
    : type_(BP_TAG_NONE)
{
}

BP_Tag::BP_Tag(int type)
    : type_(type)
{
}

BP_Tag::~BP_Tag()
{
}

BP_TagVec::const_iterator
BP_TagVec::find_tag(int type, BP_TagVec::const_iterator iter) const
{
    for ( ; iter != end(); ++iter)
    {
        if (iter->type() == type)
            break;
    }

    return iter;
}

BP_TagVec::const_iterator
BP_TagVec::find_tag(int type) const
{
    return find_tag(type, begin());
}

bool
BP_TagVec::has_tag(int type) const
{
    BP_TagVec::const_iterator iter;
    
    for (iter = begin(); iter != end(); ++iter)
    {
        if (iter->type() == type)
            return true;  // found
    }

    // not found
    return false;
}

} // namespace dtn

#endif /* BSP_ENABLED */
