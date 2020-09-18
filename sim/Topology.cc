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
#  include <dtn-config.h>
#endif

#include "Topology.h"
#include "Node.h"

namespace dtnsim {

Topology::NodeTable Topology::nodes_;

Node*
Topology::create_node(const char* name)
{
    Node* node = new Node(name);
    node->do_init();
    nodes_[name] = node;
    return node;
}

Node*
Topology::find_node(const char* name)
{
    NodeTable::iterator iter = nodes_.find(name);
    if (iter == nodes_.end())
        return NULL;

    return (*iter).second;
}
} // namespace dtnsim
