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

#ifndef _TOPOLOGY_H_
#define _TOPOLOGY_H_


#include <vector>
#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>
#include <oasys/util/StringUtils.h>

namespace dtnsim {

class Node;


/**
 * The class that maintains the topology of the network
 */
class Topology  {

public:
    static Node* create_node(const char* name);
    static Node* find_node(const char* name);

    typedef oasys::StringHashMap<Node*> NodeTable;

    static NodeTable* node_table() { return &nodes_; }

protected:
    static NodeTable nodes_;    
    static const int MAX_NODES  = 100;
        
};
} // namespace dtnsim

#endif /* _TOPOLOGY_H_ */
