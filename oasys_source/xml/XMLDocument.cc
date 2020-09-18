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

#include "XMLDocument.h"
#include "XMLObject.h"
#include "util/StringBuffer.h"

namespace oasys {

//----------------------------------------------------------------------
XMLDocument::XMLDocument()
    : root_(NULL)
{
}

//----------------------------------------------------------------------
XMLDocument::~XMLDocument()
{
    delete root_;
}

//----------------------------------------------------------------------
void
XMLDocument::add_header(const char* header, size_t len)
{
    if (len == 0) {
        len = strlen(header);
    }
    
    header_.append(header, len);
}

//----------------------------------------------------------------------
void
XMLDocument::set_root(XMLObject* root)
{
    ASSERT(root_ == NULL);
    root_ = root;
}

//----------------------------------------------------------------------
void
XMLDocument::to_string(StringBuffer* buf, int indent) const
{
    if (header_.length() != 0) {
        buf->append(header_.c_str(), header_.length());
        buf->append("\n");
    }
    
    root_->to_string(buf, indent);
}

} // namespace oasys
