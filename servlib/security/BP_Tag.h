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

#ifndef _BP_TAG_H_
#define _BP_TAG_H_

#ifdef BSP_ENABLED

#include <vector>
#include <oasys/debug/DebugUtils.h>  // ASSERT()

namespace dtn {

/**
 * Tag objects are used to hold auxiliary information pertaining to
 * the completion of various operations, e.g., security processing.
 */
class BP_Tag {

public:
    BP_Tag();

    BP_Tag(int type);

    virtual ~BP_Tag();

    int type() const {
        ASSERT(type_ != BP_TAG_NONE);
        return type_;
    }

    /**
     * Tag values.  The high byte should be the block type to which
     * the tag relates, or 0xff if it relates to none.
     */
    enum {
        BP_TAG_NONE             = 0xffff,
        BP_TAG_E2E_SEC_OUT_DONE = 0xff01,
        BP_TAG_BAB_IN_DONE      = 0x0201,
        BP_TAG_PIB_IN_DONE      = 0x0301,
        BP_TAG_PCB_IN_DONE       = 0x0401,
    };

private:
    int type_;
};

/**
 * Vector of BP_Tag, except with the usual vector operations hidden;
 * accessible only via find_tag() and append_tag().
 */
class BP_TagVec : public std::vector<BP_Tag> {

public:
    
    /**
     * Get an iterator pointing to the next tag of the given type, or
     * end() if there is none, beginning at location represented by
     * the provided iterator.
     */
    BP_TagVec::const_iterator find_tag(int type,
                                       BP_TagVec::const_iterator iter) const;
    
    /**
     * Get an iterator to the first tag of the given type, or end() if
     * there is none.
     */
    BP_TagVec::const_iterator find_tag(int type) const;
    
    /**
     * Determine whether at least one tag of the given type exists in
     * this list.
     */
    bool has_tag(int type) const;
    
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _BP_TAG_H_ */
